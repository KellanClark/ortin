#include "emulator/ppu.hpp"

#include <cmath>

#define convertColor(x) ((x) | 0x8000)
#define VRAM_SIZE ((128 + 128 + 128 + 128 + 64 + 16 + 16 + 32 + 16) * 1024)

static constexpr u32 toPage(u32 address) {
	return address >> 14; // Pages are 16KB
}

PPU::PPU(std::shared_ptr<BusShared> shared) : shared(shared) {
	vramAll = new u8[VRAM_SIZE];
	vramA = vramAll; // 128KB
	vramB = vramA + 0x20000; // 128KB
	vramC = vramB + 0x20000; // 128KB
	vramD = vramC + 0x20000; // 128KB
	vramE = vramD + 0x20000; // 64KB
	vramF = vramE + 0x10000; // 16KB
	vramG = vramF + 0x4000; // 16KB
	vramH = vramG + 0x4000; // 32KB
	vramI = vramH + 0x8000; // 16KB
}

PPU::~PPU() {
	delete[] vramAll;
}

void PPU::reset() {
	vBlankIrq9 = hBlankIrq9 = vCounterIrq9 = false;
	vBlankIrq7 = hBlankIrq7 = vCounterIrq7 = false;

	memset(framebufferA, 0xFF, sizeof(framebufferA));
	memset(framebufferB, 0xFF, sizeof(framebufferB));

	memset(vramInfoTable, 0, sizeof(vramInfoTable));
	memset(vramPageTable, 0, sizeof(vramPageTable));
	memset(pram, 0, 0x800);
	memset(vramA, 0, 0x20000);
	memset(vramB, 0, 0x20000);
	memset(vramC, 0, 0x20000);
	memset(vramD, 0, 0x20000);
	memset(vramE, 0, 0x10000);
	memset(vramF, 0, 0x4000);
	memset(vramG, 0, 0x4000);
	memset(vramH, 0, 0x8000);
	memset(vramI, 0, 0x4000);
	memset(oam, 0, 0x800);

	engineA.DISPCNT = engineB.DISPCNT = 0;
	DISPSTAT9 = DISPSTAT7 = 0;
	VCOUNT = 0;
	engineA.screenBase = engineB.screenBase = 0;
	engineA.charBase = engineB.charBase = 0;
	engineA.bg[0].BGCNT = engineA.bg[1].BGCNT = engineA.bg[2].BGCNT = engineA.bg[3].BGCNT = engineB.bg[0].BGCNT = engineB.bg[1].BGCNT = engineB.bg[2].BGCNT = engineB.bg[3].BGCNT = 0;
	engineA.bg[0].BGHOFS = engineA.bg[1].BGHOFS = engineA.bg[2].BGHOFS = engineA.bg[3].BGHOFS = engineB.bg[0].BGHOFS = engineB.bg[1].BGHOFS = engineB.bg[2].BGHOFS = engineB.bg[3].BGHOFS = 0;
	engineA.bg[0].BGVOFS = engineA.bg[1].BGVOFS = engineA.bg[2].BGVOFS = engineA.bg[3].BGVOFS = engineB.bg[0].BGVOFS = engineB.bg[1].BGVOFS = engineB.bg[2].BGVOFS = engineB.bg[3].BGVOFS = 0;
	engineA.bg[2].BGPA = engineA.bg[3].BGPA = engineB.bg[2].BGPA = engineB.bg[3].BGPA = 0;
	engineA.bg[2].BGPB = engineA.bg[3].BGPB = engineB.bg[2].BGPB = engineB.bg[3].BGPB = 0;
	engineA.bg[2].BGPC = engineA.bg[3].BGPC = engineB.bg[2].BGPC = engineB.bg[3].BGPC = 0;
	engineA.bg[2].BGPD = engineA.bg[3].BGPD = engineB.bg[2].BGPD = engineB.bg[3].BGPD = 0;
	engineA.bg[2].BGX = engineA.bg[3].BGX = engineB.bg[2].BGX = engineB.bg[3].BGX = 0;
	engineA.bg[2].internalBGX = engineA.bg[3].internalBGX = engineB.bg[2].internalBGX = engineB.bg[3].internalBGX = 0;
	engineA.bg[2].BGY = engineA.bg[3].BGY = engineB.bg[2].BGY = engineB.bg[3].BGY = 0;
	engineA.bg[2].internalBGY = engineA.bg[3].internalBGY = engineB.bg[2].internalBGY = engineB.bg[3].internalBGY = 0;
	engineA.WIN0H = engineA.WIN1H = engineA.WIN0V = engineA.WIN1V = engineB.WIN0H = engineB.WIN1H = engineB.WIN0V = engineB.WIN1V = 0;
	engineA.WININ = engineA.WINOUT = engineB.WININ = engineB.WINOUT = 0;
	engineA.MOSAIC = engineB.MOSAIC = 0;
	engineA.MASTER_BRIGHT = engineB.MASTER_BRIGHT = 0;

	VRAMSTAT = 0;
	VRAMCNT_A = VRAMCNT_B = VRAMCNT_C = VRAMCNT_D = VRAMCNT_E = VRAMCNT_F = VRAMCNT_G = VRAMCNT_H = VRAMCNT_I = 0;

	POWCNT1 = 0;

	// > Dot clock = 5.585664 MHz (=33.513982 MHz / 6)
	// > H-Timing: 256 dots visible, 99 dots blanking, 355 dots total (15.7343KHz)
	// Line length = (256 + 99) * 6 * 2 = 4,260
	shared->addEvent(4260, EventType::PPU_LINE_START);
	shared->addEvent(3072, EventType::PPU_HBLANK);
}

void PPU::lineStart() {
	shared->addEvent(4260, EventType::PPU_LINE_START);

	++currentScanline;
	switch (currentScanline) {
	case 192: // VBlank
		updateScreen = true;
		vBlankFlag9 = vBlankFlag7 = true;

		if (vBlankIrqEnable9)
			vBlankIrq9 = true;
		if (vBlankIrqEnable7)
			vBlankIrq7 = true;
		break;
	case 262:
		// > Similar as on GBA, the VBlank flag isn't set in the last line (ie. only in lines 192..261, but not in line 262).
		vBlankFlag9 = vBlankFlag7 = false;
		break;
	case 263: // Start of Frame
		++frameCounter;
		currentScanline = 0;

		engineA.bg[2].internalBGX = (float)((i32)(engineA.bg[2].BGX << 4) >> 4) / 256;
		engineA.bg[2].internalBGY = (float)((i32)(engineA.bg[2].BGY << 4) >> 4) / 256;
		engineA.bg[3].internalBGX = (float)((i32)(engineA.bg[3].BGX << 4) >> 4) / 256;
		engineA.bg[3].internalBGY = (float)((i32)(engineA.bg[3].BGY << 4) >> 4) / 256;
		engineB.bg[2].internalBGX = (float)((i32)(engineB.bg[2].BGX << 4) >> 4) / 256;
		engineB.bg[2].internalBGY = (float)((i32)(engineB.bg[2].BGY << 4) >> 4) / 256;
		engineB.bg[3].internalBGX = (float)((i32)(engineB.bg[3].BGX << 4) >> 4) / 256;
		engineB.bg[3].internalBGY = (float)((i32)(engineB.bg[3].BGY << 4) >> 4) / 256;

		//memset(engineA.objInfoBuf, 0, sizeof(engineA.objInfoBuf));
		//memset(engineB.objInfoBuf, 0, sizeof(engineB.objInfoBuf));
		break;
	}

	if (((lycMsb9 << 8) | lycLsb9) == currentScanline) {
		vCounterFlag9 = vCounterFlag7 = true;

		if (vCounterIrqEnable9)
			vCounterIrq9 = true;
		if (vCounterIrqEnable7)
			vCounterIrq7 = true;
	} else {
		vCounterFlag9 = vCounterFlag7 = false;
	}

	// Window
	calculateWindowMasks<true>();
	calculateWindowMasks<false>();
}

void PPU::hBlank() {
	shared->addEvent(4260, EventType::PPU_HBLANK);

	hBlankFlag9 = hBlankFlag7 = true;
	if (hBlankIrqEnable9)
		hBlankIrq9 = true;
	if (hBlankIrqEnable7)
		hBlankIrq7 = true;

	if (currentScanline < 192) {
		drawLine();
	}
	if ((currentScanline < 192) || (currentScanline == 262)) {
		memset(engineA.objInfoBuf, 0, sizeof(engineA.objInfoBuf));
		if (engineA.displayMode == 1 && engineA.displayBgObj)
			drawObjects<true>();
		memset(engineB.objInfoBuf, 0, sizeof(engineB.objInfoBuf));
		if (engineB.displayMode == 1 && engineB.displayBgObj)
			drawObjects<false>();
	}
}

