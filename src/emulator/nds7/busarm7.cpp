#include "emulator/nds7/busarm7.hpp"

#include "emulator/ipc.hpp"
#include "emulator/ppu.hpp"
#include "emulator/cartridge/gamecard.hpp"

#include "arm7tdmi/arm7tdmi.hpp"
#include "emulator/dma.hpp"
#include "emulator/timer.hpp"
#include "emulator/nds7/rtc.hpp"
#include "emulator/nds7/spi.hpp"
#include "emulator/nds7/apu.hpp"
#include "emulator/nds7/wifi.hpp"

static constexpr u32 toPage(u32 address) {
	return address >> 14; // Pages are 16KB
}

static constexpr u32 toAddress(u32 page) {
	return page << 14;
}

BusARM7::BusARM7(std::shared_ptr<BusShared> shared, std::shared_ptr<IPC> ipc, std::shared_ptr<PPU> ppu, std::shared_ptr<Gamecard> gamecard) :
	shared(shared),
	log(shared->log),
	ipc(ipc),
	ppu(ppu),
	gamecard(gamecard) {
	cpu = std::make_unique<ARM7TDMI<BusARM7>>(*this);
	dma = std::make_unique<DMA<false>>(shared, *this);
	timer = std::make_unique<Timer>(false, shared);
	rtc = std::make_unique<RTC>(shared);
	spi = std::make_unique<SPI>(shared);
	apu = std::make_unique<APU>(shared, *this);
	wifi = std::make_unique<WiFi>(shared);

	wram = new u8[0x10000]; // 64KB
	memset(wram, 0, 0x10000);
	bios = new u8[0x4000]; // 16KB
	memset(bios, 0, 0x4000);

	POSTFLG = 0;

	// Fill page tables
	memset(&readTable, 0, sizeof(readTable));
	memset(&writeTable, 0, sizeof(writeTable));

	// PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)
	for (int i = toPage(0x2000000); i < toPage(0x3000000); i++) {
		readTable[i] = writeTable[i] = shared->psram + (((toAddress(i) - 0x2000000)) % 0x400000);
	}

	// ARM7 WRAM (64KB mirrored 0x3800000 - 0x4000000)
	for (int i = toPage(0x3800000); i < toPage(0x4000000); i++) {
		readTable[i] = writeTable[i] = wram + (((toAddress(i) - 0x3800000)) % 0x10000);
	}

	const int startingWaitstates[2][2][2][16] = {
		//  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
		{{{ 2,  2, 20,  2,  2,  2,  2,  2,  0,  0,  0,  2,  2,  2,  2,  2},   // Data Nonsequential 32
		  { 2,  2, 18,  2,  2,  2,  2,  2,  0,  0,  0,  2,  2,  2,  2,  2}},  // Data Nonsequential 16
		 {{ 2,  2,  4,  2,  2,  2,  4,  2,  0,  0,  0,  2,  2,  2,  2,  2},   // Data Sequential 32
		  { 2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  2,  2,  2,  2,  2}}}, // Data Sequential 16
		{{{ 2,  2, 18,  2,  2,  2,  4,  2,  0,  0,  0,  2,  2,  2,  2,  2},   // Code Nonsequential 32
		  { 2,  2, 16,  2,  2,  2,  2,  2,  0,  0,  0,  2,  2,  2,  2,  2}},  // Code Nonsequential 16
		 {{ 2,  2,  4,  2,  2,  2,  4,  2,  0,  0,  0,  2,  2,  2,  2,  2},   // Code Sequential 32
		  { 2,  2,  2,  2,  2,  2,  2,  2,  0,  0,  0,  2,  2,  2,  2,  2}}}  // Code Sequential 16
	};
	memcpy(waitstates, startingWaitstates, 2 * 2 * 2 * 16 * sizeof(int));
}

BusARM7::~BusARM7() {
	delete[] wram;
	delete[] bios;
}

