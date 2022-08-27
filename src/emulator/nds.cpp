
#include "emulator/nds.hpp"

NDS::NDS() : shared(log), ipc(shared, log), ppu(shared, log), nds9(shared, log, ipc, ppu), nds7(shared, ipc, ppu, log) {
	romInfo.romLoaded = romInfo.bios9Loaded = romInfo.bios7Loaded = false;

	disassembler9.defaultSettings();
	disassembler7.defaultSettings();
}

NDS::~NDS() {
	//
}

void NDS::reset() {
	running = false;

	shared.reset();
	ipc.reset();
	ppu.reset();
	nds9.reset();
	nds7.reset();

	nds9.refreshWramPages();
	nds7.refreshWramPages();
	nds9.refreshVramPages();

	// Copy entry points into memory
	for (int i = 0; i < romInfo.arm9CopySize; i++)
		nds9.write<u8>(romInfo.arm9CopyDestination + i, romMap[romInfo.arm9RomOffset + i], false);
	nds9.cpu.reg.R[15] = romInfo.arm9EntryPoint;
	nds9.cpu.flushPipeline();

	for (int i = 0; i < romInfo.arm7CopySize; i++)
		nds7.write<u8>(romInfo.arm7CopyDestination + i, romMap[romInfo.arm7RomOffset + i], false);
	nds7.cpu.reg.R[15] = romInfo.arm7EntryPoint;
	nds7.cpu.flushPipeline();

	// Register values after boot
	nds9.cpu.reg.R[12] = nds9.cpu.reg.R[14] = romInfo.arm9EntryPoint;
	nds9.cpu.reg.R[13] = 0x3002F7C;
	nds9.cpu.reg.R13_irq = 0x3003F80;
	nds9.cpu.reg.R13_svc = 0x3003FC0;
	nds9.coprocessorWrite(15, 0, 9, 1, 0, 0x0080000A);
	nds9.coprocessorWrite(15, 0, 9, 1, 1, 0x0000000C);
	nds7.cpu.reg.R[12] = nds7.cpu.reg.R[14] = romInfo.arm7EntryPoint;
	nds7.cpu.reg.R[13] = 0x300FD80;
	nds7.cpu.reg.R13_irq = 0x300FF80;
	nds7.cpu.reg.R13_svc = 0x300FFC0;
}

