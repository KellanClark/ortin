
#ifndef ORTIN_BUSARM7_HPP
#define ORTIN_BUSARM7_HPP

#define ARM7TDMI_DISABLE_FIQ

#include "types.hpp"
#include "emulator/busshared.hpp"
#include "emulator/ipc.hpp"
#include "emulator/ppu.hpp"
#include "emulator/cartridge/gamecard.hpp"
#include "emulator/dma.hpp"
#include "emulator/timer.hpp"
#include "emulator/nds7/rtc.hpp"
#include "emulator/nds7/spi.hpp"
#include "emulator/nds7/apu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"

class BusARM7 {
public:
	// Connected components
	ARM7TDMI<BusARM7> cpu;
	IPC& ipc;
	PPU& ppu;
	Gamecard& gamecard;
	DMA<false> dma;
	Timer timer;
	RTC rtc;
	SPI spi;
	APU apu;
	u8 *wram;
	u8 *bios;

	BusShared& shared;
	std::stringstream& log;

	// For external use
	BusARM7(BusShared &shared, IPC &ipc, PPU &ppu, Gamecard &gamecard, std::stringstream &log);
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
	u8 *readTable[0x4000];
	u8 *writeTable[0x4000];

	void refreshWramPages();
	void refreshVramPages();
	void refreshRomPages();

	void hacf();
	template <typename T, bool code> T read(u32 address, bool sequential);
	template <typename T> void write(u32 address, T value, bool sequential);
	void iCycle(int cycles);
	void breakpoint();

	u8 readIO(u32 address, bool final);
	void writeIO(u32 address, u8 value, bool final);
};

#endif //ORTIN_BUSARM7_HPP
