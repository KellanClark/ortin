#ifndef ORTIN_APU_HPP
#define ORTIN_APU_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"

class APU {
public:
	BusShared &shared;
	std::stringstream &log;

	// External Use
	APU(BusShared &shared, std::stringstream &log);
	~APU();
	void reset();

	// Memory Interface
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	// I/O Registers
	struct AudioChannel {
		union {
			struct {
				u32 volumeMul : 7;
				u32 : 1;
				u32 volumeDiv : 2;
				u32 : 5;
				u32 hold : 1;
				u32 panning : 7;
				u32 : 1;
				u32 waveDuty : 3;
				u32 repeatMode : 2;
				u32 format : 2;
				u32 start : 1;
			};
			u32 SOUNDCNT; // NDS7 - 0x40004x0
		};
		u32 SOUNDSAD; // NDS7 - 0x40004x4
		u16 SOUNDTMR; // NDS7 - 0x40004x8
		u16 SOUNDPNT; // NDS7 - 0x40004xA
		u32 SOUNDLEN; // NDS7 - 0x40004xC
	} channel[16];
	union {
		struct {
			u16 masterVolume : 7;
			u16 : 1;
			u16 leftOutputSource : 2;
			u16 rightOutputSource : 2;
			u16 ch1ToMixer : 1;
			u16 ch3ToMixer : 1;
			u16 : 1;
			u16 masterEnable : 1;
		};
		u16 SOUNDCNT; // NDS7 - 0x4000500
	};
	u16 SOUNDBIAS; // NDS7 - 0x4000504
	union {
		struct {
			u8 cap0AddMode : 1;
			u8 cap0Source : 1;
			u8 cap0Repeat : 1;
			u8 cap0Format : 1;
			u8 : 3;
			u8 cap0Start : 1;
		};
		u8 SNDCAP0CNT; // NDS7 - 0x4000508
	};
	union {
		struct {
			u8 cap1AddMode : 1;
			u8 cap1Source : 1;
			u8 cap1Repeat : 1;
			u8 cap1Format : 1;
			u8 : 3;
			u8 cap1Start : 1;
		};
		u8 SNDCAP1CNT; // NDS7 - 0x4000509
	};
	u32 SNDCAP0DAD; // NDS7 - 0x4000510
	u16 SNDCAP0LEN; // NDS7 - 0x4000514
	u32 SNDCAP1DAD; // NDS7 - 0x4000518
	u16 SNDCAP1LEN; // NDS7 - 0x400051C
};

#endif //ORTIN_APU_HPP