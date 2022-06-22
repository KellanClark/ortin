
#include "busshared.hpp"

BusShared::BusShared() {
	psram = new u8[0x400000]; // 4MB
	memset(psram, 0, 0x400000);
}

BusShared::~BusShared() {
	delete[] psram;
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