void PPU::drawLine() {
	/* Engine A */
	switch (engineA.displayMode) {
	case 3: // Main Memory Display
		printf("shit\n");
	case 0: // Display off
		for (int i = 0; i < 256; i++)
			framebufferA[currentScanline][i] = 0xFFFF;
		break;
	case 1: // Graphics Display
		// Clear buffers
		for (int i = 0; i < 256; i++)
			engineA.bg[0].drawBuf[i].raw = engineA.bg[1].drawBuf[i].raw = engineA.bg[2].drawBuf[i].raw = engineA.bg[3].drawBuf[i].raw = 0;

		// BG modes
		switch (engineA.bgMode) {
		case 0:
			if (engineA.displayBg0) draw2D<true, 0>();
			if (engineA.displayBg1) draw2D<true, 1>();
			if (engineA.displayBg2) draw2D<true, 2>();
			if (engineA.displayBg3) draw2D<true, 3>();
			break;
		case 1:
			if (engineA.displayBg0) draw2D<true, 0>();
			if (engineA.displayBg1) draw2D<true, 1>();
			if (engineA.displayBg2) draw2D<true, 2>();
			if (engineA.displayBg3) draw2DAffine<true, 3, false>();
			break;
		case 2:
			if (engineA.displayBg0) draw2D<true, 0>();
			if (engineA.displayBg1) draw2D<true, 1>();
			if (engineA.displayBg2) draw2DAffine<true, 2, false>();
			if (engineA.displayBg3) draw2DAffine<true, 3, false>();
			break;
		case 3:
			if (engineA.displayBg0) draw2D<true, 0>();
			if (engineA.displayBg1) draw2D<true, 1>();
			if (engineA.displayBg2) draw2D<true, 2>();
			if (engineA.displayBg3) draw2DAffine<true, 3, true>();
			break;
		case 4:
			if (engineA.displayBg0) draw2D<true, 0>();
			if (engineA.displayBg1) draw2D<true, 1>();
			if (engineA.displayBg2) draw2DAffine<true, 2, false>();
			if (engineA.displayBg3) draw2DAffine<true, 3, true>();
			break;
		case 5:
			if (engineA.displayBg0) draw2D<true, 0>();
			if (engineA.displayBg1) draw2D<true, 1>();
			if (engineA.displayBg2) draw2DAffine<true, 2, true>();
			if (engineA.displayBg3) draw2DAffine<true, 3, true>();
			break;
		case 6:
			if (engineA.displayBg2) draw2DAffine<true, 2, true>();
			break;
		}

		// Combine everything
		combineLayers<true>();
		break;
	case 2: // VRAM Display
		u16 *bank;
		switch (engineA.vramBlock) {
		case 0: bank = (u16 *)vramA; break;
		case 1: bank = (u16 *)vramB; break;
		case 2: bank = (u16 *)vramC; break;
		case 3: bank = (u16 *)vramD; break;
		}

		for (int i = 0; i < 256; i++)
			framebufferA[currentScanline][i] = convertColor(bank[(currentScanline * 256) + i]);
		break;
	}

	/* Engine B */
	if (engineB.displayMode == 1) { // Graphics Display
		// Clear buffers
		for (int i = 0; i < 256; i++) {
			engineB.bg[0].drawBuf[i].raw = 0;
			engineB.bg[1].drawBuf[i].raw = 0;
			engineB.bg[2].drawBuf[i].raw = 0;
			engineB.bg[3].drawBuf[i].raw = 0;
		}

		// BG modes
		switch (engineB.bgMode) {
		case 0:
			if (engineB.displayBg0) draw2D<false, 0>();
			if (engineB.displayBg1) draw2D<false, 1>();
			if (engineB.displayBg2) draw2D<false, 2>();
			if (engineB.displayBg3) draw2D<false, 3>();
			break;
		case 1:
			if (engineB.displayBg0) draw2D<false, 0>();
			if (engineB.displayBg1) draw2D<false, 1>();
			if (engineB.displayBg2) draw2D<false, 2>();
			if (engineB.displayBg3) draw2DAffine<false, 3, false>();
			break;
		case 2:
			if (engineB.displayBg0) draw2D<false, 0>();
			if (engineB.displayBg1) draw2D<false, 1>();
			if (engineB.displayBg2) draw2DAffine<false, 2, false>();
			if (engineB.displayBg3) draw2DAffine<false, 3, false>();
			break;
		case 3:
			if (engineB.displayBg0) draw2D<false, 0>();
			if (engineB.displayBg1) draw2D<false, 1>();
			if (engineB.displayBg2) draw2D<false, 2>();
			if (engineB.displayBg3) draw2DAffine<false, 3, true>();
			break;
		case 4:
			if (engineB.displayBg0) draw2D<false, 0>();
			if (engineB.displayBg1) draw2D<false, 1>();
			if (engineB.displayBg2) draw2DAffine<false, 2, false>();
			if (engineB.displayBg3) draw2DAffine<false, 3, true>();
			break;
		case 5:
			if (engineB.displayBg0) draw2D<false, 0>();
			if (engineB.displayBg1) draw2D<false, 1>();
			if (engineB.displayBg2) draw2DAffine<false, 2, true>();
			if (engineB.displayBg3) draw2DAffine<false, 3, true>();
			break;
		}

		// Combine everything
		combineLayers<false>();
	} else { // Display off
		for (int i = 0; i < 256; i++)
			framebufferB[currentScanline][i] = 0xFFFF;
	}

	/* Master Bright Pass */
	if (engineA.brightnessMode == 1) { // Up
		float multiplier = (float)((engineA.brightnessFactor > 16) ? 16 : engineA.brightnessFactor) / 16;
		for (int i = 0; i < 256; i++) {
			Pixel &pix = *(Pixel *)&framebufferA[currentScanline][i];

			pix.r = pix.r + ((63 - pix.r) * multiplier);
			pix.g = pix.g + ((63 - pix.g) * multiplier);
			pix.b = pix.b + ((63 - pix.b) * multiplier);
		}
	} else if (engineA.brightnessMode == 2) { // Down
		float multiplier = (float)((engineA.brightnessFactor > 16) ? 16 : engineA.brightnessFactor) / 16;
		for (int i = 0; i < 256; i++) {
			Pixel &pix = *(Pixel *)&framebufferA[currentScanline][i];

			pix.r = pix.r - (pix.r * multiplier);
			pix.g = pix.g - (pix.g * multiplier);
			pix.b = pix.b - (pix.b * multiplier);
		}
	}

	if (engineB.brightnessMode == 1) { // Up
		float multiplier = (float)((engineB.brightnessFactor > 16) ? 16 : engineB.brightnessFactor) / 16;
		for (int i = 0; i < 256; i++) {
			Pixel &pix = *(Pixel *)&framebufferB[currentScanline][i];

			pix.r = pix.r + ((63 - pix.r) * multiplier);
			pix.g = pix.g + ((63 - pix.g) * multiplier);
			pix.b = pix.b + ((63 - pix.b) * multiplier);
		}
	} else if (engineB.brightnessMode == 2) { // Down
		float multiplier = (float)((engineB.brightnessFactor > 16) ? 16 : engineB.brightnessFactor) / 16;
		for (int i = 0; i < 256; i++) {
			Pixel &pix = *(Pixel *)&framebufferB[currentScanline][i];

			pix.r = pix.r - (pix.r * multiplier);
			pix.g = pix.g - (pix.g * multiplier);
			pix.b = pix.b - (pix.b * multiplier);
		}
	}

	/* Update Affine Registers */
	engineA.bg[2].internalBGX += (float)engineA.bg[2].BGPB / 256;
	engineA.bg[2].internalBGY += (float)engineA.bg[2].BGPD / 256;
	engineA.bg[3].internalBGX += (float)engineA.bg[3].BGPB / 256;
	engineA.bg[3].internalBGY += (float)engineA.bg[3].BGPD / 256;
	engineB.bg[2].internalBGX += (float)engineB.bg[2].BGPB / 256;
	engineB.bg[2].internalBGY += (float)engineB.bg[2].BGPD / 256;
	engineB.bg[3].internalBGX += (float)engineB.bg[3].BGPB / 256;
	engineB.bg[3].internalBGY += (float)engineB.bg[3].BGPD / 256;
}

template <bool useEngineA, int layer>
void PPU::draw2D() {
	GraphicsEngine& engine = useEngineA ? engineA : engineB;
	auto& bg = engine.bg[layer];

	int x = bg.BGHOFS;
	int y = currentScanline + bg.BGVOFS;

	if (bg.mosaic)
		y = y - (y % (engine.bgMosV + 1));

	for (int column = 0; column < 256; column++) {
		if (!inWindow<useEngineA, layer>(column))
			continue;

		u32 tileAddress;
		switch (bg.screenSize) {
		case 0: // 256x256
			tileAddress = bg.screenBlockBaseAddress + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
			break;
		case 1: // 512x256
			tileAddress = bg.screenBlockBaseAddress + ((x & 0x100) << 3) + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
			break;
		case 2: // 256x512
			tileAddress = bg.screenBlockBaseAddress + (((y % 512) / 8) * 64) + (((x % 256) / 8) * 2);
			break;
		case 3: // 512x512
			tileAddress = bg.screenBlockBaseAddress + ((x & 0x100) << 3) + (16 * (y & 0x100)) + (((y % 256) / 8) * 64) + (((x % 256) / 8) * 2);
			break;
		}

		TileInfo tile;
		Pixel color;
		tile.raw = readVram<u16, useEngineA, false>(tileAddress);

		int xMod = tile.horizontalFlip ? (7 - (x % 8)) : (x % 8);
		int yMod = tile.verticalFlip ? (7 - (y % 8)) : (y % 8);
		u8 tileData;
		if (bg.eightBitColor) { // 8 bits per pixel
			tileData = readVram<u8, useEngineA, false>(bg.charBlockBaseAddress + (tile.tileIndex * 64) + (yMod * 8) + xMod);

			if (engine.bgExtendedPalette) {
				color.raw = readExtendedPalette<useEngineA, false>(layer + (((layer <= 1) && bg.extendedPaletteSlot) ? 2 : 0), tileData);
			} else {
				color = (useEngineA ? engineABgPalette : engineBBgPalette)[tileData];
			}
		} else { // 4 bits per pixel
			tileData = readVram<u8, useEngineA, false>(bg.charBlockBaseAddress + (tile.tileIndex * 32) + (yMod * 4) + (xMod / 2));

			if (xMod & 1) {
				tileData >>= 4;
			} else {
				tileData &= 0xF;
			}

			color = (useEngineA ? engineABgPalette : engineBBgPalette)[(tile.paletteBank << 4) | tileData];
		}

		if (tileData != 0) {
			bg.drawBuf[column] = color;
			bg.drawBuf[column].solid = true;
		}

		if (bg.mosaic) {
			x = bg.BGHOFS + column;
			x = x - (x % (engine.bgMosH + 1));
		} else {
			++x;
		}
	}
}

