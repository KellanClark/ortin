#ifndef ORTIN_BUSSHARED_HPP
#define ORTIN_BUSSHARED_HPP

#include "types.hpp"

#include <queue>

enum EventType {
	STOP,
	IPC_SYNC9,
	IPC_SYNC7,
	IPC_SEND_FIFO9,
	IPC_SEND_FIFO7,
	IPC_RECV_FIFO9,
	IPC_RECV_FIFO7,
	PPU_LINE_START,
	PPU_HBLANK,
	REFRESH_WRAM_PAGES,
	REFRESH_VRAM_PAGES,
	SPI_FINISHED,
	RTC_REFRESH,
	SERIAL_INTERRUPT,
	TIMER_OVERFLOW_9,
	TIMER_OVERFLOW_7,
	GAMECARD_TRANSFER_READY
};

// This class is not the same as BusARM9 and BusARM7.
// It's a catch-all for shared memory, I/O registers, and other things that either don't warrant their own class or need to be seen by everything.
class BusShared {
public:
	u8 *psram;
	u8 *wram;
	std::stringstream& log;

	BusShared(std::stringstream &log);
	~BusShared();
	void reset();

	u8 readIO9(u32 address);
	void writeIO9(u32 address, u8 value);
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	u16 KEYINPUT; // 0x4000130
	u16 KEYCNT9; // NDS9 - 0x4000132
	u16 KEYCNT7; // NDS7 - 0x4000132
	u16 EXTKEYIN; // NDS7 - 0x4000136
	union {
		struct {
			u16 : 7; // Handled in separate busses
			u16 gbaSlotAccess : 1;
			u16 : 3;
			u16 ndsSlotAccess : 1;
			u16 : 4; // I am *not* handling memory access conflicts
		};
		u16 EXMEMCNT; // NDS9 - 0x4000204 aka. EXMEMSTAT NDS7 - 0x4000204
	};
	u8 WRAMCNT; // NDS9 - 0x4000247 aka. WRAMSTAT NDS7 - 0x4000241

	// For the scheduler
	struct Event {
		u64 timeStamp;
		EventType type;
	};
	struct eventSorter {
		bool operator()(const Event &lhs, const Event &rhs);
	};

	void addEvent(u64 cycles, EventType type);
	void addEventAbsolute(u64 time, EventType type);

	u64 currentTime;
	std::priority_queue<Event, std::vector<Event>, eventSorter> eventQueue;
};

#endif //ORTIN_BUSSHARED_HPP
