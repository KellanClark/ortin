
#include "emulator/nds9/busarm9.hpp"

#include "emulator/ipc.hpp"
#include "emulator/ppu.hpp"
#include "emulator/cartridge/gamecard.hpp"

#include "arm946e/arm946e.hpp"
#include "emulator/dma.hpp"
#include "emulator/timer.hpp"
#include "emulator/nds9/dsmath.hpp"

static constexpr u32 toPage(u32 address) {
	return address >> 14; // Pages are 16KB
}

static constexpr u32 toAddress(u32 page) {
	return page << 14;
}

BusARM9::BusARM9(std::shared_ptr<BusShared> shared, std::shared_ptr<IPC> ipc, std::shared_ptr<PPU> ppu, std::shared_ptr<Gamecard> gamecard) :
	shared(shared),
	log(shared->log),
	ipc(ipc),
	ppu(ppu),
	gamecard(gamecard) {
	cpu = std::make_unique<ARM946E<BusARM9>>(*this);
	dma = std::make_unique<DMA<true>>(shared, *this);
	timer = std::make_unique<Timer>(true, shared);
	dsmath = std::make_unique<DSMath>(shared);

	bios = new u8[0x8000]; // 32KB
	memset(bios, 0, 0x8000);

	// Fill page tables
	memset(&readTable, 0, sizeof(readTable));
	memset(&readTable8, 0, sizeof(readTable8));
	memset(&writeTable, 0, sizeof(writeTable));

	// PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)
	for (int i = toPage(0x2000000); i < toPage(0x3000000); i++) {
		readTable[i] = readTable8[i] = writeTable[i] = shared->psram + ((toAddress(i) - 0x2000000)) % 0x400000;
	}

	POSTFLG = 0;
}

BusARM9::~BusARM9() {
	//
}

void BusARM9::reset() {
	delay = 0;

	IME = false;
	IE = IF = 0;

	dma->reset();
	dsmath->reset();
	cpu->resetARM946E();
}

void BusARM9::requestInterrupt(InterruptType type) {
	IF |= type;

	refreshInterrupts();
}

void BusARM9::refreshInterrupts() {
	if (IME && (IE & IF)) {
		cpu->processIrq = true;
	} else {
		cpu->processIrq = false;
	}
}

void BusARM9::refreshWramPages() {
	// Set the first two pages in one table
	switch (shared->WRAMCNT) { // > ARM9/ARM7 (0-3 = 32K/0K, 2nd 16K/1st 16K, 1st 16K/2nd 16K, 0K/32K)
	case 0:
		readTable[toPage(0x3000000)] = shared->wram;
		readTable[toPage(0x3004000)] = shared->wram + 0x4000;
		break;
	case 1:
		readTable[toPage(0x3000000)] = shared->wram + 0x4000;
		readTable[toPage(0x3004000)] = shared->wram + 0x4000;
		break;
	case 2:
		readTable[toPage(0x3000000)] = shared->wram;
		readTable[toPage(0x3004000)] = shared->wram;
		break;
	case 3:
		readTable[toPage(0x3000000)] = NULL;
		readTable[toPage(0x3004000)] = NULL;
		break;
	}

	// Mirror it across the full 16MB in all tables
	for (int i = toPage(0x3000000); i < toPage(0x4000000); i += 2) {
		readTable[i] = readTable8[i] = writeTable[i] = readTable[toPage(0x3000000)];
		readTable[i + 1] = readTable8[i + 1] = writeTable[i + 1] = readTable[toPage(0x3004000)];
	}
}