void BusARM7::reset() {
	delay = 0;

	memset(wram, 0, 0x10000);

	IME = false;
	IE = IF = 0;
	HALTCNT = 0;

	refreshWramPages();
	refreshVramPages();
	refreshRomPages();

	dma->reset();
	timer->reset();
	rtc->reset();
	spi->reset();
	apu->reset();
	wifi->reset();
	cpu->resetARM7TDMI();
}

void BusARM7::requestInterrupt(InterruptType type) {
	IF |= type;

	refreshInterrupts();
}

void BusARM7::refreshInterrupts() {
	if (IE & IF)
		HALTCNT = 0;

	if (IME && (IE & IF)) {
		cpu->processIrq = true;
	} else {
		cpu->processIrq = false;
	}
}

void BusARM7::refreshWramPages() {
	// Set the first two pages in one table
	switch (shared->WRAMCNT) { // > ARM9/ARM7 (0-3 = 32K/0K, 2nd 16K/1st 16K, 1st 16K/2nd 16K, 0K/32K)
	case 0:
		readTable[toPage(0x3000000)] = wram + 0x0000;
		readTable[toPage(0x3004000)] = wram + 0x4000;
		readTable[toPage(0x3008000)] = wram + 0x8000;
		readTable[toPage(0x300C000)] = wram + 0xC000;
		break;
	case 1:
		readTable[toPage(0x3000000)] = shared->wram;
		readTable[toPage(0x3004000)] = shared->wram;
		readTable[toPage(0x3008000)] = shared->wram;
		readTable[toPage(0x300C000)] = shared->wram;
		break;
	case 2:
		readTable[toPage(0x3000000)] = shared->wram + 0x4000;
		readTable[toPage(0x3004000)] = shared->wram + 0x4000;
		readTable[toPage(0x3008000)] = shared->wram + 0x4000;
		readTable[toPage(0x300C000)] = shared->wram + 0x4000;
		break;
	case 3:
		readTable[toPage(0x3000000)] = shared->wram;
		readTable[toPage(0x3004000)] = shared->wram + 0x4000;
		readTable[toPage(0x3008000)] = shared->wram;
		readTable[toPage(0x300C000)] = shared->wram + 0x4000;
		break;
	}

	// Mirror it across the full 8MB in all tables
	for (int i = toPage(0x3000000); i < toPage(0x3800000); i += 4) {
		readTable[i] = writeTable[i] = readTable[toPage(0x3000000)];
		readTable[i + 1] = writeTable[i + 1] = readTable[toPage(0x3004000)];
		readTable[i + 2] = writeTable[i + 2] = readTable[toPage(0x3008000)];
		readTable[i + 3] = writeTable[i + 3] = readTable[toPage(0x300C000)];
	}
}

void BusARM7::refreshVramPages() {
	// Clear existing entries
	for (int i = toPage(0x6000000); i < toPage(0x6040000); i++)
		readTable[i] = nullptr;

	// Hopefully temporary
	if ((ppu->vramCMapped7 != ppu->vramDMapped7) || (ppu->vramCOffset != ppu->vramDOffset)) { // No overlap
		if (ppu->vramCMapped7) {
			if (ppu->vramCOffset) {
				for (int i = 0; i < toPage(0x20000); i++)
					readTable[toPage(0x6020000) + i] = ppu->vramC + toAddress(i);
			} else {
				for (int i = 0; i < toPage(0x20000); i++)
					readTable[toPage(0x6000000) + i] = ppu->vramC + toAddress(i);
			}
		}

		if (ppu->vramDMapped7) {
			if (ppu->vramDOffset) {
				for (int i = 0; i < toPage(0x20000); i++)
					readTable[toPage(0x6020000) + i] = ppu->vramD + toAddress(i);
			} else {
				for (int i = 0; i < toPage(0x20000); i++)
					readTable[toPage(0x6000000) + i] = ppu->vramD + toAddress(i);
			}
		}
	}

	// Mirror and copy to write table
	for (int i = toPage(0x6000000); i < toPage(0x7000000); i++)
		readTable[i] = writeTable[i] = readTable[i & toPage(0xF03FFFF)];
}

