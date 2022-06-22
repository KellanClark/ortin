
#ifndef ORTIN_PPU_HPP
#define ORTIN_PPU_HPP

#include "types.hpp"
#include "busshared.hpp"

class PPU {
public:
	BusShared& shared;
	std::stringstream& log;

	// External Use
	int frameCounter;
	bool updateScreen;
	uint16_t framebufferA[192][256];
	uint16_t framebufferB[192][256];

	PPU(BusShared &shared, std::stringstream &log);
	~PPU();
	void reset();

	// Events/Internal Function
	void lineStart();
	void hBlank();
	void drawLine();

	// Memory Interface
	u8 *vramA;
	u8 *vramB;
	u8 *vramC;
	u8 *vramD;
	u8 *vramE;
	u8 *vramF;
	u8 *vramG;
	u8 *vramH;
	u8 *vramI;

	u8 readIO9(u32 address);
	void writeIO9(u32 address, u8 value);
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	// I/O Registers
	union {
		struct {
			u16 vBlankFlag : 1;
			u16 hBlankFlag : 1;
			u16 vCounterFlag : 1;
			u16 vBlankIrqEnable : 1;
			u16 hBlankIrqEnable : 1;
			u16 vCounterIrqEnable : 1;
			u16 : 1;
			u16 lycMsb : 1;
			u16 lycLsb : 8;
		};
		u16 DISPSTAT; // 0x4000004
	};

	union {
		struct {
			u16 currentScanline : 9;
			u16 : 6;
		};
		u16 VCOUNT; // 0x4000006
	};

	union {
		struct {
			u8 vramAMst : 2;
			u8 : 1;
			u8 vramAOffset : 2;
			u8 : 2;
			u8 vramAEnable : 1;
		};
		u8 VRAMCNT_A; // NDS9 - 0x4000240
	};

	union {
		struct {
			u8 vramBMst : 2;
			u8 : 1;
			u8 vramBOffset : 2;
			u8 : 2;
			u8 vramBEnable : 1;
		};
		u8 VRAMCNT_B; // NDS9 - 0x4000241
	};

	union {
		struct {
			u8 vramCMst : 3;
			u8 vramCOffset : 2;
			u8 : 2;
			u8 vramCEnable : 1;
		};
		u8 VRAMCNT_C; // NDS9 - 0x4000242
	};

	union {
		struct {
			u8 vramDMst : 3;
			u8 vramDOffset : 2;
			u8 : 2;
			u8 vramDEnable : 1;
		};
		u8 VRAMCNT_D; // NDS9 - 0x4000243
	};

	union {
		struct {
			u8 vramEMst : 3;
			u8 : 4;
			u8 vramEEnable : 1;
		};
		u8 VRAMCNT_E; // NDS9 - 0x4000244
	};

	union {
		struct {
			u8 vramFMst : 3;
			u8 vramFOffset : 2;
			u8 : 2;
			u8 vramFEnable : 1;
		};
		u8 VRAMCNT_F; // NDS9 - 0x4000245
	};

	union {
		struct {
			u8 vramGMst : 3;
			u8 vramGOffset : 2;
			u8 : 2;
			u8 vramGEnable : 1;
		};
		u8 VRAMCNT_G; // NDS9 - 0x4000246
	};

	union {
		struct {
			u8 vramHMst : 2;
			u8 : 4;
			u8 vramHEnable : 1;
		};
		u8 VRAMCNT_H; // NDS9 - 0x4000247
	};

	union {
		struct {
			u8 vramIMst : 2;
			u8 : 4;
			u8 vramIEnable : 1;
		};
		u8 VRAMCNT_I; // NDS9 - 0x4000248
	};

	union {
		struct {
			u16 lcdPower : 1; // TODO: No info
			u16 engineAPower : 1;
			u16 rendering3DPower : 1;
			u16 geometry3DPower : 1;
			u16 : 5;
			u16 engineBPower : 1;
			u16 : 5;
			u16 displaySwap : 1;
		};
		u16 POWCNT1; // NDS9 - 0x4000304
	};

	struct {
		union {
			struct {
				u32 bgMode : 3;
				u32 bg03d : 1;
				u32 tileObjMapping : 1;
				u32 bitmapObjDimension : 1;
				u32 bitmapObjMapping : 1;
				u32 forcedBlank : 1;
				u32 displayBg0 : 1;
				u32 displayBg1 : 1;
				u32 displayBg2 : 1;
				u32 displayBg3 : 1;
				u32 displayBgObj : 1;
				u32 win0Enable : 1;
				u32 win1Enable : 1;
				u32 winObjEnable : 1;
				u32 displayMode : 2;
				u32 vramBlock : 2;
				u32 tileObjBoundary : 2;
				u32 bitmapObjBoundary : 1;
				u32 hBlankIntervalFree : 1;
				u32 charBase : 3;
				u32 screenBase : 3;
				u32 bgExtendedPalette : 1;
				u32 objExtendedPalette : 1;
			};
			u32 DISPCNT; // NDS9 - 0x4000000
		};
	} engineA;

	struct {
		union {
			struct {
				u32 bgMode : 3;
				u32 : 1;
				u32 tileObjMapping : 1;
				u32 bitmapObjDimension : 1;
				u32 bitmapObjMapping : 1;
				u32 forcedBlank : 1;
				u32 displayBg0 : 1;
				u32 displayBg1 : 1;
				u32 displayBg2 : 1;
				u32 displayBg3 : 1;
				u32 displayBgObj : 1;
				u32 win0Enable : 1;
				u32 win1Enable : 1;
				u32 winObjEnable : 1;
				u32 displayMode : 2;
				u32 : 2;
				u32 tileObjBoundary : 2;
				u32 : 1;
				u32 hBlankIntervalFree : 1;
				u32 : 6;
				u32 bgExtendedPalette : 1;
				u32 objExtendedPalette : 1;
			};
			u32 DISPCNT; // NDS9 - 0x4001000
		};
	} engineB;
};


#endif //ORTIN_PPU_HPP
