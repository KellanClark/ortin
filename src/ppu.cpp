
#include "ppu.hpp"

#define convertColor(x) ((x) | 0x8000)

PPU::PPU(BusShared &shared, std::stringstream &log) : shared(shared), log(log) {
	vramA = new u8[0x20000]; // 128KB
	vramB = new u8[0x20000]; // 128KB
	vramC = new u8[0x20000]; // 128KB
	vramD = new u8[0x20000]; // 128KB
	vramE = new u8[0x10000]; // 64KB
	vramF = new u8[0x4000]; // 16KB
	vramG = new u8[0x4000]; // 16KB
	vramH = new u8[0x8000]; // 32KB
	vramI = new u8[0x4000]; // 16KB
}

PPU::~PPU() {
	delete[] vramA;
	delete[] vramB;
	delete[] vramC;
	delete[] vramD;
	delete[] vramE;
	delete[] vramF;
	delete[] vramG;
	delete[] vramH;
	delete[] vramI;
}

void PPU::reset() {
	memset(framebufferA, 0xFF, sizeof(framebufferA));
	memset(framebufferB, 0xFF, sizeof(framebufferB));

	memset(vramA, 0, 0x20000);
	memset(vramB, 0, 0x20000);
	memset(vramC, 0, 0x20000);
	memset(vramD, 0, 0x20000);
	memset(vramE, 0, 0x10000);
	memset(vramF, 0, 0x4000);
	memset(vramG, 0, 0x4000);
	memset(vramH, 0, 0x8000);
	memset(vramI, 0, 0x4000);

	engineA.DISPCNT = 0;
	engineB.DISPCNT = 0;

	POWCNT1 = 0;

	// > Dot clock = 5.585664 MHz (=33.513982 MHz / 6)
	// > H-Timing: 256 dots visible, 99 dots blanking, 355 dots total (15.7343KHz)
	// Line length = (256 + 99) * 6 * 2 = 4,260
	shared.addEvent(4260, EventType::PPU_LINE_START);
	shared.addEvent(3072, EventType::PPU_HBLANK);
}

void PPU::lineStart() {
	shared.addEvent(4260, EventType::PPU_LINE_START);

	++currentScanline;
	switch (currentScanline) {
	case 192: // VBlank
		updateScreen = true;
		vBlankFlag = true;

		//if (vBlankIrqEnable)
		//	bus.cpu.requestInterrupt(GBACPU::IRQ_VBLANK);
		break;
	case 262:
		// > Similar as on GBA, the VBlank flag isn't set in the last line (ie. only in lines 192..261, but not in line 262).
		vBlankFlag = false;
		break;
	case 263: // Start of Frame
		++frameCounter;
		currentScanline = 0;
		vBlankFlag = false;
		break;
	}

	if (((lycMsb << 8) | lycLsb) == currentScanline) {
		vCounterFlag = true;

		//if (vCounterIrqEnable)
		//	bus.cpu.requestInterrupt(GBACPU::IRQ_VCOUNT);
	} else {
		vCounterFlag = false;
	}
}

void PPU::hBlank() {
	shared.addEvent(4260, EventType::PPU_HBLANK);

	hBlankFlag = true;
	//if (hBlankIrqEnable)
	//	bus.cpu.requestInterrupt(GBACPU::IRQ_HBLANK);

	if (currentScanline < 192) {
		drawLine();
		//bus.dma.onHBlank();
	 }
}

void PPU::drawLine() {
	switch (engineA.displayMode) {
	default:
		printf("shit\n");
	case 0: // Display off
		for (int i = 0; i < 256; i++)
			framebufferA[currentScanline][i] = 0xFFFF;
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

	// It's your fault if you think I'm going to write another graphics engine right now.
	for (int i = 0; i < 256; i++)
		framebufferB[currentScanline][i] = 0xFFFF;
}

u8 PPU::readIO9(u32 address) {
	switch (address) {
	case 0x4000004:
		return (u8)DISPSTAT;
	case 0x4000005:
		return (u8)(DISPSTAT >> 8);
	case 0x4000006:
		return (u8)VCOUNT;
	case 0x4000007:
		return (u8)(VCOUNT >> 8);
	case 0x4000304:
		return (u8)POWCNT1;
	case 0x4000305:
		return (u8)(POWCNT1 >> 8);
	case 0x4000306:
	case 0x4000307:
		return 0;
	default:
		log << fmt::format("[ARM9 Bus][PPU] Read from unknown IO register 0x{:0>8X}\n", address);
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
		break;
	case 0x4000004:
		DISPSTAT = (DISPSTAT & 0xFF47) | ((value & 0xB8) << 0);
		break;
	case 0x4000005:
		DISPSTAT = (DISPSTAT & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000240:
		VRAMCNT_A = value & 0x9B;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000241:
		VRAMCNT_B = value & 0x9F;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000242:
		VRAMCNT_C = value & 0x9F;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000243:
		VRAMCNT_D = value & 0x9F;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000244:
		VRAMCNT_E = value & 0x87;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000245:
		VRAMCNT_F = value & 0x9F;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000246:
		VRAMCNT_G = value & 0x9F;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000247:
		VRAMCNT_H = value & 0x83;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
		break;
	case 0x4000248:
		VRAMCNT_I = value & 0x83;

		shared.addEvent(0, EventType::REFRESH_VRAM_PAGES);
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
	case 0x4000304:
		POWCNT1 = (POWCNT1 & 0xFF00) | ((value & 0x0F) << 0);
		break;
	case 0x4000305:
		POWCNT1 = (POWCNT1 & 0x00FF) | ((value & 0x82) << 8);
		break;
	case 0x4000306:
	case 0x4000307:
		break;
	default:
		log << fmt::format("[ARM9 Bus][PPU] Write to unknown IO register 0x{:0>8X} with value 0x{:0>8X}\n", address, value);
		break;
	}
}

u8 PPU::readIO7(u32 address) {
	switch (address) {
	default:
		log << fmt::format("[ARM7 Bus][PPU] Read from unknown IO register 0x{:0>8X}\n", address);
		return 0;
	}
}

void PPU::writeIO7(u32 address, u8 value) {
	switch (address) {
	default:
		log << fmt::format("[ARM7 Bus][PPU] Write to unknown IO register 0x{:0>8X} with value 0x{:0>8X}\n", address, value);
		break;
	}
}
