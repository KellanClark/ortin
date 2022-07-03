#ifndef ORTIN_BUSSHARED_HPP
#define ORTIN_BUSSHARED_HPP

#include "types.hpp"

#include <queue>

enum EventType {
	STOP,
	PPU_LINE_START,
	PPU_HBLANK,
	REFRESH_VRAM_PAGES
};

// This class is not the same as BusARM9 and BusARM7.
// It's a catch-all for shared memory, I/O registers, and other things that either don't warrant their own class or need to be seen by everything.
class BusShared {
public:
	u8 *psram;
	std::stringstream& log;

	BusShared(std::stringstream &log);
	~BusShared();
	void reset();

	u8 readIO9(u32 address);
	void writeIO9(u32 address, u8 value);
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	u16 KEYINPUT; // 0x4000130
	u16 KEYCNT; // 0x4000132
	u16 EXTKEYIN; // NDS7 - 0x4000136

	// For the scheduler
	struct Event {
		u64 timeStamp;
		EventType type;
	};
	struct eventSorter {
		bool operator()(const Event &lhs, const Event &rhs);
	};

	void addEvent(u64 cycles, EventType type);

	u64 currentTime;
	std::priority_queue<Event, std::vector<Event>, eventSorter> eventQueue;
};

#endif //ORTIN_BUSSHARED_HPP
