
#ifndef ORTIN_PPU_HPP
#define ORTIN_PPU_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"

class PPU {
public:
	BusShared& shared;
	std::stringstream& log;

	// External Use
	int frameCounter;
	bool updateScreen;
	uint16_t framebufferA[192][256];
	uint16_t framebufferB[192][256];
	bool vBlankIrq9, hBlankIrq9, vCounterIrq9;
	bool vBlankIrq7, hBlankIrq7, vCounterIrq7;

	PPU(BusShared &shared, std::stringstream &log);
	~PPU();
	void reset();

	// Types
	union VramInfoEntry {
		struct {
			u32 enableA : 1;
			u32 enableB : 1;
			u32 enableC : 1;
			u32 enableD : 1;
			u32 enableE : 1;
			u32 enableF : 1;
			u32 enableG : 1;
			u32 enableH : 1;
			u32 enableI : 1;
			u32 bankA : 3;
			u32 bankB : 3;
			u32 bankC : 3;
			u32 bankD : 3;
			u32 bankE : 2;
			u32 bankH : 1;
			u32 : 8;
		};
		u32 raw;
	};

	struct TileInfo {
		union {
			struct {
				u16 tileIndex : 10;
				u16 horizontalFlip : 1;
				u16 verticalFlip : 1;
				u16 paletteBank : 4;
			};
			u16 raw;
		};
	};

	struct Pixel {
		union {
			struct {
				u16 r : 5;
				u16 g : 5;
				u16 b : 5;
				u16 solid : 1;
			};
			u16 raw;
		};
	};

	// Events/Internal Function
	void lineStart();
	void hBlank();
	void drawLine();
	template <bool useEngineA, int layer> void draw2D();
	template <bool useEngineA, int layer, bool extended> void draw2DAffine();
	template <bool useEngineA> void combineLayers();

	// Memory Interface
	union {
		struct {
			Pixel engineABgPalette[0x100];
			Pixel engineAObjPalette[0x100];
			Pixel engineBBgPalette[0x100];
			Pixel engineBObjPalette[0x100];
		};
		u8 pram[0x800];
	};

	VramInfoEntry vramInfoTable[0x200];
	u8 *vramPageTable[0x200];
	u8 *vramA;
	u8 *vramB;
	u8 *vramC;
	u8 *vramD;
	u8 *vramE;
	u8 *vramF;
	u8 *vramG;
	u8 *vramH;
	u8 *vramI;
	u8 oam[0x800];

	void refreshVramPages();
	template <typename T, bool useEngineA, bool useObj> T readVram(u32 address);
	u8 readIO9(u32 address);
	void writeIO9(u32 address, u8 value);
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	// I/O Registers
	struct GraphicsEngine { // Engine B has a memory offset of 0x1000
		Pixel objBuf[256];

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

		struct {
			Pixel drawBuf[256];
			u32 screenBlockBaseAddress;
			u32 charBlockBaseAddress;

			union {
				struct {
					u16 priority : 2;
					u16 charBlock : 4;
					u16 mosaic : 1;
					u16 eightBitColor : 1;
					u16 screenBlock : 5;
					u16 extendedPaletteSlot : 1; // BG0/BG1
					u16 screenSize : 2;
				};
				struct {
					u16 : 13;
					u16 displayAreaWraparound : 1; // BG2/BG3
					u16 : 2;
				};
				u16 BGCNT; // NDS9 - 0x4000008/0x400000A/0x400000C/0x400000E
			};
			u16 BGHOFS; // NDS9 - 0x4000010/0x4000014/0x4000018/0x400001C
			u16 BGVOFS; // NDS9 - 0x4000012/0x4000016/0x400001A/0x400001E
			i16 BGPA; // NDS9 - BG2/BG3 - 0x4000020/0x4000030
			i16 BGPB; // NDS9 - BG2/BG3 - 0x4000022/0x4000032
			i16 BGPC; // NDS9 - BG2/BG3 - 0x4000024/0x4000034
			i16 BGPD; // NDS9 - BG2/BG3 - 0x4000026/0x4000036
			u32 BGX; // NDS9 - BG2/BG3 - 0x4000028/0x4000038
			u32 BGY; // NDS9 - BG2/BG3 - 0x400002C/0x400003C

			float internalBGX;
			float internalBGY;
		} bg[4];

		union {
			struct {
				u16 bgMosH : 4;
				u16 bgMosV : 4;
				u16 objMosH : 4;
				u16 objMosV : 4;
			};
			u16 MOSAIC; // NDS9 - 0x400004C
		};

		union {
			struct {
				u16 brightnessFactor : 5;
				u16 : 9;
				u16 brightnessMode : 2;
			};
			u16 MASTER_BRIGHT; // NDS9 - 0x400006C
		};
	} engineA, engineB;

	union {
		struct {
			u16 vBlankFlag9 : 1;
			u16 hBlankFlag9 : 1;
			u16 vCounterFlag9 : 1;
			u16 vBlankIrqEnable9 : 1;
			u16 hBlankIrqEnable9 : 1;
			u16 vCounterIrqEnable9 : 1;
			u16 : 1;
			u16 lycMsb9 : 1;
			u16 lycLsb9 : 8;
		};
		u16 DISPSTAT9; // NDS9 - 0x4000004
	};
	union {
		struct {
			u16 vBlankFlag7 : 1;
			u16 hBlankFlag7 : 1;
			u16 vCounterFlag7 : 1;
			u16 vBlankIrqEnable7 : 1;
			u16 hBlankIrqEnable7 : 1;
			u16 vCounterIrqEnable7 : 1;
			u16 : 1;
			u16 lycMsb7 : 1;
			u16 lycLsb7 : 8;
		};
		u16 DISPSTAT7; // NDS7 - 0x4000004
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
			u8 vramCMapped7 : 1;
			u8 vramDMapped7 : 1;
			u8 : 6;
		};
		u8 VRAMSTAT;
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
			u8 : 5;
			u8 vramHEnable : 1;
		};
		u8 VRAMCNT_H; // NDS9 - 0x4000248
	};

	union {
		struct {
			u8 vramIMst : 2;
			u8 : 5;
			u8 vramIEnable : 1;
		};
		u8 VRAMCNT_I; // NDS9 - 0x4000249
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
};


#endif //ORTIN_PPU_HPP
