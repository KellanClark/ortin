#pragma once

#define ARM946E_DISABLE_FIQ

#include "types.hpp"
#include "emulator/busshared.hpp"

class BusShared;
class IPC;
class PPU;
class Gamecard;
template <class> class ARM946E;
template <bool> class DMA;
class Timer;
class DSMath;

class BusARM9 {
public:
	// Connected components
	std::shared_ptr<BusShared> shared;
	std::shared_ptr<IPC> ipc;
	std::shared_ptr<PPU> ppu;
	std::shared_ptr<Gamecard> gamecard;
	std::unique_ptr<ARM946E<BusARM9>> cpu;
	std::unique_ptr<DMA<true>> dma;
	std::unique_ptr<Timer> timer;
	std::unique_ptr<DSMath> dsmath;
	u8 *bios;

	// For external use
	BusARM9(std::shared_ptr<BusShared> shared, std::shared_ptr<IPC> ipc, std::shared_ptr<PPU> ppu, std::shared_ptr<Gamecard> gamecard);
	~BusARM9();
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
		INT_GEOMETRY_FIFO = 1 << 21,
	};

	bool IME; // NDS9 - 0x4000208
	u32 IE; // NDS9 - 0x4000210
	u32 IF; // NDS9 - 0x4000214
	u8 POSTFLG; // NDS9 - 0x4000300

	void requestInterrupt(InterruptType type);
	void refreshInterrupts();

	// For CPU and memory
	i64 delay;
	u8 *readTable[0x4000];
	u8 *readTable8[0x4000];
	u8 *writeTable[0x4000];

	void refreshWramPages();
	void refreshVramPages();
	void refreshRomPages();

	std::stringstream &log;
	void hacf(); // TODO: Document this interface
	template <typename T, bool code> T read(u32 address, bool sequential);
	template <typename T> void write(u32 address, T value, bool sequential);
	void iCycle(int cycles);
	void breakpoint();

	u8 readIO(u32 address, bool final);
	void writeIO(u32 address, u8 value, bool final);

	// Coprocessor
	u32 coprocessorRead(u32 copNum, u32 copOpc, u32 copSrcDestReg, u32 copOpReg, u32 copOpcType);
	void coprocessorWrite(u32 copNum, u32 copOpc, u32 copSrcDestReg, u32 copOpReg, u32 copOpcType, u32 value);
};