void NDS::run() {
	while (true) {
		while (running) { [[likely]]
			if (--nds9.delay <= 0) {
				/*if (nds9.cpu.reg.thumbMode) {
					std::string disasm = disassembler9.disassemble(nds9.cpu.reg.R[15] - 4, nds9.cpu.pipelineOpcode3, true);
					log << fmt::format("0x{:0>7X} |     0x{:0>4X} | {}\n", nds9.cpu.reg.R[15] - 4, nds9.cpu.pipelineOpcode3, disasm);
				} else {
					std::string disasm = disassembler9.disassemble(nds9.cpu.reg.R[15] - 8, nds9.cpu.pipelineOpcode3, false);
					log << fmt::format("0x{:0>7X} | 0x{:0>8X} | {}\n", nds9.cpu.reg.R[15] - 8, nds9.cpu.pipelineOpcode3, disasm);
				}*/

				nds9.cpu.cycle();
				nds9.delay = 1;
			}
			if (--nds7.delay <= 0) {
				/*if (nds7.cpu.reg.thumbMode) {
					std::string disasm = disassembler7.disassemble(nds7.cpu.reg.R[15] - 4, nds7.cpu.pipelineOpcode3, true);
					log << fmt::format("0x{:0>7X} |     0x{:0>4X} | {}\n", nds7.cpu.reg.R[15] - 4, nds7.cpu.pipelineOpcode3, disasm);
				} else {
					std::string disasm = disassembler7.disassemble(nds7.cpu.reg.R[15] - 8, nds7.cpu.pipelineOpcode3, false);
					log << fmt::format("0x{:0>7X} | 0x{:0>8X} | {}\n", nds7.cpu.reg.R[15] - 8, nds7.cpu.pipelineOpcode3, disasm);
				}*/

				nds7.cpu.cycle();
			}

			while (shared.eventQueue.top().timeStamp <= shared.currentTime) {
				auto type = shared.eventQueue.top().type;
				shared.eventQueue.pop();

				switch (type) {
				default:
					log << "Invalid event " << shared.eventQueue.top().type;
				case EventType::STOP:
					running = false;
					break;
				case IPC_SYNC9: nds9.requestInterrupt(BusARM9::INT_IPC_SYNC); break;
				case IPC_SYNC7: nds7.requestInterrupt(BusARM7::INT_IPC_SYNC); break;
				case IPC_SEND_FIFO9: nds9.requestInterrupt(BusARM9::INT_IPC_SEND_FIFO); break;
				case IPC_SEND_FIFO7: nds7.requestInterrupt(BusARM7::INT_IPC_SEND_FIFO); break;
				case IPC_RECV_FIFO9: nds9.requestInterrupt(BusARM9::INT_IPC_RECV_FIFO); break;
				case IPC_RECV_FIFO7: nds7.requestInterrupt(BusARM7::INT_IPC_RECV_FIFO); break;
				case EventType::PPU_LINE_START:
					ppu.lineStart();

					if (ppu.vBlankIrq9) { nds9.requestInterrupt(BusARM9::INT_VBLANK); ppu.vBlankIrq9 = false; }
					if (ppu.vBlankIrq7) { nds7.requestInterrupt(BusARM7::INT_VBLANK); ppu.vBlankIrq7 = false; }
					if (ppu.vCounterIrq9) { nds9.requestInterrupt(BusARM9::INT_VCOUNT); ppu.vCounterIrq9 = false; }
					if (ppu.vCounterIrq7) { nds7.requestInterrupt(BusARM7::INT_VCOUNT); ppu.vCounterIrq7 = false; }

					if (ppu.currentScanline == 192) {
						nds9.dma.checkDma(DMA_VBLANK);
						nds7.dma.checkDma(DMA_VBLANK);
					}

					if (ppu.currentScanline == 0)
						handleThreadQueue();
					break;
				case EventType::PPU_HBLANK:
					ppu.hBlank();

					if (ppu.hBlankIrq9) { nds9.requestInterrupt(BusARM9::INT_HBLANK); ppu.hBlankIrq9 = false; }
					if (ppu.hBlankIrq7) { nds7.requestInterrupt(BusARM7::INT_HBLANK); ppu.hBlankIrq7 = false; }

					if (ppu.currentScanline < 192)
						nds9.dma.checkDma(DMA_HBLANK);
					break;
				case EventType::REFRESH_WRAM_PAGES:
					nds9.refreshWramPages();
					nds7.refreshWramPages();
					break;
				case EventType::REFRESH_VRAM_PAGES:
					nds9.refreshVramPages();
					break;
				}
			}

			++shared.currentTime;
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
			if (romInfo.romLoaded && romInfo.bios9Loaded && romInfo.bios7Loaded) {
				running = true;
			} else {
				threadQueue = {};
			}
			break;
		case STOP:
			running = false;
			break;
		case RESET:
			reset();
			break;
		case STEP_ARM9:
			running = true;
			shared.addEvent(nds9.delay - 1, EventType::STOP);
			break;
		case STEP_ARM7:
			running = true;
			shared.addEvent(nds7.delay - 1, EventType::STOP);
			break;
		case LOAD_ROM:
			if (loadRom(*(std::filesystem::path *)currentEvent.ptrArg)) {
				threadQueue = {};
			} else {
				romInfo.romLoaded = true;
			}
			break;
		case LOAD_BIOS9:
			if (loadBios9(*(std::filesystem::path *)currentEvent.ptrArg)) {
				threadQueue = {};
			} else {
				romInfo.bios9Loaded = true;
			}
			break;
		case LOAD_BIOS7:
			if (loadBios7(*(std::filesystem::path *)currentEvent.ptrArg)) {
				threadQueue = {};
			} else {
				romInfo.bios7Loaded = true;
			}
			break;
		case CLEAR_LOG:
			log.str("");
			break;
		case UPDATE_KEYS:
			shared.KEYINPUT = ~currentEvent.intArg & 0x03FF;
			shared.EXTKEYIN = ((~currentEvent.intArg >> 10) & 0x0003) | 0x007C;
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

int NDS::loadBios9(std::filesystem::path bios9FilePath) {
	std::error_code error;

	bios9Map.map(bios9FilePath.c_str(), error);
	if (error) {
		log << fmt::format("Failed to load NDS9 BIOS : {}\n", bios9FilePath.string(), error.message());
		return error.value();
	}
	romInfo.bios9FilePath = bios9FilePath;

	// Copy file into memory
	memset(nds9.bios, 0, 0x8000);
	for (int i = 0; i < std::min((u32)bios9Map.size(), (u32)0x8000); i++)
		nds9.bios[i] = bios9Map[i];

	log << fmt::format("Loaded NDS9 BIOS {}\n", bios9FilePath.string());
	return 0;
}

int NDS::loadBios7(std::filesystem::path bios7FilePath) {
	std::error_code error;

	bios7Map.map(bios7FilePath.c_str(), error);
	if (error) {
		log << fmt::format("Failed to load NDS7 BIOS : {}\n", bios7FilePath.string(), error.message());
		return error.value();
	}
	romInfo.bios7FilePath = bios7FilePath;

	// Copy file into memory
	memset(nds7.bios, 0, 0x4000);
	for (int i = 0; i < std::min((u32)bios7Map.size(), (u32)0x4000); i++)
		nds7.bios[i] = bios7Map[i];

	log << fmt::format("Loaded NDS7 BIOS {}\n", bios7FilePath.string());
	return 0;
}