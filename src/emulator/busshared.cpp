#include "emulator/busshared.hpp"

BusShared::BusShared() {
	psram = new u8[0x400000]; // 4MB
	memset(psram, 0, 0x400000);
	wram = new u8[0x8000]; // 32KB
	memset(wram, 0, 0x8000);

	KEYINPUT = 0x03FF;
	KEYCNT9 = KEYCNT7 = 0x0000;
	EXTKEYIN = 0x007F;
	EXMEMCNT = 0;
	WRAMCNT = 0x03;
}

BusShared::~BusShared() {
	delete[] psram;
	delete[] wram;
}

void BusShared::reset() {
	currentTime = 0;
	eventQueue = {};
}

bool BusShared::eventSorter::operator()(const Event &lhs, const Event &rhs) {
	return lhs.timeStamp > rhs.timeStamp;
}

void BusShared::addEvent(u64 cycles, EventType type) {
	eventQueue.push(Event{currentTime + cycles, type});
}

void BusShared::addEventAbsolute(u64 time, EventType type) {
	eventQueue.push(Event{time, type});
}

u8 BusShared::readIO9(u32 address) {
	switch (address) {
	case 0x4000130:
		return (u8)KEYINPUT;
	case 0x4000131:
		return (u8)(KEYINPUT >> 8);
	case 0x4000132:
		return (u8)KEYCNT9;
	case 0x4000133:
		return (u8)(KEYCNT9 >> 8);
	case 0x4000204:
		return (u8)EXMEMCNT;
	case 0x4000205:
		return (u8)(EXMEMCNT >> 8);
	case 0x4000247:
		return WRAMCNT;
	default:
		log << fmt::format("[NDS9 Bus][Shared] Read from unknown IO register 0x{:0>8X}\n", address);
		return 0;
	}
}

void BusShared::writeIO9(u32 address, u8 value) {
	switch (address) {
	case 0x4000132:
		KEYCNT9 = (KEYCNT9 & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000133:
		KEYCNT9 = (KEYCNT9 & 0x00FF) | ((value & 0xC3) << 8);
		break;
	case 0x4000204:
		EXMEMCNT = (EXMEMCNT & 0xFF00) | ((value & 0xFF) << 0);
		EXMEMSTAT = (EXMEMSTAT & 0x007F) | (EXMEMCNT & 0xFF80);
		break;
	case 0x4000205:
		EXMEMCNT = (EXMEMCNT & 0x00FF) | (((value & 0xC8) | 0x20) << 8);
		EXMEMSTAT = (EXMEMSTAT & 0x007F) | (EXMEMCNT & 0xFF80);
		break;
	case 0x4000247:
		WRAMCNT = value & 0x03;

		addEvent(0, EventType::REFRESH_WRAM_PAGES);
		break;
	default:
		log << fmt::format("[NDS9 Bus][Shared] Write to unknown IO register 0x{:0>8X} with value 0x{:0>8X}\n", address, value);
		break;
	}
}

u8 BusShared::readIO7(u32 address) {
	switch (address) {
	case 0x4000130:
		return (u8)KEYINPUT;
	case 0x4000131:
		return (u8)(KEYINPUT >> 8);
	case 0x4000132:
		return (u8)KEYCNT7;
	case 0x4000133:
		return (u8)(KEYCNT7 >> 8);
	case 0x4000136:
		return (u8)EXTKEYIN;
	case 0x4000137:
		return (u8)(EXTKEYIN >> 8);
	case 0x4000204:
		return (u8)EXMEMSTAT;
	case 0x4000205:
		return (u8)(EXMEMSTAT >> 8);
	case 0x4000241:
		return WRAMCNT;
	default:
		log << fmt::format("[NDS7 Bus][Shared] Read from unknown IO register 0x{:0>8X}\n", address);
		return 0;
	}
}

void BusShared::writeIO7(u32 address, u8 value) {
	switch (address) {
	case 0x4000132:
		KEYCNT7 = (KEYCNT7 & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000133:
		KEYCNT7 = (KEYCNT7 & 0x00FF) | ((value & 0xC3) << 8);
		break;
	case 0x4000204:
		EXMEMSTAT = (EXMEMCNT & 0xFF80) | ((value & 0x7F) << 0);
		break;
	case 0x4000205:
		break;
	default:
		log << fmt::format("[NDS7 Bus][Shared] Write to unknown IO register 0x{:0>8X} with value 0x{:0>8X}\n", address, value);
		break;
	}
}
