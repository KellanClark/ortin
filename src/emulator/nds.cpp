
#include "emulator/nds.hpp"

#include "emulator/dma.hpp"
#include "emulator/timer.hpp"
#include "arm946e/arm946e.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "emulator/nds7/rtc.hpp"
#include "emulator/nds7/spi.hpp"
#include "emulator/nds7/apu.hpp"

NDS::NDS() {
	shared = std::make_shared<BusShared>();
	ppu = std::make_shared<PPU>(shared);
	gamecard = std::make_shared<Gamecard>(shared);
	ipc = std::make_shared<IPC>(shared);
	nds9 = std::make_shared<BusARM9>(shared, ipc, ppu, gamecard);
	nds7 = std::make_shared<BusARM7>(shared, ipc, ppu, gamecard);

	romInfo.romLoaded = romInfo.bios9Loaded = romInfo.bios7Loaded = romInfo.firmwareLoaded = false;
	running = false;
	stepArm9 = stepArm7 = 0;

	disassembler9.defaultSettings();
	disassembler7.defaultSettings();
}

NDS::~NDS() {
	//
}

void NDS::reset() {
	running = false;

	shared->reset();
	ipc.reset();
	ppu->reset();
	gamecard->reset();
	nds9->reset();
	nds7->reset();

	nds9->refreshWramPages();
	nds7->refreshWramPages();
	nds9->refreshVramPages();

	//directBoot();

	nds9->delay = 0;
	nds7->delay = 0;
}