void BusARM7::refreshRomPages() {
	//
}

void BusARM7::hacf() {
	shared->addEvent(0, EventType::STOP);
}

template <typename T, bool code>
T BusARM7::read(u32 address, bool sequential) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 page = toPage(alignedAddress & 0x0FFFFFFF);
	u32 offset = alignedAddress & 0x3FFF;

	delay += waitstates[code][sequential][sizeof(T) == 4][(alignedAddress >> 24) & 0xF];
	//delay += 2;

	u8 *ptr = readTable[page];
	T val = 0;
	if ((address < 0x10000000) && (ptr != NULL)) { [[likely]]
		memcpy(&val, ptr + offset, sizeof(T));
	} else {
		switch (address) {
		case 0x0000000 ... 0x0004000: // ARM7-BIOS
			memcpy(&val, bios + alignedAddress, sizeof(T));
			break;
		case 0x4000000 ... 0x47FFFFF: // ARM7-I/O Ports
			if constexpr (sizeof(T) == 4) {
				val |= (readIO(alignedAddress | 0, false) << 0);
				val |= (readIO(alignedAddress | 1, false) << 8);
				val |= (readIO(alignedAddress | 2, false) << 16);
				val |= (readIO(alignedAddress | 3, true) << 24);
			} else if constexpr (sizeof(T) == 2) {
				val |= (readIO(alignedAddress | 0, false) << 0);
				val |= (readIO(alignedAddress | 1, true) << 8);
			} else {
				val = readIO(alignedAddress, true);
			}
			break;
		case 0x4800000 ... 0x4808FFF: // Wifi
			if constexpr (sizeof(T) == 2) {
				val = wifi->readIO7(alignedAddress);
			} else if constexpr (sizeof(T) == 4) {
				val = wifi->readIO7(alignedAddress) | (wifi->readIO7(alignedAddress + 2) << 16); // Split into 16-bit reads
			}
			// Byte reads aren't allowed
			break;
		case 0x6000000 ... 0x6FFFFFF: // Fallback for when VRAM banks overlap
			offset = alignedAddress & 0x1FFFF;

			if (ppu->vramCMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramCOffset & 1)))
				memcpy(&val, ppu->vramC + offset, sizeof(T));
			if (ppu->vramDMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramDOffset & 1)))
				memcpy(&val, ppu->vramD + offset, sizeof(T));

			if (!(ppu->vramCMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramCOffset & 1))) && !(ppu->vramDMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramDOffset & 1))))
				delay -= waitstates[code][sequential][sizeof(T) == 4][0x6] + 2;
			break;
		case 0x8000000 ... 0x9FFFFFF: // GBA Slot ROM (open bus for now)
			if (sizeof(T) == 4) {
				val = ((alignedAddress / 2) & 0xFFFF) | (((alignedAddress / 2) + 1) & 0xFFFF);
			} else {
				val = (T)(alignedAddress / 2);
			}
			break;
		default:
			delay -= waitstates[code][sequential][sizeof(T) == 4][(alignedAddress >> 24) & 0xF] + 2;

			shared->log << fmt::format("[NDS7 Bus] Read from unknown location 0x{:0>8X}\n", address);
			break;
		}
	}

	return val;
}
template u8 BusARM7::read<u8, false>(u32, bool);
template u16 BusARM7::read<u16, true>(u32, bool);
template u16 BusARM7::read<u16, false>(u32, bool);
template u32 BusARM7::read<u32, true>(u32, bool);
template u32 BusARM7::read<u32, false>(u32, bool);

