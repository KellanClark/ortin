#pragma once

#include "types.hpp"
#include "emulator/busshared.hpp"

class PPU {
public:
	std::shared_ptr<BusShared> shared;

	// External Use
	int frameCounter;
	bool updateScreen;
	uint16_t framebufferA[192][256];
	uint16_t framebufferB[192][256];
	bool vBlankIrq9, hBlankIrq9, vCounterIrq9;
	bool vBlankIrq7, hBlankIrq7, vCounterIrq7;

	PPU(std::shared_ptr<BusShared> shared);
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
			u32 bankF : 1; // Only used for extended palettes
			u32 bankG : 1; // Only used for extended palettes
			u32 bankH : 2; // Top bit only used for extended palettes
			u32 : 5;
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

	struct __attribute__ ((packed)) Object {
		union {
			struct {
				u16 objY : 8;
				u16 objMode : 2;
				u16 gfxMode : 2;
				u16 mosaic : 1;
				u16 eightBitColor : 1;
				u16 shape : 2;
			};
			u16 attribute0;
		};
		union {
			struct {
				u16 objX : 9;
				u16 : 3;
				u16 horizontalFlip : 1;
				u16 verticalFlip : 1;
				u16 size : 2;
			};
			struct {
				u16 : 9;
				u16 affineIndex : 5;
				u16 : 2;
			};
			u16 attribute1;
		};
		union {
			struct {
				u16 tileIndex : 10;
				u16 priority : 2;
				u16 paletteBank : 4;
			};
			u16 attribute2;
		};
		u16 unused;
	};
	struct __attribute__ ((packed)) ObjectMatrix {
		u16 un1, un2, un3;
		i16 pa;
		u16 un4, un5, un6;
		i16 pb;
		u16 un7, un8, un9;
		i16 pc;
		u16 un10, un11, un12;
		i16 pd;
	};

	// Events/Internal Function
	void lineStart(); // Timed events
	void hBlank();
	void drawLine();
	template <bool useEngineA, int layer> void draw2D(); // Drawing
	template <bool useEngineA, int layer, bool extended> void draw2DAffine();
	template <bool useEngineA> void drawObjects();
	template <bool useEngineA, int layer> bool inWindow(int x);
	template <bool useEngineA> void combineLayers(); // Merging
	template <bool useEngineA> void calculateWindowMasks();

	// Memory Interface
	union { // Palette RAM
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
	u8 *vramAll; // VRAM is allocated as one big contiguous block
	u8 *vramA, *vramB, *vramC, *vramD, *vramE, *vramF, *vramG, *vramH, *vramI; // Each bank is an offset into that big block

	VramInfoEntry epramInfoTable[10]; // For extended palettes (0-3 = engine A BG, 4 = engine A OBJ, 5-8 = engine B BG, 9 = engine B OBJ)
	u8 *epramPageTable[10];

	union {
		struct {
			struct {
				union {
					Object objects[128];
					ObjectMatrix objectMatrices[32];
				};
			} oamA, oamB;
		};
		u8 oam[0x800];
	};

	void refreshVramPages();
	template <typename T, bool useEngineA, bool useObj> T readVram(u32 address);
	template <bool useEngineA, bool useObj> u16 readExtendedPalette(int slot, u32 index);
	u8 readIO9(u32 address);
	void writeIO9(u32 address, u8 value);
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	// I/O Registers
	struct GraphicsEngine { // Engine B has a memory offset of 0x1000
		struct {
			bool objWin;
			bool mosaic;
			bool semiTransparent;
			int priority;
			Pixel pix;
		} objInfoBuf[256];

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
				u16 win0Right : 8;
				u16 win0Left : 8;
			};
			u16 WIN0H; // NDS9 - 0x4000040
		};

		union {
			struct {
				u16 win1Right : 8;
				u16 win1Left : 8;
			};
			u16 WIN1H; // NDS9 - 0x4000042
		};

		union {
			struct {
				u16 win0Bottom : 8;
				u16 win0Top : 8;
			};
			u16 WIN0V; // NDS9 - 0x4000044
		};

		union {
			struct {
				u16 win1Bottom : 8;
				u16 win1Top : 8;
			};
			u16 WIN1V; // NDS9 - 0x4000046
		};

		union {
			struct {
				u16 win0Bg0Enable : 1;
				u16 win0Bg1Enable : 1;
				u16 win0Bg2Enable : 1;
				u16 win0Bg3Enable : 1;
				u16 win0ObjEnable : 1;
				u16 win0BldEnable : 1;
				u16 : 2;
				u16 win1Bg0Enable : 1;
				u16 win1Bg1Enable : 1;
				u16 win1Bg2Enable : 1;
				u16 win1Bg3Enable : 1;
				u16 win1ObjEnable : 1;
				u16 win1BldEnable : 1;
				u16 : 2;
			};
			u16 WININ; // NDS9 - 0x4000048
		};

		union {
			struct {
				u16 winOutBg0Enable : 1;
				u16 winOutBg1Enable : 1;
				u16 winOutBg2Enable : 1;
				u16 winOutBg3Enable : 1;
				u16 winOutObjEnable : 1;
				u16 winOutBldEnable : 1;
				u16 : 2;
				u16 winObjBg0Enable : 1;
				u16 winObjBg1Enable : 1;
				u16 winObjBg2Enable : 1;
				u16 winObjBg3Enable : 1;
				u16 winObjObjEnable : 1;
				u16 winObjBldEnable : 1;
				u16 : 2;
			};
			u16 WINOUT; // NDS9 - 0x400004A
		};

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

		bool win0VerticalMatch, win1VerticalMatch;
		std::bitset<256> win0Mask, win1Mask, winObjMask, win0EffectiveMask, win1EffectiveMask, winOutMask;
		bool windowMasksDirty;
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
