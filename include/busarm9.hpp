
#ifndef ORTIN_BUSARM9_HPP
#define ORTIN_BUSARM9_HPP

#define ARM946E_DISABLE_FIQ

#include "types.hpp"
#include "busshared.hpp"
#include "ppu.hpp"
#include "arm946e/arm946e.hpp"

class BusARM9 {
public:
	// Connected components
	ARM946E<BusARM9> cpu;
	PPU& ppu;
	BusShared& shared;
	u8 *itcm;

	// For normal use
	std::stringstream& log;

	BusARM9(BusShared &shared, PPU &ppu, std::stringstream &log);
	~BusARM9();
	void reset();

	// For CPU and memory
	i64 delay;
	u8 *readTable[0x4000];
	u8 *readTable8[0x4000];
	u8 *writeTable[0x4000];

	void refreshVramPages();

	void hacf();
	template <typename T, bool code> T read(u32 address, bool sequential);
	template <typename T> void write(u32 address, T value, bool sequential);
	void iCycle(int cycles);

	u8 readIO(u32 address);
	void writeIO(u32 address, u8 value);

	// Coprocessor
	u32 coprocessorRead(u32 copNum, u32 copOpc, u32 copSrcDestReg, u32 copOpReg, u32 copOpcType);
	void coprocessorWrite(u32 copNum, u32 copOpc, u32 copSrcDestReg, u32 copOpReg, u32 copOpcType, u32 value);
};

#endif //ORTIN_BUSARM9_HPP
