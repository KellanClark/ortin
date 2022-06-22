
#include "busarm9.hpp"

static constexpr u32 toPage(u32 address) {
	return address >> 14; // Pages are 16KB
}

static constexpr u32 toAddress(u32 page) {
	return page << 14;
}

BusARM9::BusARM9(BusShared &shared, PPU &ppu, std::stringstream &log) : cpu(*this), shared(shared), log(log), ppu(ppu) {
	// Fill page tables
	memset(&readTable, 0, sizeof(readTable));
	memset(&readTable8, 0, sizeof(readTable8));
	memset(&writeTable, 0, sizeof(writeTable));

	// PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)
	for (int i = toPage(0x2000000); i < toPage(0x3000000); i++) {
		readTable[i] = readTable8[i] = writeTable[i] = shared.psram + ((toAddress(i) - 0x2000000)) % 0x400000;
	}
}

BusARM9::~BusARM9() {
	//
}

void BusARM9::reset() {
	log.str("");

	cpu.resetARM946E();
}

void BusARM9::refreshVramPages() {
	// Clear the VRAM section of the table
	for (int i = toPage(0x6000000); i < toPage(0x7000000); i++)
		readTable[i] = NULL;

	// LCDC maps
	if (ppu.vramAEnable && (ppu.vramAMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6800000) + i] = ppu.vramA + toAddress(i);
	}
	if (ppu.vramBEnable && (ppu.vramBMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6820000) + i] = ppu.vramB + toAddress(i);
	}
	if (ppu.vramCEnable && (ppu.vramCMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6840000) + i] = ppu.vramC + toAddress(i);
	}
	if (ppu.vramDEnable && (ppu.vramDMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6860000) + i] = ppu.vramD + toAddress(i);
	}
	if (ppu.vramEEnable && (ppu.vramEMst == 0)) {
		for (int i = 0; i < toPage(0x10000); i++)
			readTable[toPage(0x6880000) + i] = ppu.vramE + toAddress(i);
	}
	if (ppu.vramFEnable && (ppu.vramFMst == 0)) {
		readTable[toPage(0x6890000)] = ppu.vramF;
	}
	if (ppu.vramGEnable && (ppu.vramGMst == 0)) {
		readTable[toPage(0x6894000)] = ppu.vramG;
	}
	if (ppu.vramHEnable && (ppu.vramHMst == 0)) {
		readTable[toPage(0x6898000)] = ppu.vramH;
		readTable[toPage(0x689C000)] = ppu.vramH + 0x4000;
	}
	if (ppu.vramIEnable && (ppu.vramIMst == 0)) {
		readTable[toPage(0x68A0000)] = ppu.vramI;
	}

	// Mirror LCDC
	for (int i = toPage(0x6800000); i < toPage(0x7000000); i++)
		readTable[i] = readTable[toPage(toAddress(i) & ~0x0700000)];

	// readTable and writeTable will always be the same for VRAM
	for (int i = toPage(0x6000000); i < toPage(0x7000000); i++)
		writeTable[i] = readTable[i];
}

void BusARM9::hacf() {
	shared.addEvent(0, EventType::STOP);
}

template <typename T, bool code>
u32 BusARM9::read(u32 address, bool sequential) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 page = toPage(alignedAddress & 0x0FFFFFFF);
	u32 offset = alignedAddress & 0x3FFF;

	u8 *ptr;
	if constexpr (sizeof(T) == 1) {
		ptr = readTable8[page];
	} else {
		ptr = readTable[page];
	}

	u32 val = 0;
	if ((address < 0x10000000) && (ptr != NULL)) { [[likely]]
		memcpy(&val, ptr + offset, sizeof(T));
	} else {
		switch (address) {
		case 0x4000000 ... 0x4FFFFFF:
			if constexpr (sizeof(T) == 4) {
				val |= (readIO(alignedAddress | 0) << 0);
				val |= (readIO(alignedAddress | 1) << 8);
				val |= (readIO(alignedAddress | 2) << 16);
				val |= (readIO(alignedAddress | 3) << 24);
			} else if constexpr (sizeof(T) == 2) {
				val |= (readIO(alignedAddress | 0) << 0);
				val |= (readIO(alignedAddress | 1) << 8);
			} else {
				val = readIO(alignedAddress);
			}
			break;
		default:
			log << fmt::format("[ARM9 Bus] Read from unknown location 0x{:0>8X}\n", address);
			break;
		}
	}

	return val;
}
template u32 BusARM9::read<u8, false>(u32, bool);
template u32 BusARM9::read<u16, true>(u32, bool);
template u32 BusARM9::read<u16, false>(u32, bool);
template u32 BusARM9::read<u32, true>(u32, bool);
template u32 BusARM9::read<u32, false>(u32, bool);

template <typename T>
void BusARM9::write(u32 address, T value, bool sequential) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 page = toPage(alignedAddress & 0x0FFFFFFF);
	u32 offset = alignedAddress & 0x3FFF;
	u8 *ptr = readTable[page];

	if ((address < 0x10000000) && (ptr != NULL)) { [[likely]]
		memcpy(ptr + offset, &value, sizeof(T));
	} else {
		switch (address) {
		case 0x4000000 ... 0x4FFFFFF:
			if constexpr (sizeof(T) == 4) {
				writeIO(alignedAddress | 0, (u8)(value >> 0));
				writeIO(alignedAddress | 1, (u8)(value >> 8));
				writeIO(alignedAddress | 2, (u8)(value >> 16));
				writeIO(alignedAddress | 3, (u8)(value >> 24));
			} else if constexpr (sizeof(T) == 2) {
				writeIO(alignedAddress | 0, (u8)(value >> 0));
				writeIO(alignedAddress | 1, (u8)(value >> 8));
			} else {
				writeIO(alignedAddress, value);
			}
			break;
		default:
			log << fmt::format("[ARM9 Bus] Write to unknown location 0x{:0>8X} with value 0x{:0>8X} of size {}\n", address, value, sizeof(T));
			break;
		}
	}
}
template void BusARM9::write<u8>(u32, u8, bool);
template void BusARM9::write<u16>(u32, u16, bool);
template void BusARM9::write<u32>(u32, u32, bool);

void BusARM9::iCycle(int cycles) {
	//
}

u8 BusARM9::readIO(u32 address) {
	switch (address) {
	case 0x4000000 ... 0x400006F:
	case 0x4000304 ... 0x4000307:
		return ppu.readIO9(address);
	default:
		log << fmt::format("[ARM9 Bus] Read from unknown IO register 0x{:0>8X}\n", address);
		return 0;
	}
}

void BusARM9::writeIO(u32 address, u8 value) {
	switch (address) {
	case 0x4000000 ... 0x400006F:
	case 0x4000240 ... 0x4000249:
	case 0x4000304 ... 0x4000307:
		ppu.writeIO9(address, value);
		break;
	default:
		log << fmt::format("[ARM9 Bus] Write to unknown IO register 0x{:0>8X} with value 0x{:0>8X}\n", address, value);
		break;
	}
}
