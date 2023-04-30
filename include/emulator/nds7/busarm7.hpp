#pragma once

#define ARM7TDMI_DISABLE_FIQ

#include "types.hpp"
#include "emulator/busshared.hpp"

class BusShared;
class IPC;
class PPU;
class Gamecard;
template <class> class ARM7TDMI;
template <bool> class DMA;
class Timer;
class RTC;
class SPI;
class APU;

class BusARM7 {
public:
	// Connected components
	std::shared_ptr<BusShared> shared;
	std::shared_ptr<IPC> ipc;
	std::shared_ptr<PPU> ppu;
	std::shared_ptr<Gamecard> gamecard;
	std::unique_ptr<ARM7TDMI<BusARM7>> cpu;
	std::unique_ptr<DMA<false>> dma;
	std::unique_ptr<Timer> timer;
	std::unique_ptr<RTC> rtc;
	std::unique_ptr<SPI> spi;
	std::unique_ptr<APU> apu;
	u8 *wram;
	u8 *bios;

	// For external use
	BusARM7(std::shared_ptr<BusShared> shared, std::shared_ptr<IPC> ipc, std::shared_ptr<PPU> ppu, std::shared_ptr<Gamecard> gamecard);
	~BusARM7();
	void reset();

	// Interrupts
	enum InterruptType {
		INT_VBLANK = 1 << 0,
		INT_HBLANK = 1 << 1,
		INT_VCOUNT = 1 << 2,
		INT_TIMER_0 = 1 << 3,
		INT_TIMER_1 = 1 << 4,
		INT_TIMER_2 = 1 << 5,
		INT_TIMER_3 = 1 << 6,
		INT_SERIAL = 1 << 7,
		INT_DMA_0 = 1 << 8,
		INT_DMA_1 = 1 << 9,
		INT_DMA_2 = 1 << 10,
		INT_DMA_3 = 1 << 11,
		INT_KEYPAD = 1 << 12,
		INT_GBA_SLOT = 1 << 13,
		INT_IPC_SYNC = 1 << 16,
		INT_IPC_SEND_FIFO = 1 << 17,
		INT_IPC_RECV_FIFO = 1 << 18,
		INT_NDS_SLOT_DATA = 1 << 19,
		INT_NDS_SLOT_IREQ = 1 << 20,
		INT_UNFOLD = 1 << 22,
		INT_SPI = 1 << 23,
		INT_WIFI = 1 << 24,
	};

	bool IME; // NDS7 - 0x4000208
	u32 IE; // NDS7 - 0x4000210
	u32 IF; // NDS7 - 0x4000214
	u8 POSTFLG; // NDS7 - 0x4000300
	u8 HALTCNT; // NDS7 - 0x4000301

	void requestInterrupt(InterruptType type);
	void refreshInterrupts();

	// For CPU and memory
	i64 delay;
	int waitstates[2][2][2][16]; // 0/1=data/code, 0/1=nonsequential/sequential, 0/1=32/16bit, 0..15=bits24..27
	u8 *readTable[0x4000];
	u8 *writeTable[0x4000];

	void refreshWramPages();
	void refreshVramPages();
	void refreshRomPages();

	std::stringstream &log;
	void hacf();
	template <typename T, bool code> T read(u32 address, bool sequential);
	template <typename T> void write(u32 address, T value, bool sequential);
	void iCycle(int cycles);
	void breakpoint();

	u8 readIO(u32 address, bool final);
	void writeIO(u32 address, u8 value, bool final);
};