template <bool useEngineA, int layer, bool extended>
void PPU::draw2DAffine() {
	GraphicsEngine& engine = useEngineA ? engineA : engineB;
	auto& bg = engine.bg[layer];

	bool largeBitmap = useEngineA && (layer == 2) && (engine.bgMode == 6);

	// > bgcnt size  text     rotscal    bitmap   large bmp
	// > 0           256x256  128x128    128x128  512x1024
	// > 1           512x256  256x256    256x256  1024x512
	// > 2           256x512  512x512    512x256  -
	// > 3           512x512  1024x1024  512x512  -
	// `text` isn't used here
	unsigned int screenSizeX;
	unsigned int screenSizeY;
	if (largeBitmap) { // `large bmp`
		if (bg.screenSize & 1) {
			screenSizeX = 1024;
			screenSizeY = 512;
		} else {
			screenSizeX = 512;
			screenSizeY = 1024;
		}
	} else if (extended && (bg.BGCNT & (1 << 7))) { // `bitmap`
		switch (bg.screenSize) {
		case 0: screenSizeX = 128; screenSizeY = 128; break;
		case 1: screenSizeX = 256; screenSizeY = 256; break;
		case 2: screenSizeX = 512; screenSizeY = 256; break;
		case 3: screenSizeX = 512; screenSizeY = 512; break;
		}
	} else { // `rotscal`
		screenSizeX = 128 << bg.screenSize;
		screenSizeY = 128 << bg.screenSize;
	}

	float affX = bg.internalBGX;
	float affY = bg.internalBGY;
	float pa = (float)bg.BGPA / 256;
	float pc = (float)bg.BGPC / 256;

	for (int column = 0; column < 256; column++, affX += pa, affY += pc) {
		if (!inWindow<useEngineA, layer>(column))
			continue;

		int x = bg.mosaic ? ((int)affX - ((int)affX % (engine.bgMosH + 1))) : (int)affX;
		int y = bg.mosaic ? ((int)affY - ((int)affY % (engine.bgMosV + 1))) : (int)affY;
		if (!bg.displayAreaWraparound && (((unsigned int)y >= screenSizeY) || ((unsigned int)x >= screenSizeX)))
			continue;

		if constexpr (extended) {
			// Save me, branch prediction.
			if (largeBitmap) { // Large screen bitmap
				u8 tileData = readVram<u8, useEngineA, false>((y * screenSizeX) + x);

				if (tileData != 0) {
					bg.drawBuf[column] = (useEngineA ? engineABgPalette : engineBBgPalette)[tileData];
					bg.drawBuf[column].solid = true;
				}
			} else if (bg.BGCNT & (1 << 7)) {
				if (bg.BGCNT & (1 << 2)) { // rot/scal direct color bitmap
					u16 tileData = readVram<u16, useEngineA, false>((bg.screenBlock * 0x4000) + (y * screenSizeX * 2) + (x * 2));

					// > However, the upper bit (Bit15) is used as Alpha flag. That is, Alpha=0=Transparent, Alpha=1=Normal (ie. on the NDS, Direct Color values 0..7FFFh are NOT displayed).
					// This conveniently matches how I store the Pixel struct
					bg.drawBuf[column].raw = tileData;
				} else { // rot/scal 256 color bitmap
					u8 tileData = readVram<u8, useEngineA, false>((bg.screenBlock * 0x4000) + (y * screenSizeX) + x);

					if (tileData != 0) {
						bg.drawBuf[column] = (useEngineA ? engineABgPalette : engineBBgPalette)[tileData];
						bg.drawBuf[column].solid = true;
					}
				}
			} else { // rot/scal with 16bit bgmap entries (Text+Affine mixup)
				TileInfo tile;
				Pixel color;
				tile.raw = readVram<u16, useEngineA, false>(bg.screenBlockBaseAddress + ((((y & (screenSizeY - 1)) / 8) * (screenSizeY / 8)) + ((x & (screenSizeX - 1)) / 8)) * 2);

				// Despite BGCNT.7 being 0, this mode uses 8 bit palette indexes because it's affine
				int xMod = tile.horizontalFlip ? (7 - (x % 8)) : (x % 8);
				int yMod = tile.verticalFlip ? (7 - (y % 8)) : (y % 8);
				u8 tileData = readVram<u8, useEngineA, false>(bg.charBlockBaseAddress + (tile.tileIndex * 64) + (yMod * 8) + xMod);

				if (engine.bgExtendedPalette) {
					color.raw = readExtendedPalette<useEngineA, false>(layer + (((layer <= 1) && bg.extendedPaletteSlot) ? 2 : 0), tileData);
				} else {
					color = (useEngineA ? engineABgPalette : engineBBgPalette)[tileData];
				}

				if (tileData != 0) {
					bg.drawBuf[column] = color;
					bg.drawBuf[column].solid = true;
				}
			}
		} else { // Normal affine
			int tile = readVram<u8, useEngineA, false>(bg.screenBlockBaseAddress + (((y & (screenSizeY - 1)) / 8) * (screenSizeY / 8)) + ((x & (screenSizeX - 1)) / 8));
			u8 tileData = readVram<u8, useEngineA, false>(bg.charBlockBaseAddress + (tile * 64) + ((y & 7) * 8) + (x & 7));

			if (tileData != 0) {
				bg.drawBuf[column] = (useEngineA ? engineABgPalette : engineBBgPalette)[tileData];
				bg.drawBuf[column].solid = true;
			}
		}
	}
}

static const unsigned int objSizeTable[4][4][2] = {
	{{ 8,  8}, {16, 16}, {32, 32}, {64, 64}},
	{{16,  8}, {32,  8}, {32, 16}, {64, 32}},
	{{ 8, 16}, { 8, 32}, {16, 32}, {32, 64}},
	{{ 0,  0}, { 0,  0}, { 0,  0}, { 0,  0}}
};