void BusARM9::refreshVramPages() {
	// Clear the VRAM section of the table
	for (int i = toPage(0x6000000); i < toPage(0x7000000); i++)
		readTable[i] = NULL;

	// LCDC maps
	if (ppu->vramAEnable && (ppu->vramAMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6800000) + i] = ppu->vramA + toAddress(i);
	}
	if (ppu->vramBEnable && (ppu->vramBMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6820000) + i] = ppu->vramB + toAddress(i);
	}
	if (ppu->vramCEnable && (ppu->vramCMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6840000) + i] = ppu->vramC + toAddress(i);
	}
	if (ppu->vramDEnable && (ppu->vramDMst == 0)) {
		for (int i = 0; i < toPage(0x20000); i++)
			readTable[toPage(0x6860000) + i] = ppu->vramD + toAddress(i);
	}
	if (ppu->vramEEnable && (ppu->vramEMst == 0)) {
		for (int i = 0; i < toPage(0x10000); i++)
			readTable[toPage(0x6880000) + i] = ppu->vramE + toAddress(i);
	}
	if (ppu->vramFEnable && (ppu->vramFMst == 0)) {
		readTable[toPage(0x6890000)] = ppu->vramF;
	}
	if (ppu->vramGEnable && (ppu->vramGMst == 0)) {
		readTable[toPage(0x6894000)] = ppu->vramG;
	}
	if (ppu->vramHEnable && (ppu->vramHMst == 0)) {
		readTable[toPage(0x6898000)] = ppu->vramH;
		readTable[toPage(0x689C000)] = ppu->vramH + 0x4000;
	}
	if (ppu->vramIEnable && (ppu->vramIMst == 0)) {
		readTable[toPage(0x68A0000)] = ppu->vramI;
	}

	// Mirror LCDC
	for (int i = toPage(0x6800000); i < toPage(0x7000000); i++)
		readTable[i] = readTable[toPage(toAddress(i) & ~0x0700000)];

	// The PPU's already done the bad stuff for us
	memcpy(&readTable[toPage(0x6000000)], ppu->vramPageTable, sizeof(ppu->vramPageTable));

	// readTable and writeTable will always be the same for VRAM
	for (int i = toPage(0x6000000); i < toPage(0x7000000); i++)
		writeTable[i] = readTable[i];
}

void BusARM9::refreshRomPages() {
	//
}

void BusARM9::hacf() {
	shared->addEvent(0, EventType::STOP);
}