template <typename T>
void BusARM7::write(u32 address, T value, bool sequential) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 page = toPage(alignedAddress & 0x0FFFFFFF);
	u32 offset = alignedAddress & 0x3FFF;

	delay += waitstates[0][sequential][sizeof(T) == 4][(alignedAddress >> 24) & 0xF];

	u8 *ptr = writeTable[page];
	if ((address < 0x10000000) && (ptr != NULL)) { [[likely]]
		memcpy(ptr + offset, &value, sizeof(T));
	} else {
		switch (address) {
		case 0x4000000 ... 0x47FFFFF: // I/O
			if constexpr (sizeof(T) == 4) {
				writeIO(alignedAddress | 0, (u8)(value >> 0), false);
				writeIO(alignedAddress | 1, (u8)(value >> 8), false);
				writeIO(alignedAddress | 2, (u8)(value >> 16), false);
				writeIO(alignedAddress | 3, (u8)(value >> 24), true);
			} else if constexpr (sizeof(T) == 2) {
				writeIO(alignedAddress | 0, (u8)(value >> 0), false);
				writeIO(alignedAddress | 1, (u8)(value >> 8), true);
			} else {
				writeIO(alignedAddress, value, true);
			}
			break;
		case 0x4800000 ... 0x4808FFF: // Wifi
			if constexpr (sizeof(T) == 2) {
				wifi->writeIO7(alignedAddress, value);
			} else if constexpr (sizeof(T) == 4) {
				wifi->writeIO7(alignedAddress, (u16)value); // Split into 16-bit writes
				wifi->writeIO7(alignedAddress + 2, (u16)(value >> 16));
			}
			// Byte writes aren't allowed
			break;
		case 0x6000000 ... 0x6FFFFFF: // Fallback for when VRAM banks overlap
			offset = alignedAddress & 0x1FFFF;

			if (ppu->vramCMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramCOffset & 1)))
				memcpy(ppu->vramC + offset, &value, sizeof(T));
			if (ppu->vramDMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramDOffset & 1)))
				memcpy(ppu->vramD + offset, &value, sizeof(T));

			if (!(ppu->vramCMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramCOffset & 1))) && !(ppu->vramDMapped7 && (((alignedAddress >> 17) & 1) == (ppu->vramDOffset & 1))))
				delay -= waitstates[0][sequential][sizeof(T) == 4][0x6] + 2;
			break;
		default:
			delay -= waitstates[0][sequential][sizeof(T) == 4][(alignedAddress >> 24) & 0xF] + 2;
			shared->log << fmt::format("[NDS7 Bus] Write to unknown location 0x{:0>8X} with {} byte value 0x{:0>{}X}\n", address, sizeof(T), value, sizeof(T) * 2);
			break;
		}
	}
}
template void BusARM7::write<u8>(u32, u8, bool);
template void BusARM7::write<u16>(u32, u16, bool);
template void BusARM7::write<u32>(u32, u32, bool);

void BusARM7::iCycle(int cycles) {
	delay += cycles * 2;
}

void BusARM7::breakpoint() {
	shared->addEvent(0, EventType::STOP);
}

