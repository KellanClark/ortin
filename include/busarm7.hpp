
#ifndef ORTIN_BUSARM7_HPP
#define ORTIN_BUSARM7_HPP

#define ARM7TDMI_DISABLE_FIQ

#include "types.hpp"
#include "busshared.hpp"
#include "ppu.hpp"
#include "arm7tdmi/arm7tdmi.hpp"

class BusARM7 {
public:
	// Connected components
	ARM7TDMI<BusARM7> cpu;
	BusShared& shared;
	PPU& ppu;
	u8 *wram;

	// For normal use
	std::stringstream& log;

	BusARM7(BusShared &shared, PPU &ppu, std::stringstream &log);
	~BusARM7();
	void reset();

	// For CPU and memory
	u8 *readTable[0x4000];
	u8 *writeTable[0x4000];

	void hacf();
	template <typename T, bool code> u32 read(u32 address, bool sequential);
	template <typename T> void write(u32 address, T value, bool sequential);
	void iCycle(int cycles);

	u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);
};

#endif //ORTIN_BUSARM7_HPP
