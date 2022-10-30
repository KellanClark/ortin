#ifndef ORTIN_RTC_HPP
#define ORTIN_RTC_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"

#include <ctime>

class RTC {
public:
	std::shared_ptr<BusShared> shared;

	// External Use
	RTC(std::shared_ptr<BusShared> shared);
	~RTC();
	void reset();

	// Internal Use
	bool logRtc;
	int bitsSent;
	u64 sentData;
	u64 readBuf;
	u64 rtcTime;

	void syncToRealTime(time_t *tt);
	template <bool scheduled>
	void refresh();

	// Memory Interface
	u8 readIO7();
	void writeIO7(u8 value);

	// I/O Registers
	union {
		struct {
			u8 data : 1;
			u8 clockOut : 1;
			u8 selectOut : 1;
			u8 : 1;
			u8 dataDirection : 1;
			u8 clockDirection : 1;
			u8 selectDirection : 1;
			u8 : 1;
		};
		u8 bus; // NDS7 - 0x4000138
	};

	// RTC Registers
	union {
		struct {
			u8 parameterReadWrite : 1;
			u8 command : 3;
			u8 : 4; // Fixed code 0110
		};
		u8 commandRegister;
	};

	union {
		struct {
			u8 resetBit : 1;
			u8 hourMode : 1;
			u8 : 2;
			u8 interrupt1Flag : 1;
			u8 interrupt2Flag : 1;
			u8 powerLowFlag : 1;
			u8 powerOffFlag : 1;
		};
		u8 statusRegister1; // Command 0
	};

	union {
		struct {
			u8 interrupt1Mode : 4;
			u8 : 2;
			u8 interrupt2Mode : 1;
			u8 testMode : 1;
		};
		u8 statusRegister2; // Command 1
	};

	union {
		struct {
			u64 year : 8;
			u64 month : 5;
			u64 : 3;
			u64 day : 6;
			u64 : 2;
			u64 weekday : 3;
			u64 : 5;
			u64 hour : 6;
			u64 pmAm : 1;
			u64 : 1;
			u64 minute : 7;
			u64 : 1;
			u64 second : 7;
			u64 : 9;
		};
		u64 dateTimeRegister; // Command 2/3
	};

	struct {
		union {
			struct {
				u32 weekday : 3;
				u32 : 4;
				u32 dayCompareEnable : 1;
				u32 hour : 6;
				u32 pmAm : 1;
				u32 hourCompareEnable : 1;
				u32 minute : 7;
				u32 minuteCompareEnable : 1;
			};
			u32 raw;
		};
		u8 frequency; // Command 4/5
	} alarm1, alarm2;

	u8 clockAdjustmentRegister; // Command 6

	u8 freeRegister; // Command 7
};

#endif //ORTIN_RTC_HPP