u8 BusARM7::readIO(u32 address, bool final) {
	switch (address) {
	case 0x4000130 ... 0x4000137:
	case 0x4000204: case 0x4000205:
	case 0x4000241:
		return shared->readIO7(address);

	case 0x4000180 ... 0x4000187:
	case 0x4100000 ... 0x4100003:
		return ipc->readIO7(address, final);

	case 0x4000004 ... 0x4000007:
	case 0x4000240:
		return ppu->readIO7(address);

	case 0x40001A0 ... 0x40001BB:
	case 0x4100010 ... 0x4100014:
		return gamecard->readIO7(address, final);

	case 0x40000B0 ... 0x40000DF:
		return dma->readIO7(address);

	case 0x4000100 ... 0x400010F:
		return timer->readIO(address);

	case 0x4000138:
		return rtc->readIO7();
	case 0x4000139:
		return 0;

	case 0x40001C0 ... 0x40001C3:
		return spi->readIO7(address);

	case 0x4000400 ... 0x400051F:
		return apu->readIO7(address);

	case 0x4000208:
		return (u8)IME;
	case 0x4000209 ... 0x400020B:
		return 0;
	case 0x4000210:
		return (u8)(IE >> 0);
	case 0x4000211:
		return (u8)(IE >> 8);
	case 0x4000212:
		return (u8)(IE >> 16);
	case 0x4000213:
		return (u8)(IE >> 24);
	case 0x4000214:
		return (u8)(IF >> 0);
	case 0x4000215:
		return (u8)(IF >> 8);
	case 0x4000216:
		return (u8)(IF >> 16);
	case 0x4000217:
		return (u8)(IF >> 24);
	case 0x4000300:
		return POSTFLG;
	case 0x4000301: // TODO: Is this only allowed in BIOS?
		return HALTCNT;

	default:
		shared->log << fmt::format("[NDS7 Bus] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void BusARM7::writeIO(u32 address, u8 value, bool final) {
	switch (address) {
	case 0x4000130 ... 0x4000137:
	case 0x4000204: case 0x4000205:
		shared->writeIO7(address, value);
		break;

	case 0x4000180 ... 0x400018B:
		ipc->writeIO7(address, value, final);
		break;

	case 0x4000004 ... 0x4000007:
		ppu->writeIO7(address, value);
		break;

	case 0x40001A0 ... 0x40001BB:
	case 0x4100010 ... 0x4100014:
		gamecard->writeIO7(address, value);
		break;

	case 0x40000B0 ... 0x40000DF:
		dma->writeIO7(address, value);
		break;

	case 0x4000100 ... 0x400010F:
		timer->writeIO(address, value);
		break;

	case 0x4000138:
		rtc->writeIO7(value);
		break;
	case 0x4000139:
		break;

	case 0x40001C0 ... 0x40001C3:
		spi->writeIO7(address, value);
		break;

	case 0x4000400 ... 0x400051F:
		apu->writeIO7(address, value);
		break;

	case 0x4000208:
		IME = value & 1;
		refreshInterrupts();
		break;
	case 0x4000209 ... 0x400020B:
		break;
	case 0x4000210:
		IE = (IE & 0xFFFFFF00) | ((value & 0xFF) << 0);
		refreshInterrupts();
		break;
	case 0x4000211:
		IE = (IE & 0xFFFF00FF) | ((value & 0x3F) << 8);
		refreshInterrupts();
		break;
	case 0x4000212:
		IE = (IE & 0xFF00FFFF) | ((value & 0xDF) << 16);
		refreshInterrupts();
		break;
	case 0x4000213:
		IE = (IE & 0x00FFFFFF) | ((value & 0x01) << 24);
		refreshInterrupts();
		break;
	case 0x4000214:
		IF &= ~(value << 0);
		refreshInterrupts();
		break;
	case 0x4000215:
		IF &= ~(value << 8);
		refreshInterrupts();
		break;
	case 0x4000216:
		IF &= ~(value << 16);
		refreshInterrupts();
		break;
	case 0x4000217:
		IF &= ~(value << 24);
		refreshInterrupts();
		break;
	case 0x4000300:
		if (cpu->reg.R[15] <= 0x7FFF) {
			POSTFLG |= value & 1;
		}
		break;
	case 0x4000301: // TODO: Is this only allowed in BIOS?
		HALTCNT = value & 0xC0;

		if (HALTCNT == 0x40) { [[unlikely]]
			shared->log << "[NDS7 Bus] Attempt to switch to GBA mode\n";
			hacf();
		} else if (HALTCNT == 0xC0) { [[unlikely]]
			shared->log << "[NDS7 Bus] Attempt to switch to Sleep mode\n";
			hacf();
		}
		break;

	default:
		shared->log << fmt::format("[NDS7 Bus] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}