template <bool useEngineA>
void PPU::drawObjects() {
	GraphicsEngine& engine = useEngineA ? engineA : engineB;
	auto& objects = useEngineA ? oamA.objects : oamB.objects;
	auto& matrices = useEngineA ? oamA.objectMatrices : oamB.objectMatrices;
	const int realLine = (currentScanline == 262) ? 0 : currentScanline + 1;
	engine.winObjMask = 0;

	for (int priority = 3; priority >= 0; priority--) {
		for (int objNum = 127; objNum >= 0; objNum--) {
			Object& obj = objects[objNum];
			if ((obj.priority != priority) || (obj.objMode == 2))
				continue;

			const bool isAffine = obj.objMode != 0;
			const bool isDoubleSize = obj.objMode == 3;
			const bool isObjWindow = obj.gfxMode == 2;
			const bool isBitmap = obj.gfxMode == 3;

			unsigned int column = obj.objX;
			unsigned int line = realLine - obj.objY;
			unsigned int xSize = objSizeTable[obj.shape][obj.size][0];
			unsigned int ySize = objSizeTable[obj.shape][obj.size][1];
			unsigned int y = (obj.mosaic ? (realLine - (realLine % (engine.objMosV + 1))) : realLine) - obj.objY;
			if (obj.verticalFlip) y = ySize - 1 - y;

			// Initialize affine variables
			float affX, affY, pa, pb, pc, pd;
			ObjectMatrix& mat = matrices[obj.affineIndex];
			if (isAffine) {
				pa = (float)mat.pa / 256;
				pb = (float)mat.pb / 256;
				pc = (float)mat.pc / 256;
				pd = (float)mat.pd / 256;

				if (isDoubleSize) {
					affX = (pb * (line - (float)ySize)) + (pa * ((float)xSize * -1)) + ((float)xSize / 2);
					affY = (pd * (line - (float)ySize)) + (pc * ((float)xSize * -1)) + ((float)ySize / 2);
				} else {
					affX = (pb * (line - ((float)ySize / 2))) + (pa * ((float)xSize / -2)) + ((float)xSize / 2);
					affY = (pd * (line - ((float)ySize / 2))) + (pc * ((float)xSize / -2)) + ((float)ySize / 2);
				}
			}

			// Eliminate objects that aren't on this line
			if ((obj.objY + ySize) > 255) {
				if ((realLine < obj.objY) && (realLine >= (u8)(obj.objY + (ySize << isDoubleSize))))
					continue;
			} else {
				if ((realLine < obj.objY) || (realLine >= (obj.objY + (ySize << isDoubleSize))))
					continue;
			}

			// Draw loop
			for (unsigned int relX = 0; relX < (xSize << isDoubleSize); relX++) {
				if (!inWindow<useEngineA, 4>(column) && !isObjWindow)
					goto nextPixel;
				if (column >= 256)
					goto nextPixel;

				// Calculate X and Y
				unsigned int x;
				if (isAffine) {
					// Get X and Y
					x = floor(affX);
					y = floor(affY);
					if (obj.mosaic) {
						y = (realLine - (realLine % (engine.objMosV + 1))) - y;
					}

					// Only draw if part of the object
					if ((x >= xSize) || (y >= ySize))
						goto nextPixel;
				} else {
					if (obj.horizontalFlip) {
						x = xSize - 1 - relX;
					} else {
						x = relX;
					}
				}

				if (isBitmap) {
					//
				} else {
					u32 tileDataAddress;
					u8 tileData = 0;
					Pixel color;

					int xMod = x & 7;//obj.horizontalFlip ? (7 - (x % 8)) : (x % 8);
					if (obj.eightBitColor) { // 8 bits per pixel
						if (engine.tileObjMapping) { // 1D
							tileDataAddress = ((obj.tileIndex & ~1) * (32 << engine.tileObjBoundary)) + ((((y / 8) * (xSize / 8)) + (x / 8)) * 64) + ((y & 7) * 8) + xMod;
						} else { // 2D
							tileDataAddress = ((obj.tileIndex & ~1) * 32) + ((((y / 8) * 16) + (x / 8)) * 64) + ((y & 7) * 8) + xMod;
						}
						tileData = readVram<u8, useEngineA, true>(tileDataAddress);
						
						if (engine.objExtendedPalette) {
							color.raw = readExtendedPalette<useEngineA, true>(0, (obj.paletteBank << 8) | tileData);
						} else {
							color = (useEngineA ? engineAObjPalette : engineBObjPalette)[tileData];
						}
					} else { // 4 bits per pixel
						if (engine.tileObjMapping) { // 1D
							tileDataAddress = (obj.tileIndex * (32 << engine.tileObjBoundary)) + ((((y / 8) * (xSize / 8)) + (x / 8)) * 32) + ((y & 7) * 4) + (xMod / 2);
						} else { // 2D
							tileDataAddress = (obj.tileIndex * 32) + ((((y / 8) * 32) + (x / 8)) * 32) + ((y & 7) * 4) + (xMod / 2);
						}
						tileData = readVram<u8, useEngineA, true>(tileDataAddress);

						if (xMod & 1) {
							tileData >>= 4;
						} else {
							tileData &= 0xF;
						}

						color = (useEngineA ? engineAObjPalette : engineBObjPalette)[(obj.paletteBank << 4) | tileData];
					}

					if (tileData != 0) {
						if (isObjWindow) {
							engine.winObjMask[column] = true;
						} else {
							engine.objInfoBuf[column].pix = color;
							engine.objInfoBuf[column].pix.solid = true;
							engine.objInfoBuf[column].mosaic = obj.mosaic;
							engine.objInfoBuf[column].priority = priority;
						}
					}
				}

				nextPixel:
				if (isAffine) {
					affX += pa;
					affY += pc;
				}
				column = (column + 1) & 0x1FF;
			}
		}
	}
}

template <bool useEngineA, int layer> // Layer 4 is objects
bool PPU::inWindow(int x) {
	GraphicsEngine& engine = useEngineA ? engineA : engineB;

	if (engine.win0Enable || engine.win1Enable || engine.winObjEnable) {
		if (engine.win0EffectiveMask[x]) return (engine.WININ & (0x1 << layer));
		if (engine.win1EffectiveMask[x]) return (engine.WININ & (0x100 << layer));
		if (engine.winObjMask[x]) return (engine.WINOUT & (0x100 << layer));
		if (engine.winOutMask[x]) return (engine.WINOUT & (0x1 << layer));

		return false;
	} else {
		return true;
	}
}

template <bool useEngineA>
void PPU::combineLayers() {
	GraphicsEngine& engine = useEngineA ? engineA : engineB;

	// TODO: Blending

	// This can probably be optimized into some godforsaken mess of AVX
	for (int i = 0; i < 256; i++) {
		Pixel final = (useEngineA ? engineABgPalette : engineBBgPalette)[0];
		final.solid = true; // Just in case the graphics API needs it.
		int objBufIndex = engine.objInfoBuf[i].mosaic ? i - (i % (engine.objMosH + 1)) : i;

		for (int priority = 3; priority >= 0; priority--) {
			if ((engine.bg[3].priority == priority) && engine.bg[3].drawBuf[i].solid) final = engine.bg[3].drawBuf[i];
			if ((engine.bg[2].priority == priority) && engine.bg[2].drawBuf[i].solid) final = engine.bg[2].drawBuf[i];
			if ((engine.bg[1].priority == priority) && engine.bg[1].drawBuf[i].solid) final = engine.bg[1].drawBuf[i];
			if ((engine.bg[0].priority == priority) && engine.bg[0].drawBuf[i].solid) final = engine.bg[0].drawBuf[i];
			if ((engine.objInfoBuf[objBufIndex].priority == priority) && engine.objInfoBuf[objBufIndex].pix.solid) final = engine.objInfoBuf[objBufIndex].pix;
		}

		(useEngineA ? framebufferA : framebufferB)[currentScanline][i] = final.raw;
	}
}

template <bool useEngineA>
void PPU::calculateWindowMasks() {
	GraphicsEngine& engine = useEngineA ? engineA : engineB;

	if ((currentScanline & 0xFF) == engine.win0Top) engine.win0VerticalMatch = true;
	if ((currentScanline & 0xFF) == engine.win0Bottom) engine.win0VerticalMatch = false;
	if ((currentScanline & 0xFF) == engine.win1Top) engine.win1VerticalMatch = true;
	if ((currentScanline & 0xFF) == engine.win1Bottom) engine.win1VerticalMatch = false;

	if (engine.windowMasksDirty) {
		bool hMatch0 = false;
		bool hMatch1 = false;
		for (int i = 0; i < 256; i++) {
			if (i == engine.win0Left) hMatch0 = true;
			if (i == engine.win0Right) hMatch0 = false;
			engine.win0Mask[i] = hMatch0;

			if (i == engine.win1Left) hMatch1 = true;
			if (i == engine.win1Right) hMatch1 = false;
			engine.win1Mask[i] = hMatch1;
		}

		engine.windowMasksDirty = false;
	}

	engine.win0EffectiveMask = (engine.win0Enable && engine.win0VerticalMatch) ? engine.win0Mask : 0;
	engine.win1EffectiveMask = (engine.win1Enable && engine.win1VerticalMatch) ? engine.win1Mask : 0;
	engine.winObjMask = engine.winObjEnable ? engine.winObjMask : 0;

	engine.winOutMask = ~(engine.win0EffectiveMask | engine.win1EffectiveMask | engine.winObjMask);
}

