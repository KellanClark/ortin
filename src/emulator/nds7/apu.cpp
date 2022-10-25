#include "emulator/nds7/apu.hpp"

APU::APU(BusShared &shared, std::stringstream &log) : shared(shared), log(log) {
	//
}

APU::~APU() {
	//
}

void APU::reset() {
	for (int i = 0; i < 16; i++) {
		AudioChannel& chan = channel[i];

		chan.SOUNDCNT = 0;
		chan.SOUNDSAD = 0;
		chan.SOUNDTMR = 0;
		chan.SOUNDPNT = 0;
		chan.SOUNDLEN = 0;
	}

	SOUNDBIAS = 0x200;
}

u8 APU::readIO7(u32 address) {
	switch (address) {
	case 0x4000400 ... 0x40004FF: {
		AudioChannel& chan = channel[(address >> 4) & 0xF];
		switch (address & 0xF) {
		case 0x0:
			return (u8)(chan.SOUNDCNT >> 0);
		case 0x1:
			return (u8)(chan.SOUNDCNT >> 8);
		case 0x2:
			return (u8)(chan.SOUNDCNT >> 16);
		case 0x3:
			return (u8)(chan.SOUNDCNT >> 24);
		default:
			return 0;
		}
		} break;
	case 0x4000500:
		return (u8)(SOUNDCNT >> 0);
	case 0x4000501:
		return (u8)(SOUNDCNT >> 8);
	case 0x4000502:
	case 0x4000503:
		return 0;
	case 0x4000504:
		return (u8)(SOUNDBIAS >> 0);
	case 0x4000505:
		return (u8)(SOUNDBIAS >> 8);
	case 0x4000508:
		return SNDCAP0CNT;
	case 0x4000509:
		return SNDCAP1CNT;
	case 0x4000510:
		return (u8)(SNDCAP0DAD >> 0);
	case 0x4000511:
		return (u8)(SNDCAP0DAD >> 8);
	case 0x4000512:
		return (u8)(SNDCAP0DAD >> 16);
	case 0x4000513:
		return (u8)(SNDCAP0DAD >> 24);
	case 0x4000518:
		return (u8)(SNDCAP0DAD >> 0);
	case 0x4000519:
		return (u8)(SNDCAP0DAD >> 8);
	case 0x400051A:
		return (u8)(SNDCAP0DAD >> 16);
	case 0x400051B:
		return (u8)(SNDCAP0DAD >> 24);
	default:
		log << fmt::format("[NDS7 Bus][APU] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void APU::writeIO7(u32 address, u8 value) {
	switch (address) {
	case 0x4000400 ... 0x40004FF: {
		AudioChannel& chan = channel[(address >> 4) & 0xF];
		switch (address & 0xF) {
		case 0x0:
			chan.SOUNDCNT = (chan.SOUNDCNT & 0xFFFFFF00) | ((value & 0x7F) << 0);
			break;
		case 0x1:
			chan.SOUNDCNT = (chan.SOUNDCNT & 0xFFFF00FF) | ((value & 0x83) << 8);
			break;
		case 0x2:
			chan.SOUNDCNT = (chan.SOUNDCNT & 0xFF00FFFF) | ((value & 0x7F) << 16);
			break;
		case 0x3:
			chan.SOUNDCNT = (chan.SOUNDCNT & 0x00FFFFFF) | ((value & 0xFF) << 24);
			break;
		case 0x4:
			chan.SOUNDSAD = (chan.SOUNDSAD & 0xFFFFFF00) | ((value & 0xFC) << 0);
			break;
		case 0x5:
			chan.SOUNDSAD = (chan.SOUNDSAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
			break;
		case 0x6:
			chan.SOUNDSAD = (chan.SOUNDSAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
			break;
		case 0x7:
			chan.SOUNDSAD = (chan.SOUNDSAD & 0x00FFFFFF) | ((value & 0x07) << 24);
			break;
		case 0x8:
			chan.SOUNDTMR = (chan.SOUNDTMR & 0xFF00) | ((value & 0xFF) << 0);
			break;
		case 0x9:
			chan.SOUNDTMR = (chan.SOUNDTMR & 0x00FF) | ((value & 0xFF) << 8);
			break;
		case 0xA:
			chan.SOUNDPNT = (chan.SOUNDPNT & 0xFF00) | ((value & 0xFF) << 0);
			break;
		case 0xB:
			chan.SOUNDPNT = (chan.SOUNDPNT & 0x00FF) | ((value & 0xFF) << 8);
			break;
		case 0xC:
			chan.SOUNDLEN = (chan.SOUNDLEN & 0xFFFFFF00) | ((value & 0xFF) << 0);
			break;
		case 0xD:
			chan.SOUNDLEN = (chan.SOUNDLEN & 0xFFFF00FF) | ((value & 0xFF) << 8);
			break;
		case 0xE:
			chan.SOUNDLEN = (chan.SOUNDLEN & 0xFF00FFFF) | ((value & 0x3F) << 16);
			break;
		case 0xF:
			break;
		}
	} break;
	case 0x4000500:
		SOUNDCNT = (SOUNDCNT & 0xFF00) | ((value & 0x7F) << 0);
		break;
	case 0x4000501:
		SOUNDCNT = (SOUNDCNT & 0x00FF) | ((value & 0xBF) << 8);
		break;
	case 0x4000502:
	case 0x4000503:
		break;
	case 0x4000504:
		SOUNDBIAS = (SOUNDBIAS & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000505:
		SOUNDBIAS = (SOUNDBIAS & 0x00FF) | ((value & 0x03) << 8);
		break;
	case 0x4000508:
		SNDCAP0CNT = value & 0x8F;
		break;
	case 0x4000509:
		SNDCAP1CNT = value & 0x8F;
		break;
	case 0x4000510:
		SNDCAP0DAD = (SNDCAP0DAD & 0xFFFFFF00) | ((value & 0xFC) << 0);
		break;
	case 0x4000511:
		SNDCAP0DAD = (SNDCAP0DAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000512:
		SNDCAP0DAD = (SNDCAP0DAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x4000513:
		SNDCAP0DAD = (SNDCAP0DAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x4000514:
		SNDCAP0LEN = (SNDCAP0LEN & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000515:
		SNDCAP0LEN = (SNDCAP0LEN & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000516:
	case 0x4000517:
		break;
	case 0x4000518:
		SNDCAP1DAD = (SNDCAP1DAD & 0xFFFFFF00) | ((value & 0xFC) << 0);
		break;
	case 0x4000519:
		SNDCAP1DAD = (SNDCAP1DAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400051A:
		SNDCAP1DAD = (SNDCAP1DAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x400051B:
		SNDCAP1DAD = (SNDCAP1DAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x400051C:
		SNDCAP1LEN = (SNDCAP1LEN & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400051D:
		SNDCAP1LEN = (SNDCAP1LEN & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400051E:
	case 0x400051F:
		break;
	default:
		log << fmt::format("[NDS7 Bus][APU] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}