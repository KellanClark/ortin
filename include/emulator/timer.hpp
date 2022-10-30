#ifndef ORTIN_TIMER_HPP
#define ORTIN_TIMER_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"

class Timer {
public:
	bool timer9;
	std::shared_ptr<BusShared> shared;

	// External Use
	Timer(bool timer9, std::shared_ptr<BusShared> shared);
	~Timer();
	void reset();

	// Internal use
	void updateCounter(int channel);
	void scheduleTimer(int channel);
	void checkOverflow();

	// Memory Interface
	u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);

	// I/O Registers
	struct {
		u16 reload;
		u16 TIMCNT_L;
		union {
			struct {
				u16 prescaler : 2;
				u16 cascade : 1;
				u16 : 3;
				u16 irqEnable : 1;
				u16 startStop : 1;
				u16 : 8;
			};
			u16 TIMCNT_H;
		};

		u64 lastIncrementTimestamp;
		bool interruptRequested;
	} timer[4];
};

#endif //ORTIN_TIMER_HPP