void PPU::refreshVramPages() {
	// Clear the VRAM tables
	for (int i = 0; i < 0x200; i++) {
		vramInfoTable[i].raw = 0;
		vramPageTable[i] = NULL;
	}
	for (int i = 0; i < 10; i++) {
		epramInfoTable[i].raw = 0;
		epramPageTable[i] = NULL;
	}

	if (vramAEnable) {
		switch (vramAMst) {
		case 1: // 6000000h+(20000h*OFS)
			for (int i = 0; i < 8; i++) {
				vramInfoTable[(vramAOffset << 3) | i].enableA = true;
				vramInfoTable[(vramAOffset << 3) | i].bankA = i;
			}
			break;
		case 2: // 6400000h+(20000h*OFS.0)
			for (int i = 0; i < 8; i++) {
				vramInfoTable[toPage(0x400000) | ((vramAOffset & 1) << 3) | i].enableA = true;
				vramInfoTable[toPage(0x400000) | ((vramAOffset & 1) << 3) | i].bankA = i;
			}
			break;
		}
	}
	if (vramBEnable) {
		switch (vramBMst) {
		case 1: // 6000000h+(20000h*OFS)
			for (int i = 0; i < 8; i++) {
				vramInfoTable[(vramBOffset << 3) | i].enableB = true;
				vramInfoTable[(vramBOffset << 3) | i].bankB = i;
			}
			break;
		case 2: // 6400000h+(20000h*OFS.0)
			for (int i = 0; i < 8; i++) {
				vramInfoTable[toPage(0x400000) | ((vramBOffset & 1) << 3) | i].enableB = true;
				vramInfoTable[toPage(0x400000) | ((vramBOffset & 1) << 3) | i].bankB = i;
			}
			break;
		}
	}
	if (vramCEnable) {
		switch (vramCMst) {
		case 1: // 6000000h+(20000h*OFS)
			for (int i = 0; i < 8; i++) {
				vramInfoTable[(vramCOffset << 3) | i].enableC = true;
				vramInfoTable[(vramCOffset << 3) | i].bankC = i;
			}
			break;
		case 4: // 6200000h
			for (int i = 0; i < 8; i++) {
				vramInfoTable[toPage(0x200000) | i].enableC = true;
				vramInfoTable[toPage(0x200000) | i].bankC = i;
			}
			break;
		}
	}
	if (vramDEnable) {
		switch (vramDMst) {
		case 1: // 6000000h+(20000h*OFS)
			for (int i = 0; i < 8; i++) {
				vramInfoTable[(vramDOffset << 3) | i].enableD = true;
				vramInfoTable[(vramDOffset << 3) | i].bankD = i;
			}
			break;
		case 4: // 6600000h
			for (int i = 0; i < 8; i++) {
				vramInfoTable[toPage(0x600000) | i].enableD = true;
				vramInfoTable[toPage(0x600000) | i].bankD = i;
			}
			break;
		}
	}
	if (vramEEnable) {
		switch (vramEMst) {
		case 1: // 6000000h
			for (int i = 0; i < 4; i++) {
				vramInfoTable[i].enableE = true;
				vramInfoTable[i].bankE = i;
			}
			break;
		case 2: // 6400000h
			for (int i = 0; i < 4; i++) {
				vramInfoTable[toPage(0x400000) | i].enableE = true;
				vramInfoTable[toPage(0x400000) | i].bankE = i;
			}
			break;
		case 4: // Engine A, BG Extended Palette, Slot 0-3
			for (int i = 0; i < 4; i++) {
				epramInfoTable[i].enableE = true;
				epramInfoTable[i].bankE = i;
			}
			break;
		}
	}
	if (vramFEnable) {
		switch (vramFMst) {
		case 1: // 6000000h+(4000h*OFS.0)+(10000h*OFS.1)
			vramInfoTable[(vramFOffset & 1) | ((vramFOffset & 2) << 1)].enableF = true;
			vramInfoTable[(vramFOffset & 1) | ((vramFOffset & 2) << 1) | 2].enableF = true;
			break;
		case 2: // 6400000h+(4000h*OFS.0)+(10000h*OFS.1)
			vramInfoTable[toPage(0x400000) | (vramFOffset & 1) | ((vramFOffset & 2) << 1)].enableF = true;
			vramInfoTable[toPage(0x400000) | (vramFOffset & 1) | ((vramFOffset & 2) << 1) | 2].enableF = true;
			break;
		case 4: // Engine A, BG Extended Palette, Slot 0-1 (OFS=0), Slot 2-3 (OFS=1)
			epramInfoTable[((vramFOffset & 1) * 2) + 0].enableF = true;
			epramInfoTable[((vramFOffset & 1) * 2) + 0].bankF = 0;
			epramInfoTable[((vramFOffset & 1) * 2) + 1].enableF = true;
			epramInfoTable[((vramFOffset & 1) * 2) + 1].bankF = 1;
			break;
		case 5: // Engine A, OBJ Extended Palette, Slot 0
			epramInfoTable[4].enableF = true;
			epramInfoTable[4].bankF = 0;
			break;
		}
	}
	if (vramGEnable) {
		switch (vramGMst) {
		case 1: // 6000000h+(4000h*OFS.0)+(10000h*OFS.1)
			vramInfoTable[(vramGOffset & 1) | ((vramGOffset & 2) << 1)].enableG = true;
			vramInfoTable[(vramGOffset & 1) | ((vramGOffset & 2) << 1) | 2].enableG = true;
			break;
		case 2: // 6400000h+(4000h*OFS.0)+(10000h*OFS.1)
			vramInfoTable[toPage(0x400000) | (vramGOffset & 1) | ((vramGOffset & 2) << 1)].enableG = true;
			vramInfoTable[toPage(0x400000) | (vramGOffset & 1) | ((vramGOffset & 2) << 1) | 2].enableG = true;
			break;
		case 4: // Engine A, BG Extended Palette, Slot 0-1 (OFS=0), Slot 2-3 (OFS=1)
			epramInfoTable[((vramFOffset & 1) * 2) + 0].enableG = true;
			epramInfoTable[((vramFOffset & 1) * 2) + 0].bankG = 0;
			epramInfoTable[((vramFOffset & 1) * 2) + 1].enableG = true;
			epramInfoTable[((vramFOffset & 1) * 2) + 1].bankG = 1;
			break;
		case 5: // Engine A, OBJ Extended Palette, Slot 0
			epramInfoTable[4].enableG = true;
			epramInfoTable[4].bankG = 0;
			break;
		}
	}
	if (vramHEnable) {
		switch (vramHMst) {
		case 1: // 6200000h
			vramInfoTable[toPage(0x200000) | 0].enableH = true;
			vramInfoTable[toPage(0x200000) | 0].bankH = 0;
			vramInfoTable[toPage(0x200000) | 1].enableH = true;
			vramInfoTable[toPage(0x200000) | 1].bankH = 1;
			vramInfoTable[toPage(0x200000) | 4].enableH = true;
			vramInfoTable[toPage(0x200000) | 4].bankH = 0;
			vramInfoTable[toPage(0x200000) | 5].enableH = true;
			vramInfoTable[toPage(0x200000) | 5].bankH = 1;
			break;
		case 2: // Engine B, BG Extended Palette, Slot 0-3
			for (int i = 0; i < 4; i++) {
				epramInfoTable[i].enableH = true;
				epramInfoTable[i].bankH = i;
			}
		}
	}
	if (vramIEnable) {
		switch (vramIMst) {
		case 1: // 6208000h
			vramInfoTable[toPage(0x200000) + 2].enableI = true;
			vramInfoTable[toPage(0x200000) + 3].enableI = true;
			vramInfoTable[toPage(0x200000) + 6].enableI = true;
			vramInfoTable[toPage(0x200000) + 7].enableI = true;
			break;
		case 2: // 6600000h
			for (int i = 0; i < 8; i++)
				vramInfoTable[toPage(0x600000) | i].enableI = true;
			break;
		case 3: // Engine B, OBJ Extended Palette, Slot 0
			epramInfoTable[9].enableI = true;
			break;
		}
	}

	// Mirroring
	for (int i = toPage(0x000000); i < toPage(0x200000); i += toPage(0x80000)) // A BG
		memcpy(&vramInfoTable[i], &vramInfoTable[toPage(0x000000)], toPage(0x80000) * sizeof(VramInfoEntry));
	for (int i = toPage(0x200000); i < toPage(0x400000); i += toPage(0x20000)) // B BG
		memcpy(&vramInfoTable[i], &vramInfoTable[toPage(0x200000)], toPage(0x20000) * sizeof(VramInfoEntry));
	for (int i = toPage(0x400000); i < toPage(0x600000); i += toPage(0x40000)) // A OBJ
		memcpy(&vramInfoTable[i], &vramInfoTable[toPage(0x400000)], toPage(0x40000) * sizeof(VramInfoEntry));
	for (int i = toPage(0x600000); i < toPage(0x800000); i += toPage(0x20000)) // B OBJ
		memcpy(&vramInfoTable[i], &vramInfoTable[toPage(0x600000)], toPage(0x20000) * sizeof(VramInfoEntry));

	// Translate non-overlapping banks into pages
	for (int i = 0; i < 0x200; i++) {
		VramInfoEntry entry = vramInfoTable[i];
		if (std::popcount(entry.raw & 0x1FF) == 1) {
			if (entry.enableA) vramPageTable[i] = vramA + (entry.bankA * 0x4000);
			if (entry.enableB) vramPageTable[i] = vramB + (entry.bankB * 0x4000);
			if (entry.enableC) vramPageTable[i] = vramC + (entry.bankC * 0x4000);
			if (entry.enableD) vramPageTable[i] = vramD + (entry.bankD * 0x4000);
			if (entry.enableE) vramPageTable[i] = vramE + (entry.bankE * 0x4000);
			if (entry.enableF) vramPageTable[i] = vramF;
			if (entry.enableG) vramPageTable[i] = vramG;
			if (entry.enableH) vramPageTable[i] = vramH + (entry.bankH * 0x4000);
			if (entry.enableI) vramPageTable[i] = vramI;
		}
	}
	for (int i = 0; i < 10; i++) {
		VramInfoEntry entry = epramInfoTable[i];
		if (std::popcount(entry.raw & 0x1FF) == 1) {
			if (entry.enableE) vramPageTable[i] = vramE + (entry.bankE * 0x2000);
			if (entry.enableF) vramPageTable[i] = vramF + (entry.bankF * 0x2000);
			if (entry.enableG) vramPageTable[i] = vramG + (entry.bankG * 0x2000);
			if (entry.enableH) vramPageTable[i] = vramH + (entry.bankH * 0x2000);
			if (entry.enableI) vramPageTable[i] = vramI;
		}
	}
}

