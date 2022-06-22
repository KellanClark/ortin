
#include "busarm7.hpp"

static constexpr u32 toPage(u32 address) {
	return address >> 14; // Pages are 16KB
}

static constexpr u32 toAddress(u32 page) {
	return page << 14;
}

BusARM7::BusARM7(BusShared &shared, PPU &ppu, std::stringstream &log) : cpu(*this), shared(shared), log(log), ppu(ppu) {
	wram = new u8[0x10000]; // 64KB
	memset(wram, 0, 0x10000);

	// Fill page tables
	memset(&readTable, 0, sizeof(readTable));
	memset(&writeTable, 0, sizeof(writeTable));

	// PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)
	for (int i = toPage(0x2000000); i < toPage(0x3000000); i++) {
		readTable[i] = writeTable[i] = shared.psram + ((toAddress(i) - 0x2000000)) % 0x400000;
	}

	// ARM7 WRAM (64KB mirrored 0x3800000 - 0x4000000)
	for (int i = toPage(0x3800000); i < toPage(0x4000000); i++) {
		readTable[i] = writeTable[i] = wram + ((toAddress(i) - 0x3800000)) % 0x10000;
	}
}

BusARM7::~BusARM7() {
	delete[] wram;
}

void BusARM7::reset() {
	log.str("");

	cpu.resetARM7TDMI();
}

void BusARM7::hacf() {
	shared.addEvent(0, EventType::STOP);
}

template <typename T, bool code>
u32 BusARM7::read(u32 address, bool sequential) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 page = toPage(alignedAddress & 0x0FFFFFFF);
	u32 offset = alignedAddress & 0x3FFF;
	u8 *ptr = readTable[page];
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
			log << fmt::format("[ARM7 Bus] Read from unknown location 0x{:0>8X}\n", address);
			break;
		}
	}

	return val;
}
template u32 BusARM7::read<u8, false>(u32, bool);
template u32 BusARM7::read<u16, true>(u32, bool);
template u32 BusARM7::read<u16, false>(u32, bool);
template u32 BusARM7::read<u32, true>(u32, bool);
template u32 BusARM7::read<u32, false>(u32, bool);

template <typename T>
void BusARM7::write(u32 address, T value, bool sequential) {
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
			log << fmt::format("[ARM7 Bus] Write to unknown location 0x{:0>8X} with value 0x{:0>8X} of size {}\n", address, value, sizeof(T));
			break;
		}
	}
}
template void BusARM7::write<u8>(u32, u8, bool);
template void BusARM7::write<u16>(u32, u16, bool);
template void BusARM7::write<u32>(u32, u32, bool);

void BusARM7::iCycle(int cycles) {
	//
}

u8 BusARM7::readIO(u32 address) {
	switch (address) {
	/*case 0x4000304 ... 0x4000307:
		ppu.readIO7(address);
		break;*/
	default:
		log << fmt::format("[ARM7 Bus] Read from unknown IO register 0x{:0>8X}\n", address);
		return 0;
	}
}

void BusARM7::writeIO(u32 address, u8 value) {
	switch (address) {
	/*case 0x4000304 ... 0x4000307:
		ppu.writeIO7(address, value);
		break;*/
	default:
		log << fmt::format("[ARM7 Bus] Write to unknown IO register 0x{:0>8X} with value 0x{:0>8X}\n", address, value);
		break;
	}
}