void NDS::directBoot() {
	// Copy entry points into memory
	for (int i = 0; i < romInfo.arm9CopySize; i++)
		nds9->write<u8>(romInfo.arm9CopyDestination + i, romMap[romInfo.arm9RomOffset + i], false);
	nds9->cpu->reg.R[15] = romInfo.arm9EntryPoint;
	nds9->cpu->flushPipeline();

	for (int i = 0; i < romInfo.arm7CopySize; i++)
		nds7->write<u8>(romInfo.arm7CopyDestination + i, romMap[romInfo.arm7RomOffset + i], false);
	nds7->cpu->reg.R[15] = romInfo.arm7EntryPoint;
	nds7->cpu->flushPipeline();

	// Register values after boot
	nds9->cpu->reg.R[12] = nds9->cpu->reg.R[14] = romInfo.arm9EntryPoint;
	nds9->cpu->reg.R[13] = 0x3002F7C;
	nds9->cpu->reg.R13_irq = 0x3003F80;
	nds9->cpu->reg.R13_svc = 0x3003FC0;
	nds9->coprocessorWrite(15, 0, 9, 1, 0, 0x0080000A);
	nds9->coprocessorWrite(15, 0, 9, 1, 1, 0x0000000C);
	nds7->cpu->reg.R[12] = nds7->cpu->reg.R[14] = romInfo.arm7EntryPoint;
	nds7->cpu->reg.R[13] = 0x300FD80;
	nds7->cpu->reg.R13_irq = 0x300FF80;
	nds7->cpu->reg.R13_svc = 0x300FFC0;

	// User Settings (normally loaded from firmware)
	nds9->write<u16>(0x27FFC80 + 0x58, 0x0000, false); // Touch-screen calibration point (adc.x1,y1) 12bit ADC-position
	nds9->write<u16>(0x27FFC80 + 0x5A, 0x0000, false);
	nds9->write<u16>(0x27FFC80 + 0x5C, 0x00'00, false); // Touch-screen calibration point (scr.x1,y1) 8bit pixel-position
	nds9->write<u16>(0x27FFC80 + 0x5E, 0x0FF0, false); // Touch-screen calibration point (adc.x2,y2) 12bit ADC-position
	nds9->write<u16>(0x27FFC80 + 0x60, 0x0BF0, false);
	nds9->write<u16>(0x27FFC80 + 0x62, 0xBF'FF, false); // Touch-screen calibration point (scr.x2,y2) 8bit pixel-position

	// Cartridge State
	gamecard->encryptionMode = Gamecard::KEY2;

	// IO registers
	nds9->POSTFLG = 1;
	nds7->POSTFLG = 1;
}

void NDS::run() {
	u64 nds9timestamp = 0;
	u64 nds7timestamp = 0;
	while (true) {
		while (running) { [[likely]]
			if (nds9timestamp <= shared->currentTime) {
				/*if (nds9->cpu->reg.thumbMode) {
					std::string disasm = disassembler9.disassemble(nds9->cpu->reg.R[15] - 4, nds9->cpu->pipelineOpcode3, true);
					shared->log << fmt::format("0x{:0>7X} |     0x{:0>4X} | {}\n", nds9->cpu->reg.R[15] - 4, nds9->cpu->pipelineOpcode3, disasm);
				} else {
					std::string disasm = disassembler9.disassemble(nds9->cpu->reg.R[15] - 8, nds9->cpu->pipelineOpcode3, false);
					shared->log << fmt::format("0x{:0>7X} | 0x{:0>8X} | {}\n", nds9->cpu->reg.R[15] - 8, nds9->cpu->pipelineOpcode3, disasm);
				}*/

				nds9->delay = 0;
				nds9->cpu->cycle();
				nds9->delay = 1;
				nds9timestamp = shared->currentTime + nds9->delay;

				if (stepArm9 && !nds9->cpu->cp15.halted) [[unlikely]]
					running = stepArm9 = false;
			}
			if ((nds7timestamp <= shared->currentTime) && !nds7->HALTCNT) {
				/*if (nds7->cpu->reg.thumbMode) {
					std::string disasm = disassembler7.disassemble(nds7->cpu->reg.R[15] - 4, nds7->cpu->pipelineOpcode3, true);
					shared->log << fmt::format("0x{:0>7X} |     0x{:0>4X} | {}\n", nds7->cpu->reg.R[15] - 4, nds7->cpu->pipelineOpcode3, disasm);
				} else {
					std::string disasm = disassembler7.disassemble(nds7->cpu->reg.R[15] - 8, nds7->cpu->pipelineOpcode3, false);
					shared->log << fmt::format("0x{:0>7X} | 0x{:0>8X} | {}\n", nds7->cpu->reg.R[15] - 8, nds7->cpu->pipelineOpcode3, disasm);
				}*/

				nds7->delay = 0;
				nds7->cpu->cycle();
				nds7timestamp = shared->currentTime + nds7->delay;

				if (stepArm7) [[unlikely]]
					running = stepArm7 = false;
			}

			while (shared->eventQueue.top().timeStamp <= shared->currentTime) {
				auto type = shared->eventQueue.top().type;
				shared->eventQueue.pop();

				switch (type) {
				default:
					shared->log << "Invalid event " << shared->eventQueue.top().type;
				case EventType::STOP:
					running = false;
					break;
				case IPC_SYNC9: nds9->requestInterrupt(BusARM9::INT_IPC_SYNC); break;
				case IPC_SYNC7: nds7->requestInterrupt(BusARM7::INT_IPC_SYNC); break;
				case IPC_SEND_FIFO9: nds9->requestInterrupt(BusARM9::INT_IPC_SEND_FIFO); break;
				case IPC_SEND_FIFO7: nds7->requestInterrupt(BusARM7::INT_IPC_SEND_FIFO); break;
				case IPC_RECV_FIFO9: nds9->requestInterrupt(BusARM9::INT_IPC_RECV_FIFO); break;
				case IPC_RECV_FIFO7: nds7->requestInterrupt(BusARM7::INT_IPC_RECV_FIFO); break;
				case PPU_LINE_START:
					ppu->lineStart();

					if (ppu->vBlankIrq9) { nds9->requestInterrupt(BusARM9::INT_VBLANK); ppu->vBlankIrq9 = false; }
					if (ppu->vBlankIrq7) { nds7->requestInterrupt(BusARM7::INT_VBLANK); ppu->vBlankIrq7 = false; }
					if (ppu->vCounterIrq9) { nds9->requestInterrupt(BusARM9::INT_VCOUNT); ppu->vCounterIrq9 = false; }
					if (ppu->vCounterIrq7) { nds7->requestInterrupt(BusARM7::INT_VCOUNT); ppu->vCounterIrq7 = false; }

					if (ppu->currentScanline == 192) {
						nds9->dma->checkDma(DMA_VBLANK);
						nds7->dma->checkDma(DMA_VBLANK);
					}

					if (ppu->currentScanline == 0)
						handleThreadQueue();
					break;
				case PPU_HBLANK:
					ppu->hBlank();

					if (ppu->hBlankIrq9) { nds9->requestInterrupt(BusARM9::INT_HBLANK); ppu->hBlankIrq9 = false; }
					if (ppu->hBlankIrq7) { nds7->requestInterrupt(BusARM7::INT_HBLANK); ppu->hBlankIrq7 = false; }

					if (ppu->currentScanline < 192)
						nds9->dma->checkDma(DMA_HBLANK);
					break;
				case REFRESH_WRAM_PAGES:
					nds9->refreshWramPages();
					nds7->refreshWramPages();
					break;
				case REFRESH_VRAM_PAGES:
					ppu->refreshVramPages();
					nds9->refreshVramPages();
					nds7->refreshVramPages();
					break;
				case REFRESH_ROM_PAGES:
					nds9->refreshRomPages();
					nds7->refreshRomPages();
					break;
				case SPI_FINISHED: nds7->requestInterrupt(BusARM7::INT_SPI); break;
				case RTC_REFRESH: nds7->rtc->refresh<true>(); break;
				case SERIAL_INTERRUPT: nds7->requestInterrupt(BusARM7::INT_SERIAL); break;
				case TIMER_OVERFLOW_9:
					nds9->timer->checkOverflow();
					if (nds9->timer->timer[0].interruptRequested) { nds9->timer->timer[0].interruptRequested = false; nds9->requestInterrupt(BusARM9::INT_TIMER_0); }
					if (nds9->timer->timer[1].interruptRequested) { nds9->timer->timer[1].interruptRequested = false; nds9->requestInterrupt(BusARM9::INT_TIMER_1); }
					if (nds9->timer->timer[2].interruptRequested) { nds9->timer->timer[2].interruptRequested = false; nds9->requestInterrupt(BusARM9::INT_TIMER_2); }
					if (nds9->timer->timer[3].interruptRequested) { nds9->timer->timer[3].interruptRequested = false; nds9->requestInterrupt(BusARM9::INT_TIMER_3); }
					break;
				case TIMER_OVERFLOW_7:
					nds7->timer->checkOverflow();
					if (nds7->timer->timer[0].interruptRequested) { nds7->timer->timer[0].interruptRequested = false; nds7->requestInterrupt(BusARM7::INT_TIMER_0); }
					if (nds7->timer->timer[1].interruptRequested) { nds7->timer->timer[1].interruptRequested = false; nds7->requestInterrupt(BusARM7::INT_TIMER_1); }
					if (nds7->timer->timer[2].interruptRequested) { nds7->timer->timer[2].interruptRequested = false; nds7->requestInterrupt(BusARM7::INT_TIMER_2); }
					if (nds7->timer->timer[3].interruptRequested) { nds7->timer->timer[3].interruptRequested = false; nds7->requestInterrupt(BusARM7::INT_TIMER_3); }
					break;
				case GAMECARD_TRANSFER_READY:
					if (shared->ndsSlotAccess) {
						nds7->requestInterrupt(BusARM7::INT_NDS_SLOT_DATA);
					} else {
						nds9->requestInterrupt(BusARM9::INT_NDS_SLOT_DATA);
					}
					//nds7->requestInterrupt(BusARM7::INT_NDS_SLOT_DATA);
					//nds9->requestInterrupt(BusARM9::INT_NDS_SLOT_DATA);
					break;
				}
			}

			shared->currentTime += std::min((u64)std::min((nds7->HALTCNT == 0x80) ? LLONG_MAX : nds7->delay, (nds9->cpu->cp15.halted && !nds9->cpu->processIrq) ? LLONG_MAX : nds9->delay), shared->eventQueue.top().timeStamp - shared->currentTime);
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
			if (romInfo.romLoaded && romInfo.bios9Loaded && romInfo.bios7Loaded && romInfo.firmwareLoaded) {
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
			stepArm9 = true;
			//shared->addEvent(nds9->delay - 1, EventType::STOP);
			break;
		case STEP_ARM7:
			running = true;
			stepArm7 = true;
			//shared->addEvent(nds7->delay - 1, EventType::STOP);
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
		case LOAD_FIRMWARE:
			if (loadFirmware(*(std::filesystem::path *)currentEvent.ptrArg)) {
				threadQueue = {};
			} else {
				romInfo.firmwareLoaded = true;
			}
			break;
		case CLEAR_LOG:
			shared->log.str("");
			break;
		case UPDATE_KEYS:
			shared->KEYINPUT = ~currentEvent.intArg & 0x03FF;
			shared->EXTKEYIN = ((~currentEvent.intArg >> 10) & 0x0043) | 0x003C;
			break;
		case SET_TIME: {
			auto tt = time(0);
			nds7->rtc->syncToRealTime(&tt);
			} break;
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
		shared->log << fmt::format("Failed to load ROM : {}\n", romFilePath.string(), error.message());
		return error.value();
	}

	gamecard->romData = romMap.data();
	gamecard->romSize = romMap.mapped_length();
	gamecard->romSizeMax = std::bit_ceil(gamecard->romSize);

	// Make a chip ID since I haven't found a database
	auto romSize = std::min(gamecard->romSizeMax, (size_t)0x100000);
	gamecard->manufacturer = 0xC2; // Macronix
	if (romSize <= 128 * 1024 * 1024) { // Between 1MB and 128MB
		gamecard->cartSize = (romSize >> 20) - 1; // > 00h..7Fh: (N+1)Mbytes
	} else {
		gamecard->cartSize = 0x100 - (romSize >> 28); // > F0h..FFh: (100h-N)*256Mbytes?
	}
	gamecard->cartFlags1 = 0x00;
	gamecard->cartFlags2 = 0x00;

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
	shared->log << fmt::format("Loaded ROM {}\n", romFilePath.string());
	return 0;
}

int NDS::loadBios9(std::filesystem::path bios9FilePath) {
	std::error_code error;

	bios9Map.map(bios9FilePath.c_str(), error);
	if (error) {
		shared->log << fmt::format("Failed to load NDS9 BIOS: {}\n", bios9FilePath.string(), error.message());
		return error.value();
	}
	romInfo.bios9FilePath = bios9FilePath;

	// Copy file into memory
	memset(nds9->bios, 0, 0x8000);
	for (int i = 0; i < std::min((u32)bios9Map.size(), (u32)0x8000); i++)
		nds9->bios[i] = bios9Map[i];

	shared->log << fmt::format("Loaded NDS9 BIOS {}\n", bios9FilePath.string());
	return 0;
}

int NDS::loadBios7(std::filesystem::path bios7FilePath) {
	std::error_code error;

	bios7Map.map(bios7FilePath.c_str(), error);
	if (error) {
		shared->log << fmt::format("Failed to load NDS7 BIOS: {}\n", bios7FilePath.string(), error.message());
		return error.value();
	}
	romInfo.bios7FilePath = bios7FilePath;

	// Copy file into memory
	memset(nds7->bios, 0, 0x4000);
	for (int i = 0; i < std::min((u32)bios7Map.size(), (u32)0x4000); i++)
		nds7->bios[i] = bios7Map[i];

	// The BIOS has a table of values used in KEY1 encryption
	memcpy(gamecard->level2.keyBuf, &nds7->bios[0x30], 0x1048);
	memcpy(gamecard->level3.keyBuf, &nds7->bios[0x30], 0x1048);

	shared->log << fmt::format("Loaded NDS7 BIOS {}\n", bios7FilePath.string());
	return 0;
}

int NDS::loadFirmware(std::filesystem::path firmwareFilePath) {
	std::error_code error;

	firmwareMap.map(firmwareFilePath.c_str(), error);
	if (error) {
		std::cout << fmt::format("Failed to load Firmware file: {}\n", firmwareFilePath.string(), error.message());
		return error.value();
	}

	nds7->spi->firmware.data = firmwareMap.data();
	romInfo.firmwareFilePath = firmwareFilePath;
	shared->log << fmt::format("Loaded Firmware file: {}\n", firmwareFilePath.string());
	return 0;
}