template <typename T, bool useEngineA, bool useObj>
T PPU::readVram(u32 address) {
	u32 alignedAddress = address & ~(sizeof(T) - 1);
	if constexpr (!useEngineA)
		alignedAddress += 0x200000;
	if constexpr (useObj)
		alignedAddress += 0x400000;

	u32 page = toPage(alignedAddress);
	u32 offset = alignedAddress & 0x3FFF;
	T val = 0;
	u8 *ptr = vramPageTable[page];

	if (ptr != NULL) { [[likely]]
		memcpy(&val, ptr + offset, sizeof(T));
	} else {
		VramInfoEntry entry = vramInfoTable[page];
		T tmpVal = 0;

		if (entry.enableA) { memcpy(&tmpVal, vramA + (entry.bankA * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableB) { memcpy(&tmpVal, vramB + (entry.bankB * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableC) { memcpy(&tmpVal, vramC + (entry.bankC * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableD) { memcpy(&tmpVal, vramD + (entry.bankD * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableE) { memcpy(&tmpVal, vramE + (entry.bankE * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableF) { memcpy(&tmpVal, vramF + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableG) { memcpy(&tmpVal, vramG + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableH) { memcpy(&tmpVal, vramH + (entry.bankH * 0x4000) + offset, sizeof(T)); val |= tmpVal; }
		if (entry.enableI) { memcpy(&tmpVal, vramI + offset, sizeof(T)); val |= tmpVal; }
	}

	return val;
}

template <bool useEngineA, bool useObj>
u16 PPU::readExtendedPalette(int slot, u32 index) {
	int page = 0;
	if constexpr (!useEngineA) {
		page += 5;
	}
	if constexpr (useObj) {
		page += 4;
	} else {
		page += slot;
	}

	u32 offset = index * 2;
	u16 val = 0;
	u8 *ptr = epramPageTable[page];

	if (ptr != NULL) { [[likely]]
		memcpy(&val, ptr + offset, sizeof(u16));
	} else {
		VramInfoEntry entry = epramInfoTable[page];
		u16 tmpVal = 0;

		if (entry.enableE) { memcpy(&tmpVal, vramE + (entry.bankE * 0x2000) + offset, sizeof(u16)); val |= tmpVal; }
		if (entry.enableF) { memcpy(&tmpVal, vramF + (entry.bankF * 0x2000) + offset, sizeof(u16)); val |= tmpVal; }
		if (entry.enableG) { memcpy(&tmpVal, vramG + (entry.bankG * 0x2000) + offset, sizeof(u16)); val |= tmpVal; }
		if (entry.enableH) { memcpy(&tmpVal, vramH + (entry.bankH * 0x2000) + offset, sizeof(u16)); val |= tmpVal; }
		if (entry.enableI) { memcpy(&tmpVal, vramI + offset, sizeof(u16)); val |= tmpVal; }
	}

	return val;
}

u8 PPU::readIO9(u32 address) {
	switch (address) {
	case 0x4000000:
		return (u8)engineA.DISPCNT;
	case 0x4000001:
		return (u8)(engineA.DISPCNT >> 8);
	case 0x4000002:
		return (u8)(engineA.DISPCNT >> 16);
	case 0x4000003:
		return (u8)(engineA.DISPCNT >> 24);
	case 0x4000004:
		return (u8)DISPSTAT9;
	case 0x4000005:
		return (u8)(DISPSTAT9 >> 8);
	case 0x4000006:
		return (u8)VCOUNT;
	case 0x4000007:
		return (u8)(VCOUNT >> 8);
	case 0x4000008:
		return (u8)engineA.bg[0].BGCNT;
	case 0x4000009:
		return (u8)(engineA.bg[0].BGCNT >> 8);
	case 0x400000A:
		return (u8)engineA.bg[1].BGCNT;
	case 0x400000B:
		return (u8)(engineA.bg[1].BGCNT >> 8);
	case 0x400000C:
		return (u8)engineA.bg[2].BGCNT;
	case 0x400000D:
		return (u8)(engineA.bg[2].BGCNT >> 8);
	case 0x400000E:
		return (u8)engineA.bg[3].BGCNT;
	case 0x400000F:
		return (u8)(engineA.bg[3].BGCNT >> 8);
	case 0x4000048:
		return (u8)engineA.WININ;
	case 0x4000049:
		return (u8)(engineA.WININ >> 8);
	case 0x400004A:
		return (u8)engineA.WINOUT;
	case 0x400004B:
		return (u8)(engineA.WINOUT >> 8);
	case 0x400006C:
		return (u8)engineA.MASTER_BRIGHT;
	case 0x400006D:
		return (u8)(engineA.MASTER_BRIGHT >> 8);
	case 0x400006E:
	case 0x400006F:
		return 0;
	case 0x4000304:
		return (u8)POWCNT1;
	case 0x4000305:
		return (u8)(POWCNT1 >> 8);
	case 0x4000306:
	case 0x4000307:
		return 0;
	case 0x4001000:
		return (u8)engineB.DISPCNT;
	case 0x4001001:
		return (u8)(engineB.DISPCNT >> 8);
	case 0x4001002:
		return (u8)(engineB.DISPCNT >> 16);
	case 0x4001003:
		return (u8)(engineB.DISPCNT >> 24);
	case 0x4001008:
		return (u8)engineB.bg[0].BGCNT;
	case 0x4001009:
		return (u8)(engineB.bg[0].BGCNT >> 8);
	case 0x400100A:
		return (u8)engineB.bg[1].BGCNT;
	case 0x400100B:
		return (u8)(engineB.bg[1].BGCNT >> 8);
	case 0x400100C:
		return (u8)engineB.bg[2].BGCNT;
	case 0x400100D:
		return (u8)(engineB.bg[2].BGCNT >> 8);
	case 0x400100E:
		return (u8)engineB.bg[3].BGCNT;
	case 0x400100F:
		return (u8)(engineB.bg[3].BGCNT >> 8);
	case 0x4001048:
		return (u8)engineB.WININ;
	case 0x4001049:
		return (u8)(engineB.WININ >> 8);
	case 0x400104A:
		return (u8)engineB.WINOUT;
	case 0x400104B:
		return (u8)(engineB.WINOUT >> 8);
	case 0x400106C:
		return (u8)engineB.MASTER_BRIGHT;
	case 0x400106D:
		return (u8)(engineB.MASTER_BRIGHT >> 8);
	case 0x400106E:
	case 0x400106F:
		return 0;
	default:
		shared->log << fmt::format("[NDS9 Bus][PPU] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void PPU::writeIO9(u32 address, u8 value) {
	switch (address) {
	case 0x4000000:
		engineA.DISPCNT = (engineA.DISPCNT & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000001:
		engineA.DISPCNT = (engineA.DISPCNT & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000002:
		engineA.DISPCNT = (engineA.DISPCNT & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x4000003:
		engineA.DISPCNT = (engineA.DISPCNT & 0x00FFFFFF) | ((value & 0xFF) << 24);
		engineA.bg[0].charBlockBaseAddress = (16384 * engineA.bg[0].charBlock) + (65536 * engineA.charBase);
		engineA.bg[1].charBlockBaseAddress = (16384 * engineA.bg[1].charBlock) + (65536 * engineA.charBase);
		engineA.bg[2].charBlockBaseAddress = (16384 * engineA.bg[2].charBlock) + (65536 * engineA.charBase);
		engineA.bg[3].charBlockBaseAddress = (16384 * engineA.bg[3].charBlock) + (65536 * engineA.charBase);
		engineA.bg[0].screenBlockBaseAddress = (2048 * engineA.bg[0].screenBlock) + (65536 * engineA.charBase);
		engineA.bg[1].screenBlockBaseAddress = (2048 * engineA.bg[1].screenBlock) + (65536 * engineA.charBase);
		engineA.bg[2].screenBlockBaseAddress = (2048 * engineA.bg[2].screenBlock) + (65536 * engineA.charBase);
		engineA.bg[3].screenBlockBaseAddress = (2048 * engineA.bg[3].screenBlock) + (65536 * engineA.charBase);
		break;
	case 0x4000004:
		DISPSTAT9 = (DISPSTAT9 & 0xFF47) | ((value & 0xB8) << 0);
		break;
	case 0x4000005:
		DISPSTAT9 = (DISPSTAT9 & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000008:
		engineA.bg[0].BGCNT = (engineA.bg[0].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineA.bg[0].charBlockBaseAddress = (16384 * engineA.bg[0].charBlock) + (65536 * engineA.charBase);
		break;
	case 0x4000009:
		engineA.bg[0].BGCNT = (engineA.bg[0].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineA.bg[0].screenBlockBaseAddress = (2048 * engineA.bg[0].screenBlock) + (65536 * engineA.screenBase);
		break;
	case 0x400000A:
		engineA.bg[1].BGCNT = (engineA.bg[1].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineA.bg[1].charBlockBaseAddress = (16384 * engineA.bg[1].charBlock) + (65536 * engineA.charBase);
		break;
	case 0x400000B:
		engineA.bg[1].BGCNT = (engineA.bg[1].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineA.bg[1].screenBlockBaseAddress = (2048 * engineA.bg[1].screenBlock) + (65536 * engineA.screenBase);
		break;
	case 0x400000C:
		engineA.bg[2].BGCNT = (engineA.bg[2].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineA.bg[2].charBlockBaseAddress = (16384 * engineA.bg[2].charBlock) + (65536 * engineA.charBase);
		break;
	case 0x400000D:
		engineA.bg[2].BGCNT = (engineA.bg[2].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineA.bg[2].screenBlockBaseAddress = (2048 * engineA.bg[2].screenBlock) + (65536 * engineA.screenBase);
		break;
	case 0x400000E:
		engineA.bg[3].BGCNT = (engineA.bg[3].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineA.bg[3].charBlockBaseAddress = (16384 * engineA.bg[3].charBlock) + (65536 * engineA.charBase);
		break;
	case 0x400000F:
		engineA.bg[3].BGCNT = (engineA.bg[3].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineA.bg[3].screenBlockBaseAddress = (2048 * engineA.bg[3].screenBlock) + (65536 * engineA.screenBase);
		break;
	case 0x4000010:
		engineA.bg[0].BGHOFS = (engineA.bg[0].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000011:
		engineA.bg[0].BGHOFS = (engineA.bg[0].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4000012:
		engineA.bg[0].BGVOFS = (engineA.bg[0].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000013:
		engineA.bg[0].BGVOFS = (engineA.bg[0].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4000014:
		engineA.bg[1].BGHOFS = (engineA.bg[1].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000015:
		engineA.bg[1].BGHOFS = (engineA.bg[1].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4000016:
		engineA.bg[1].BGVOFS = (engineA.bg[1].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000017:
		engineA.bg[1].BGVOFS = (engineA.bg[1].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4000018:
		engineA.bg[2].BGHOFS = (engineA.bg[2].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000019:
		engineA.bg[2].BGHOFS = (engineA.bg[2].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x400001A:
		engineA.bg[2].BGVOFS = (engineA.bg[2].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400001B:
		engineA.bg[2].BGVOFS = (engineA.bg[2].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x400001C:
		engineA.bg[3].BGHOFS = (engineA.bg[3].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400001D:
		engineA.bg[3].BGHOFS = (engineA.bg[3].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x400001E:
		engineA.bg[3].BGVOFS = (engineA.bg[3].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400001F:
		engineA.bg[3].BGVOFS = (engineA.bg[3].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4000020:
		engineA.bg[2].BGPA = (engineA.bg[2].BGPA & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000021:
		engineA.bg[2].BGPA = (engineA.bg[2].BGPA & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000022:
		engineA.bg[2].BGPB = (engineA.bg[2].BGPB & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000023:
		engineA.bg[2].BGPB = (engineA.bg[2].BGPB & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000024:
		engineA.bg[2].BGPC = (engineA.bg[2].BGPC & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000025:
		engineA.bg[2].BGPC = (engineA.bg[2].BGPC & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000026:
		engineA.bg[2].BGPD = (engineA.bg[2].BGPD & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000027:
		engineA.bg[2].BGPD = (engineA.bg[2].BGPD & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000028:
		engineA.bg[2].BGX = (engineA.bg[2].BGX & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineA.bg[2].internalBGX = (float)((i32)(engineA.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x4000029:
		engineA.bg[2].BGX = (engineA.bg[2].BGX & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineA.bg[2].internalBGX = (float)((i32)(engineA.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x400002A:
		engineA.bg[2].BGX = (engineA.bg[2].BGX & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineA.bg[2].internalBGX = (float)((i32)(engineA.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x400002B:
		engineA.bg[2].BGX = (engineA.bg[2].BGX & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineA.bg[2].internalBGX = (float)((i32)(engineA.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x400002C:
		engineA.bg[2].BGY = (engineA.bg[2].BGY & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineA.bg[2].internalBGY = (float)((i32)(engineA.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x400002D:
		engineA.bg[2].BGY = (engineA.bg[2].BGY & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineA.bg[2].internalBGY = (float)((i32)(engineA.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x400002E:
		engineA.bg[2].BGY = (engineA.bg[2].BGY & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineA.bg[2].internalBGY = (float)((i32)(engineA.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x400002F:
		engineA.bg[2].BGY = (engineA.bg[2].BGY & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineA.bg[2].internalBGY = (float)((i32)(engineA.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x4000030:
		engineA.bg[3].BGPA = (engineA.bg[3].BGPA & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000031:
		engineA.bg[3].BGPA = (engineA.bg[3].BGPA & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000032:
		engineA.bg[3].BGPB = (engineA.bg[3].BGPB & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000033:
		engineA.bg[3].BGPB = (engineA.bg[3].BGPB & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000034:
		engineA.bg[3].BGPC = (engineA.bg[3].BGPC & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000035:
		engineA.bg[3].BGPC = (engineA.bg[3].BGPC & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000036:
		engineA.bg[3].BGPD = (engineA.bg[3].BGPD & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000037:
		engineA.bg[3].BGPD = (engineA.bg[3].BGPD & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000038:
		engineA.bg[3].BGX = (engineA.bg[3].BGX & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineA.bg[3].internalBGX = (float)((i32)(engineA.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x4000039:
		engineA.bg[3].BGX = (engineA.bg[3].BGX & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineA.bg[3].internalBGX = (float)((i32)(engineA.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x400003A:
		engineA.bg[3].BGX = (engineA.bg[3].BGX & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineA.bg[3].internalBGX = (float)((i32)(engineA.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x400003B:
		engineA.bg[3].BGX = (engineA.bg[3].BGX & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineA.bg[3].internalBGX = (float)((i32)(engineA.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x400003C:
		engineA.bg[3].BGY = (engineA.bg[3].BGY & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineA.bg[3].internalBGY = (float)((i32)(engineA.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x400003D:
		engineA.bg[3].BGY = (engineA.bg[3].BGY & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineA.bg[3].internalBGY = (float)((i32)(engineA.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x400003E:
		engineA.bg[3].BGY = (engineA.bg[3].BGY & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineA.bg[3].internalBGY = (float)((i32)(engineA.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x400003F:
		engineA.bg[3].BGY = (engineA.bg[3].BGY & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineA.bg[3].internalBGY = (float)((i32)(engineA.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x4000040:
		engineA.WIN0H = (engineA.WIN0H & 0xFF00) | ((value & 0xFF) << 0);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000041:
		engineA.WIN0H = (engineA.WIN0H & 0x00FF) | ((value & 0xFF) << 8);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000042:
		engineA.WIN1H = (engineA.WIN1H & 0xFF00) | ((value & 0xFF) << 0);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000043:
		engineA.WIN1H = (engineA.WIN1H & 0x00FF) | ((value & 0xFF) << 8);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000044:
		engineA.WIN0V = (engineA.WIN0V & 0xFF00) | ((value & 0xFF) << 0);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000045:
		engineA.WIN0V = (engineA.WIN0V & 0x00FF) | ((value & 0xFF) << 8);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000046:
		engineA.WIN1V = (engineA.WIN1V & 0xFF00) | ((value & 0xFF) << 0);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000047:
		engineA.WIN1V = (engineA.WIN1V & 0x00FF) | ((value & 0xFF) << 8);
		engineA.windowMasksDirty = true;
		break;
	case 0x4000048:
		engineA.WININ = (engineA.WININ & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000049:
		engineA.WININ = (engineA.WININ & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400004A:
		engineA.WINOUT = (engineA.WINOUT & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400004B:
		engineA.WINOUT = (engineA.WINOUT & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400004C:
		engineA.MOSAIC = (engineA.MOSAIC & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400004D:
		engineA.MOSAIC = (engineA.MOSAIC & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400006C:
		engineA.MASTER_BRIGHT = (engineA.MASTER_BRIGHT & 0xFF00) | ((value & 0x1F) << 0);
		break;
	case 0x400006D:
		engineA.MASTER_BRIGHT = (engineA.MASTER_BRIGHT & 0x00FF) | ((value & 0xC0) << 8);
		break;
	case 0x400006E:
	case 0x400006F:
		break;
	case 0x4000240:
		VRAMCNT_A = value & 0x9B;

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000241:
		VRAMCNT_B = value & 0x9F;

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000242:
		VRAMCNT_C = value & 0x9F;
		vramCMapped7 = vramCEnable && (vramCMst == 2);

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000243:
		VRAMCNT_D = value & 0x9F;
		vramDMapped7 = vramDEnable && (vramDMst == 2);

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000244:
		VRAMCNT_E = value & 0x87;

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000245:
		VRAMCNT_F = value & 0x9F;

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000246:
		VRAMCNT_G = value & 0x9F;

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000248:
		VRAMCNT_H = value & 0x83;

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000249:
		VRAMCNT_I = value & 0x83;

		shared->addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000304:
		POWCNT1 = (POWCNT1 & 0xFF00) | ((value & 0x0F) << 0);
		break;
	case 0x4000305:
		POWCNT1 = (POWCNT1 & 0x00FF) | ((value & 0x82) << 8);
		break;
	case 0x4000306:
	case 0x4000307:
		break;
	case 0x4001000:
		engineB.DISPCNT = (engineB.DISPCNT & 0xFFFFFF00) | ((value & 0xF7) << 0);
		break;
	case 0x4001001:
		engineB.DISPCNT = (engineB.DISPCNT & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001002:
		engineB.DISPCNT = (engineB.DISPCNT & 0xFF00FFFF) | ((value & 0xB3) << 16);
		break;
	case 0x4001003:
		engineB.DISPCNT = (engineB.DISPCNT & 0x00FFFFFF) | ((value & 0xC0) << 24);
		break;
	case 0x4001008:
		engineB.bg[0].BGCNT = (engineB.bg[0].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineB.bg[0].charBlockBaseAddress = 16384 * engineB.bg[0].charBlock;
		break;
	case 0x4001009:
		engineB.bg[0].BGCNT = (engineB.bg[0].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineB.bg[0].screenBlockBaseAddress = 2048 * engineB.bg[0].screenBlock;
		break;
	case 0x400100A:
		engineB.bg[1].BGCNT = (engineB.bg[1].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineB.bg[1].charBlockBaseAddress = 16384 * engineB.bg[1].charBlock;
		break;
	case 0x400100B:
		engineB.bg[1].BGCNT = (engineB.bg[1].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineB.bg[1].screenBlockBaseAddress = 2048 * engineB.bg[1].screenBlock;
		break;
	case 0x400100C:
		engineB.bg[2].BGCNT = (engineB.bg[2].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineB.bg[2].charBlockBaseAddress = 16384 * engineB.bg[2].charBlock;
		break;
	case 0x400100D:
		engineB.bg[2].BGCNT = (engineB.bg[2].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineB.bg[2].screenBlockBaseAddress = 2048 * engineB.bg[2].screenBlock;
		break;
	case 0x400100E:
		engineB.bg[3].BGCNT = (engineB.bg[3].BGCNT & 0xFF00) | ((value & 0xFF) << 0);
		engineB.bg[3].charBlockBaseAddress = 16384 * engineB.bg[3].charBlock;
		break;
	case 0x400100F:
		engineB.bg[3].BGCNT = (engineB.bg[3].BGCNT & 0x00FF) | ((value & 0xFF) << 8);
		engineB.bg[3].screenBlockBaseAddress = 2048 * engineB.bg[3].screenBlock;
		break;
	case 0x4001010:
		engineB.bg[0].BGHOFS = (engineB.bg[0].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001011:
		engineB.bg[0].BGHOFS = (engineB.bg[0].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4001012:
		engineB.bg[0].BGVOFS = (engineB.bg[0].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001013:
		engineB.bg[0].BGVOFS = (engineB.bg[0].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4001014:
		engineB.bg[1].BGHOFS = (engineB.bg[1].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001015:
		engineB.bg[1].BGHOFS = (engineB.bg[1].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4001016:
		engineB.bg[1].BGVOFS = (engineB.bg[1].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001017:
		engineB.bg[1].BGVOFS = (engineB.bg[1].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4001018:
		engineB.bg[2].BGHOFS = (engineB.bg[2].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001019:
		engineB.bg[2].BGHOFS = (engineB.bg[2].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x400101A:
		engineB.bg[2].BGVOFS = (engineB.bg[2].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400101B:
		engineB.bg[2].BGVOFS = (engineB.bg[2].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x400101C:
		engineB.bg[3].BGHOFS = (engineB.bg[3].BGHOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400101D:
		engineB.bg[3].BGHOFS = (engineB.bg[3].BGHOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x400101E:
		engineB.bg[3].BGVOFS = (engineB.bg[3].BGVOFS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400101F:
		engineB.bg[3].BGVOFS = (engineB.bg[3].BGVOFS & 0x00FF) | ((value & 0x01) << 8);
		break;
	case 0x4001020:
		engineB.bg[2].BGPA = (engineB.bg[2].BGPA & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001021:
		engineB.bg[2].BGPA = (engineB.bg[2].BGPA & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001022:
		engineB.bg[2].BGPB = (engineB.bg[2].BGPB & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001023:
		engineB.bg[2].BGPB = (engineB.bg[2].BGPB & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001024:
		engineB.bg[2].BGPC = (engineB.bg[2].BGPC & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001025:
		engineB.bg[2].BGPC = (engineB.bg[2].BGPC & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001026:
		engineB.bg[2].BGPD = (engineB.bg[2].BGPD & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001027:
		engineB.bg[2].BGPD = (engineB.bg[2].BGPD & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001028:
		engineB.bg[2].BGX = (engineB.bg[2].BGX & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineB.bg[2].internalBGX = (float)((i32)(engineB.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x4001029:
		engineB.bg[2].BGX = (engineB.bg[2].BGX & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineB.bg[2].internalBGX = (float)((i32)(engineB.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x400102A:
		engineB.bg[2].BGX = (engineB.bg[2].BGX & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineB.bg[2].internalBGX = (float)((i32)(engineB.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x400102B:
		engineB.bg[2].BGX = (engineB.bg[2].BGX & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineB.bg[2].internalBGX = (float)((i32)(engineB.bg[2].BGX << 4) >> 4) / 256;
		break;
	case 0x400102C:
		engineB.bg[2].BGY = (engineB.bg[2].BGY & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineB.bg[2].internalBGY = (float)((i32)(engineB.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x400102D:
		engineB.bg[2].BGY = (engineB.bg[2].BGY & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineB.bg[2].internalBGY = (float)((i32)(engineB.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x400102E:
		engineB.bg[2].BGY = (engineB.bg[2].BGY & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineB.bg[2].internalBGY = (float)((i32)(engineB.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x400102F:
		engineB.bg[2].BGY = (engineB.bg[2].BGY & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineB.bg[2].internalBGY = (float)((i32)(engineB.bg[2].BGY << 4) >> 4) / 256;
		break;
	case 0x4001030:
		engineB.bg[3].BGPA = (engineB.bg[3].BGPA & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001031:
		engineB.bg[3].BGPA = (engineB.bg[3].BGPA & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001032:
		engineB.bg[3].BGPB = (engineB.bg[3].BGPB & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001033:
		engineB.bg[3].BGPB = (engineB.bg[3].BGPB & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001034:
		engineB.bg[3].BGPC = (engineB.bg[3].BGPC & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001035:
		engineB.bg[3].BGPC = (engineB.bg[3].BGPC & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001036:
		engineB.bg[3].BGPD = (engineB.bg[3].BGPD & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001037:
		engineB.bg[3].BGPD = (engineB.bg[3].BGPD & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4001038:
		engineB.bg[3].BGX = (engineB.bg[3].BGX & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineB.bg[3].internalBGX = (float)((i32)(engineB.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x4001039:
		engineB.bg[3].BGX = (engineB.bg[3].BGX & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineB.bg[3].internalBGX = (float)((i32)(engineB.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x400103A:
		engineB.bg[3].BGX = (engineB.bg[3].BGX & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineB.bg[3].internalBGX = (float)((i32)(engineB.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x400103B:
		engineB.bg[3].BGX = (engineB.bg[3].BGX & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineB.bg[3].internalBGX = (float)((i32)(engineB.bg[3].BGX << 4) >> 4) / 256;
		break;
	case 0x400103C:
		engineB.bg[3].BGY = (engineB.bg[3].BGY & 0xFFFFFF00) | ((value & 0xFF) << 0);
		engineB.bg[3].internalBGY = (float)((i32)(engineB.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x400103D:
		engineB.bg[3].BGY = (engineB.bg[3].BGY & 0xFFFF00FF) | ((value & 0xFF) << 8);
		engineB.bg[3].internalBGY = (float)((i32)(engineB.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x400103E:
		engineB.bg[3].BGY = (engineB.bg[3].BGY & 0xFF00FFFF) | ((value & 0xFF) << 16);
		engineB.bg[3].internalBGY = (float)((i32)(engineB.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x400103F:
		engineB.bg[3].BGY = (engineB.bg[3].BGY & 0x00FFFFFF) | ((value & 0x0F) << 24);
		engineB.bg[3].internalBGY = (float)((i32)(engineB.bg[3].BGY << 4) >> 4) / 256;
		break;
	case 0x4001040:
		engineB.WIN0H = (engineB.WIN0H & 0xFF00) | ((value & 0xFF) << 0);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001041:
		engineB.WIN0H = (engineB.WIN0H & 0x00FF) | ((value & 0xFF) << 8);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001042:
		engineB.WIN1H = (engineB.WIN1H & 0xFF00) | ((value & 0xFF) << 0);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001043:
		engineB.WIN1H = (engineB.WIN1H & 0x00FF) | ((value & 0xFF) << 8);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001044:
		engineB.WIN0V = (engineB.WIN0V & 0xFF00) | ((value & 0xFF) << 0);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001045:
		engineB.WIN0V = (engineB.WIN0V & 0x00FF) | ((value & 0xFF) << 8);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001046:
		engineB.WIN1V = (engineB.WIN1V & 0xFF00) | ((value & 0xFF) << 0);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001047:
		engineB.WIN1V = (engineB.WIN1V & 0x00FF) | ((value & 0xFF) << 8);
		engineB.windowMasksDirty = true;
		break;
	case 0x4001048:
		engineB.WININ = (engineB.WININ & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4001049:
		engineB.WININ = (engineB.WININ & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400104A:
		engineB.WINOUT = (engineB.WINOUT & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400104B:
		engineB.WINOUT = (engineB.WINOUT & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400104C:
		engineB.MOSAIC = (engineB.MOSAIC & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400104D:
		engineB.MOSAIC = (engineB.MOSAIC & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400106C:
		engineB.MASTER_BRIGHT = (engineB.MASTER_BRIGHT & 0xFF00) | ((value & 0x1F) << 0);
		break;
	case 0x400106D:
		engineB.MASTER_BRIGHT = (engineB.MASTER_BRIGHT & 0x00FF) | ((value & 0xC0) << 8);
		break;
	case 0x400106E:
	case 0x400106F:
		break;
	default:
		shared->log << fmt::format("[NDS9 Bus][PPU] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}

u8 PPU::readIO7(u32 address) {
	switch (address) {
	case 0x4000004:
		return (u8)DISPSTAT7;
	case 0x4000005:
		return (u8)(DISPSTAT7 >> 8);
	case 0x4000006:
		return (u8)VCOUNT;
	case 0x4000007:
		return (u8)(VCOUNT >> 8);
	case 0x4000240:
		return VRAMSTAT;
	default:
		shared->log << fmt::format("[NDS7 Bus][PPU] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void PPU::writeIO7(u32 address, u8 value) {
	switch (address) {
	case 0x4000004:
		DISPSTAT7 = (DISPSTAT7 & 0xFF47) | ((value & 0xB8) << 0);
		break;
	case 0x4000005:
		DISPSTAT7 = (DISPSTAT7 & 0x00FF) | ((value & 0xFF) << 8);
		break;
	default:
		shared->log << fmt::format("[NDS7 Bus][PPU] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}
