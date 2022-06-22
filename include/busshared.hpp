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
	BusShared();
	~BusShared();
	void reset();

	u8 *psram;

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
