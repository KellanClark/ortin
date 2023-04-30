#include "emulator/nds7/apu.hpp"

const int indexTable[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

i16 adpcmTable[89] = {
	0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010, 0x0011, 0x0013, 0x0015,
	0x0017, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
	0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F, 0x009D, 0x00AD, 0x00BE, 0x00D1,
	0x00E6, 0x00FD, 0x0117, 0x0133, 0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
	0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583, 0x0610, 0x06AB, 0x0756, 0x0812,
	0x08E0, 0x09C3, 0x0ABD, 0x0BD0, 0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
	0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B, 0x3BB9, 0x41B2, 0x4844, 0x4F7E,
	0x5771, 0x602F, 0x69CE, 0x7462, 0x7FFF
};

APU::APU(std::shared_ptr<BusShared> shared, BusARM7& bus) : shared(shared), bus(bus) {
	//newSamples = new i16[SAMPLE_BUFFER_SIZE * 2];
	//outputSamples = new i16[SAMPLE_BUFFER_SIZE * 2];

	soundRunning = false;
}

APU::~APU() {
	//delete[] newSamples;
	//delete[] outputSamples;
}

void APU::reset() {
	sampleIndex = 0;

	for (int i = 0; i < 16; i++) {
		AudioChannel& chan = channel[i];

		chan.SOUNDCNT = 0;
		chan.SOUNDSAD = 0;
		chan.SOUNDTMR = 0;
		chan.SOUNDPNT = 0;
		chan.SOUNDLEN = 0;

		chan.inFifo = {};
		chan.fifoOffset = 0;
		chan.wordsRead = 0;

		chan.lastSample = 0;
		chan.lastSampleTimestamp = 0;
		chan.timerPeriod = 0;
		chan.lfsr = 0x7FFF;
	}

	SOUNDCNT = 0;
	SOUNDBIAS = 0x200;
	SNDCAP0CNT = SNDCAP1CNT = 0;
	SNDCAP0DAD = SNDCAP1DAD = 0;
	SNDCAP0LEN = SNDCAP1LEN = 0;

	shared->addEvent(67108864 / 32768, APU_SAMPLE);
}

void APU::doSample() {
	u16 leftSample, rightSample;
	static int num = 0;
	num++;

	if (masterEnable) {
		i32 totalLeft = 0;
		i32 totalRight = 0;
		int channelsUsed = 0;

		for (int i = 0; i < 16; i++) {
			auto& chan = channel[i];
			if (!chan.start)
				break;

			// 0 Incoming PCM16 Data          16.0  -8000h     +7FFFh
			while ((chan.lastSampleTimestamp + chan.timerPeriod) <= shared->currentTime) {
				switch (chan.format) {
				case 0: // PCM-8
					chan.lastSample = (chan.inFifo.front() >> (8 * chan.fifoOffset)) & 0xFF;
					chan.lastSample = (i16)(chan.lastSample << 8);

					chan.fifoOffset = (chan.fifoOffset + 1) & 3;
					break;
				case 1: // PCM-16
					chan.lastSample = (chan.inFifo.front() >> (16 * chan.fifoOffset)) & 0xFFFF;

					chan.fifoOffset = (chan.fifoOffset + 1) & 1;
					break;
				}

				if ((chan.format != 3) && (chan.fifoOffset == 0)) {
					chan.inFifo.pop();
					if (chan.inFifo.size() <= 4)
						fillFifo(i);

					if ((chan.repeatMode == 2) && (chan.wordsRead >= (chan.SOUNDPNT + chan.SOUNDLEN)))
						chan.start = false;
				}

				chan.lastSampleTimestamp += chan.timerPeriod;
			}
			i32 sample = chan.lastSample;

			// 1 Volume Divider (div 1..16)   16.4  -8000h     +7FFFh
			switch (chan.volumeDiv) {
			case 0: sample <<= 4; break; // Normal
			case 1: sample <<= 3; break; // Div2
			case 2: sample <<= 2; break; // Div4
			case 3: sample <<= 0; break; // Div16
			}

			// 2 Volume Factor (mul N/128)    16.11 -8000h     +7FFFh
			sample = (sample << 3) * chan.volumeDiv;

			// 3 Panning (mul N/128)          16.18 -8000h     +7FFFh
			// 4 Rounding Down (strip 10bit)  16.8  -8000h     +7FFFh
			// 5 Mixer (add channel 0..15)    20.8  -80000h    +7FFF0h
			++channelsUsed;
			totalLeft += (sample * ((127 - chan.panning) << 4)) >> 10;
			totalRight += (sample * (chan.panning << 4)) >> 10;
		}

		// 6 Master Volume (mul N/128/64) 14.21 -2000h     +1FF0h
		totalLeft *= masterVolume << 1;
		totalRight *= masterVolume << 1;

		// 7 Strip fraction               14.0  -2000h     +1FF0h
		totalLeft >>= 21;
		totalRight >>= 21;

		// 8 Add Bias (0..3FFh, def=200h) 15.0  -2000h+0   +1FF0h+3FFh
		totalLeft += SOUNDBIAS;
		totalRight += SOUNDBIAS;

		// 9 Clip (min/max 0h..3FFh)      10.0  0          +3FFh
		leftSample = std::clamp(totalLeft, 0, 0x3FF);
		rightSample = std::clamp(totalRight, 0, 0x3FF);
	} else {
		leftSample = rightSample = 0;
	}

	//newSamples[sampleIndex++] = (num & 4) ? 0x7FFF : 0x8000;//(leftSample << 6) | (leftSample >> 4);
	//newSamples[sampleIndex++] = (num & 4) ? 0x7FFF : 0x8000;//(rightSample << 6) | (rightSample >> 4);
	if (sampleIndex >= SAMPLE_BUFFER_SIZE * 2) {
		std::swap(newSamples, outputSamples);
		soundRunning = false;
		sampleIndex = 0;
	}

	shared->addEvent(67108864 / 32768, APU_SAMPLE);
}

void APU::fillFifo(int chanNum) {
	auto& chan = channel[chanNum];
	u32 address = chan.SOUNDSAD;

	for (int i = 0; i < 4; i++) {
		if (chan.wordsRead < chan.SOUNDPNT) {
			address = (chan.SOUNDSAD + (chan.wordsRead * 4)) & 0x07FFFFFC;
		} else {
			if (chan.SOUNDLEN != 0)
				address = (((((chan.wordsRead - chan.SOUNDPNT) % chan.SOUNDLEN) + chan.SOUNDPNT) * 4) + chan.SOUNDSAD) & 0x07FFFFFC;
		}
		auto val = bus.read<u32, false>(address, i > 0);
		//printf("address: %08X  val: %08X\n", address, val);
		chan.inFifo.push(val);
		++chan.wordsRead;
	}
}

void APU::startChannel(int chanNum) {
	auto& chan = channel[chanNum];

	chan.inFifo = {};
	chan.fifoOffset = 0;
	chan.wordsRead = 0;
	fillFifo(chanNum);

	chan.lastSample = 0;
	chan.lastSampleTimestamp = shared->currentTime;
	chan.timerPeriod = ((BUS_CLOCK_SPEED / 2) / chan.SOUNDTMR) * (INTERNAL_CLOCK_SPEED / BUS_CLOCK_SPEED);

	if (chanNum >= 14)
		chan.lfsr = 0x7FFF;
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
		shared->log << fmt::format("[NDS7 Bus][APU] Read from unknown IO register 0x{:0>7X}\n", address);
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
		case 0x3: {
			bool oldStart = chan.start;
			chan.SOUNDCNT = (chan.SOUNDCNT & 0x00FFFFFF) | ((value & 0xFF) << 24);

			if (!oldStart && chan.start)
				startChannel((address >> 4) & 0xF);
			} break;
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
		shared->log << fmt::format("[NDS7 Bus][APU] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}
