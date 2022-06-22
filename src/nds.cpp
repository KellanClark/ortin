
#include "nds.hpp"

NDS::NDS() : ppu(shared, log), nds9(shared, ppu, log), nds7(shared, ppu, log) {
	//
}

NDS::~NDS() {
	//
}

void NDS::reset() {
	running = false;

	shared.reset();
	ppu.reset();
	nds9.reset();
	nds7.reset();

	// Copy entry points into memory
	for (int i = 0; i < romInfo.arm9CopySize; i++)
		nds9.write<u8>(romInfo.arm9CopyDestination + i, romMap[romInfo.arm9RomOffset + i], false);
	nds9.cpu.reg.R[15] = romInfo.arm9EntryPoint;
	nds9.cpu.flushPipeline();

	for (int i = 0; i < romInfo.arm7CopySize; i++)
		nds7.write<u8>(romInfo.arm7CopyDestination + i, romMap[romInfo.arm7RomOffset + i], false);
	nds7.cpu.reg.R[15] = romInfo.arm7EntryPoint;
	nds7.cpu.flushPipeline();
}

void NDS::run() {
	std::cout << "Hi from thread!" << std::endl;

	while (true) {
		while (running) { [[likely]]
			nds9.cpu.cycle();
			nds9.cpu.cycle();
			nds7.cpu.cycle();

			while (shared.eventQueue.top().timeStamp <= shared.currentTime) {
				auto type = shared.eventQueue.top().type;
				shared.eventQueue.pop();

				switch (type) {
				default:
					log << "Invalid event " << shared.eventQueue.top().type;
				case EventType::STOP:
					running = false;
					break;
				case EventType::PPU_LINE_START:
					ppu.lineStart();
					break;
				case EventType::PPU_HBLANK:
					ppu.hBlank();
					break;
				case EventType::REFRESH_VRAM_PAGES:
					nds9.refreshVramPages();
					break;
				}
			}

			shared.currentTime += 2;
		}

		handleThreadQueue();
	}
}

// Thread-safe queue
void NDS::handleThreadQueue() {
	threadQueueMutex.lock();
	while (!threadQueue.empty()) {
		threadEvent currentEvent = threadQueue.front();
		threadQueue.pop();

		switch (currentEvent.type) {
		case START:
			running = true;
			break;
		case STOP:
			running = false;
			break;
		case RESET:
			reset();
			break;
		/*case LOAD_BIOS:
			if (loadBios(*(std::filesystem::path *)currentEvent.ptrArg)) {
				threadQueue = {};
			}
			break;*/
		case LOAD_ROM:
			if (loadRom(*(std::filesystem::path *)currentEvent.ptrArg)) {
				threadQueue = {};
			}
			break;
		case CLEAR_LOG:
			nds9.log.str("");
			nds7.log.str("");
			break;
		default:
			printf("Unknown thread event:  %d\n", currentEvent.type);
			break;
		}
	}
	threadQueueMutex.unlock();
}

void NDS::addThreadEvent(threadEventType type) { addThreadEvent(type, 0, nullptr); }

void NDS::addThreadEvent(threadEventType type, u64 intArg) { addThreadEvent(type, intArg, nullptr); }

void NDS::addThreadEvent(threadEventType type, void *ptrArg) { addThreadEvent(type, 0, ptrArg); }

void NDS::addThreadEvent(threadEventType type, u64 intArg, void *ptrArg) {
	threadQueueMutex.lock();
	threadQueue.push(NDS::threadEvent{type, intArg, ptrArg});
	threadQueueMutex.unlock();
}

int NDS::loadRom(std::filesystem::path romFilePath) {
	std::error_code error;

	romMap.map(romFilePath.c_str(), error);
	if (error) {
		log << fmt::format("Failed to load ROM : {}\n", romFilePath.string(), error.message());
		return error.value();
	}

	// Fill ROM info struct
	char name[13] = {0};
	for (int i = 0; i < 12; i++)
		name[i] = romMap[i];
	romInfo.name = name;

	romInfo.version = romMap[0x01E];
	romInfo.autostart = romMap[0x01F] >> 2;

	memcpy(&romInfo.arm9RomOffset, &romMap[0x020], 4);
	memcpy(&romInfo.arm9EntryPoint, &romMap[0x024], 4);
	memcpy(&romInfo.arm9CopyDestination, &romMap[0x028], 4);
	memcpy(&romInfo.arm9CopySize, &romMap[0x02C], 4);

	memcpy(&romInfo.arm7RomOffset, &romMap[0x030], 4);
	memcpy(&romInfo.arm7EntryPoint, &romMap[0x034], 4);
	memcpy(&romInfo.arm7CopyDestination, &romMap[0x038], 4);
	memcpy(&romInfo.arm7CopySize, &romMap[0x03C], 4);

	romInfo.filePath = romFilePath;
	log << fmt::format("Loaded ROM {}\n", romFilePath.string());
	return 0;
}