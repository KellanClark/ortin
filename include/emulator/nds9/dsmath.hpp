#pragma once

#include "types.hpp"
#include "emulator/busshared.hpp"

class DSMath {
public:
	std::shared_ptr<BusShared> shared;

	// External Use
	DSMath(std::shared_ptr<BusShared> shared);
	~DSMath();
	void reset();

	// Memory Interface
	u8 readIO9(u32 address, bool final);
	void writeIO9(u32 address, u8 value, bool final);

	// I/O Registers
	union {
		struct {
			u16 divMode : 2;
			u16 : 12;
			u16 div0 : 1;
			u16 divBusy : 1;
		};
		u16 DIVCNT; // NDS9 - 0x4000280
	};
	i64 DIV_NUMER; // NDS9 - 0x4000290
	i64 DIV_DENOM; // NDS9 - 0x4000298
	i64 DIV_RESULT; // NDS9 - 0x40002A0
	i64 DIVREM_RESULT; // NDS9 - 0x40002A8

	union {
		struct {
			u16 sqrtMode : 1;
			u16 : 14;
			u16 sqrtBusy : 1;
		};
		u16 SQRTCNT; // NDS9 - 0x40002B0
	};
	u32 SQRT_RESULT; // NDS9 - 0x40002B4
	u64 SQRT_PARAM; // NDS9 - 0x40002B8

	u64 divFinishTimestamp;
	u64 sqrtFinishTimestamp;
};