template <typename T, bool code>
T BusARM9::read(u32 address, bool sequential) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 page = toPage(alignedAddress & 0x0FFFFFFF);
	u32 offset = alignedAddress & 0x3FFF;

	// TCM
	T val = 0;
	if (cpu->cp15.itcmEnable && !cpu->cp15.itcmWriteOnly && (address < cpu->cp15.itcmEnd)) {
		memcpy(&val, &cpu->cp15.itcm[alignedAddress & 0x7FFF], sizeof(T));
		return val;
	} else if (!code && cpu->cp15.dtcmEnable && !cpu->cp15.dtcmWriteOnly && (address >= cpu->cp15.dtcmStart) && (address < cpu->cp15.dtcmEnd)) {
		memcpy(&val, &cpu->cp15.dtcm[alignedAddress & 0x3FFF], sizeof(T));
		return val;
	}

	u8 *ptr;
	if constexpr (sizeof(T) == 1) {
		ptr = readTable8[page];
	} else {
		ptr = readTable[page];
	}

	if ((address < 0x10000000) && (ptr != NULL)) { [[likely]]
		memcpy(&val, ptr + offset, sizeof(T));
	} else {
		switch (address) {
		case 0x4000000 ... 0x4FFFFFF: // ARM9 I/O Ports
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
		case 0x5000000 ... 0x5FFFFFF: // PRAM
			memcpy(&val, ppu->pram + (alignedAddress & 0x7FF), sizeof(T));
			break;
		case 0x6000000 ... 0x67FFFFF: { // VRAM fallback
			PPU::VramInfoEntry entry = ppu->vramInfoTable[toPage(alignedAddress - 0x6000000)];
			T tmpVal = 0;

			if (entry.enableA) { memcpy(&tmpVal, ppu->vramA + (entry.bankA * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableB) { memcpy(&tmpVal, ppu->vramB + (entry.bankB * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableC) { memcpy(&tmpVal, ppu->vramC + (entry.bankC * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableD) { memcpy(&tmpVal, ppu->vramD + (entry.bankD * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableE) { memcpy(&tmpVal, ppu->vramE + (entry.bankE * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableF) { memcpy(&tmpVal, ppu->vramF + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableG) { memcpy(&tmpVal, ppu->vramG + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableH) { memcpy(&tmpVal, ppu->vramH + (entry.bankH * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
			if (entry.enableI) { memcpy(&tmpVal, ppu->vramI + offset, sizeof(T)); val |= tmpVal; }
			} break;
		case 0x7000000 ... 0x7FFFFFF: // OAM
			memcpy(&val, ppu->oam + (alignedAddress & 0x7FF), sizeof(T));
			break;
		case 0xFFFF0000 ... 0xFFFFFFFF: // ARM9-BIOS
			memcpy(&val, bios + (alignedAddress - 0xFFFF0000), sizeof(T));
			break;
		default:
			shared->log << fmt::format("[NDS9 Bus] Read from unknown location 0x{:0>8X}\n", address);
			break;
		}
	}

	return val;
}
template u8 BusARM9::read<u8, false>(u32, bool);
template u16 BusARM9::read<u16, true>(u32, bool);
template u16 BusARM9::read<u16, false>(u32, bool);
template u32 BusARM9::read<u32, true>(u32, bool);
template u32 BusARM9::read<u32, false>(u32, bool);

template <typename T>
void BusARM9::write(u32 address, T value, bool sequential) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	u32 page = toPage(alignedAddress & 0x0FFFFFFF);
	u32 offset = alignedAddress & 0x3FFF;
	u8 *ptr = readTable[page];

	// TCM
	if (cpu->cp15.itcmEnable && (address < cpu->cp15.itcmEnd)) {
		memcpy(&cpu->cp15.itcm[alignedAddress & 0x7FFF], &value, sizeof(T));
		return;
	} else if (cpu->cp15.dtcmEnable && (address >= cpu->cp15.dtcmStart) && (address < cpu->cp15.dtcmEnd)) {
		memcpy(&cpu->cp15.dtcm[alignedAddress & 0x3FFF], &value, sizeof(T));
		return;
	}

	if ((address < 0x10000000) && (ptr != NULL)) { [[likely]]
		memcpy(ptr + offset, &value, sizeof(T));
	} else {
		switch (address) {
		case 0x4000000 ... 0x4FFFFFF: // NDS9 I/O Ports
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
		case 0x5000000 ... 0x5FFFFFF: // PRAM
			memcpy(ppu->pram + (alignedAddress & 0x7FF), &value, sizeof(T));
			break;
		case 0x6000000 ... 0x67FFFFF: { // VRAM fallback
			PPU::VramInfoEntry entry = ppu->vramInfoTable[toPage(alignedAddress - 0x6000000)];

			if (entry.enableA) memcpy(ppu->vramA + (entry.bankA * 0x4000) + offset, &value, sizeof(T));
			if (entry.enableB) memcpy(ppu->vramB + (entry.bankB * 0x4000) + offset, &value, sizeof(T));
			if (entry.enableC) memcpy(ppu->vramC + (entry.bankC * 0x4000) + offset, &value, sizeof(T));
			if (entry.enableD) memcpy(ppu->vramD + (entry.bankD * 0x4000) + offset, &value, sizeof(T));
			if (entry.enableE) memcpy(ppu->vramE + (entry.bankE * 0x4000) + offset, &value, sizeof(T));
			if (entry.enableF) memcpy(ppu->vramF + offset, &value, sizeof(T));
			if (entry.enableG) memcpy(ppu->vramG + offset, &value, sizeof(T));
			if (entry.enableH) memcpy(ppu->vramH + (entry.bankH * 0x4000) + offset, &value, sizeof(T));
			if (entry.enableI) memcpy(ppu->vramI + offset, &value, sizeof(T));
			} break;
		case 0x7000000 ... 0x7FFFFFF: // OAM
			memcpy(ppu->oam + (alignedAddress & 0x7FF), &value, sizeof(T));
			break;
		default:
			shared->log << fmt::format("[NDS9 Bus] Write to unknown location 0x{:0>8X} with value 0x{:0>8X} of size {}\n", address, value, sizeof(T));
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

void BusARM9::breakpoint() {
	shared->addEvent(0, EventType::STOP);
}

u8 BusARM9::readIO(u32 address, bool final) {
	switch (address) {
	case 0x4000130 ... 0x4000137:
	case 0x4000204: case 0x4000205:
	case 0x4000247:
		return shared->readIO9(address);

	case 0x4000180 ... 0x4000187:
	case 0x4100000 ... 0x4100003:
		return ipc->readIO9(address, final);

	case 0x4000000 ... 0x400006F:
	case 0x4000304 ... 0x4000307:
	case 0x4001000 ... 0x400106F:
		return ppu->readIO9(address);

	case 0x40001A0 ... 0x40001BB:
	case 0x4100010 ... 0x4100014:
		return gamecard->readIO9(address, final);

	case 0x40000B0 ... 0x40000EF:
		return dma->readIO9(address);

	case 0x4000100 ... 0x400010F:
		return timer->readIO(address);

	case 0x4000280 ... 0x40002BF:
		return dsmath->readIO9(address, final);

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

	default:
		shared->log << fmt::format("[NDS9 Bus] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void BusARM9::writeIO(u32 address, u8 value, bool final) {
	switch (address) {
	case 0x4000130 ... 0x4000137:
	case 0x4000204: case 0x4000205:
	case 0x4000247:
		shared->writeIO9(address, value);
		break;

	case 0x4000180 ... 0x400018B:
		ipc->writeIO9(address, value, final);
		break;

	case 0x4000000 ... 0x400006F:
	case 0x4000240 ... 0x4000246:
	case 0x4000248:
	case 0x4000249:
	case 0x4000304 ... 0x4000307:
	case 0x4001000 ... 0x400106F:
		ppu->writeIO9(address, value);
		break;

	case 0x40001A0 ... 0x40001BB:
	case 0x4100010 ... 0x4100014:
		gamecard->writeIO9(address, value);
		break;

	case 0x40000B0 ... 0x40000EF:
		dma->writeIO9(address, value);
		break;

	case 0x4000100 ... 0x400010F:
		timer->writeIO(address, value);
		break;

	case 0x4000280 ... 0x40002BF:
		dsmath->writeIO9(address, value, final);
		break;

	case 0x4000208:
		IME = value & 1;
		refreshInterrupts();
		break;
	case 0x4000209 ... 0x400020B:
		break;
	case 0x4000210:
		IE = (IE & 0xFFFFFF00) | ((value & 0x7F) << 0);
		refreshInterrupts();
		break;
	case 0x4000211:
		IE = (IE & 0xFFFF00FF) | ((value & 0x3F) << 8);
		refreshInterrupts();
		break;
	case 0x4000212:
		IE = (IE & 0xFF00FFFF) | ((value & 0x3F) << 16);
		refreshInterrupts();
		break;
	case 0x4000213:
		IE = (IE & 0x00FFFFFF) | ((value & 0x00) << 24);
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
		POSTFLG = ((POSTFLG | value) & 1) | (value & 2);
		break;

	default:
		shared->log << fmt::format("[NDS9 Bus] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}

u32 BusARM9::coprocessorRead(u32 copNum, u32 copOpc, u32 copSrcDestReg, u32 copOpReg, u32 copOpcType) {
	if (copNum != 15) {
		shared->log << "Invalid coprocessor: p" << copNum << "\n";
		hacf();
		return 0;
	}
	if (copOpc != 0) {
		shared->log << "Invalid coprocessor opcode: " << copOpc << "\n";
		hacf();
		return 0;
	}

	switch (copSrcDestReg) {
	case 0: // ID Codes
		if (copOpReg != 0) {
			break;
		}

		switch (copOpcType) {
		default:
		case 0: // Main ID Register
			return 0x41059461;
		case 1: // Cache Type Register
			return 0x0F0D2112;
		case 2: // Tightly Coupled Memory Size Register
			return 0x00140180;
		}
	case 1: // Control Register
		return cpu->cp15.control;
	case 2:
	case 3:
	case 5:
	case 6:
	case 7: return 0; // I hate crt0
	case 9:
		if (copOpReg == 1) {
			if (copOpcType == 0) {
				return cpu->cp15.dtcmConfig;
			} else if (copOpcType == 1) {
				return cpu->cp15.itcmConfig;
			}
		}
		return 0; // I hate crt0
	}

	shared->log << fmt::format("Invalid CP15 register {}, c{}, c{}, {}", copOpc, copSrcDestReg, copOpReg, copOpcType);
	hacf();
	return 0;
}

void BusARM9::coprocessorWrite(u32 copNum, u32 copOpc, u32 copSrcDestReg, u32 copOpReg, u32 copOpcType, u32 value) {
	if (copNum != 15) {
		shared->log << "Invalid coprocessor: p" << copNum << "\n";
		hacf();
		return;
	}
	if (copOpc != 0) {
		shared->log << "Invalid coprocessor opcode: " << copOpc << "\n";
		hacf();
		return;
	}

	switch (copSrcDestReg) {
	case 1: // Control register
		if (copOpReg != 0)
			break;

		cpu->cp15.control = (value & 0xFF085) | 0x00000078;
		return;
	case 2:
	case 3:
	case 5:
	case 6: return; // I hate crt0
	case 7:
		if (((copOpReg == 0) && (copOpcType == 4)) || ((copOpReg == 8) && (copOpcType == 2))) {
			cpu->cp15.halted = true;
		}
		return;
	case 9:
		if (copOpReg == 1) {
			if (copOpcType == 0) {
				cpu->cp15.dtcmConfig = value & 0xFFFFF03E;

				cpu->cp15.dtcmStart = cpu->cp15.dtcmRegionBase << 12;
				cpu->cp15.dtcmEnd = cpu->cp15.dtcmStart + (512 << cpu->cp15.dtcmVirtualSize);
			} else if (copOpcType == 1) {
				cpu->cp15.itcmConfig = value & 0x0000003E;

				cpu->cp15.itcmEnd = 512 << cpu->cp15.itcmVirtualSize;
			}
		}
		return; // I hate crt0
	}

	shared->log << fmt::format("Invalid CP15 register {}, c{}, c{}, {}", copOpc, copSrcDestReg, copOpReg, copOpcType);
	hacf();
}
