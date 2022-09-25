#include "emulator/timer.hpp"

Timer::Timer(bool timer9, BusShared &shared, std::stringstream &log) : timer9(timer9), shared(shared), log(log) {
	//
}

Timer::~Timer() {
	//
}

void Timer::reset() {
	//
}

const u64 prescalerShifts[4] = { 1, 7, 9, 12 };

void Timer::updateCounter(int channel) {
	auto& tim = timer[channel];
	int shift = prescalerShifts[tim.prescaler];
	if (!tim.startStop || tim.cascade)
		return;

	tim.TIMCNT_L += (shared.currentTime - tim.lastIncrementTimestamp) >> shift;
	tim.lastIncrementTimestamp = shared.currentTime & ~((1 << shift) - 1);
}

void Timer::scheduleTimer(int channel) {
	auto& tim = timer[channel];
	shared.addEventAbsolute(((shared.currentTime >> prescalerShifts[tim.prescaler]) + (0x1000 - tim.TIMCNT_L)) << prescalerShifts[tim.prescaler], timer9 ? TIMER_OVERFLOW_9 : TIMER_OVERFLOW_7);
}

void Timer::checkOverflow() {
	bool overflow = false;
	for (int channel = 0; channel < 4; channel++) {
		auto& tim = timer[channel];
		if (!tim.startStop)
			break;

		if (tim.cascade) {
			if (overflow) {
				if (++tim.TIMCNT_L == 0) {
					if (tim.irqEnable)
						tim.interruptRequested = true;

					tim.TIMCNT_L = tim.reload;
				} else {
					overflow = false;
				}
			}
		} else {
			overflow = false;

			updateCounter(channel);
			if ((tim.TIMCNT_L == 0) && (tim.lastIncrementTimestamp == shared.currentTime)) { // Overflow
				if (tim.irqEnable)
					tim.interruptRequested = true;

				tim.TIMCNT_L = tim.reload;
				scheduleTimer(channel);

				overflow = true;
			}
		}
	}
}

u8 Timer::readIO(u32 address) {
	switch (address) {
	case 0x4000100:
		updateCounter(0);
		return (u8)(timer[0].TIMCNT_L >> 0);
	case 0x4000101:
		updateCounter(0);
		return (u8)(timer[0].TIMCNT_L >> 8);
	case 0x4000102:
		return (u8)(timer[0].TIMCNT_H >> 0);
	case 0x4000103:
		return (u8)(timer[0].TIMCNT_H >> 8);
	case 0x4000104:
		updateCounter(1);
		return (u8)(timer[1].TIMCNT_L >> 0);
	case 0x4000105:
		updateCounter(1);
		return (u8)(timer[1].TIMCNT_L >> 8);
	case 0x4000106:
		return (u8)(timer[1].TIMCNT_H >> 0);
	case 0x4000107:
		return (u8)(timer[1].TIMCNT_H >> 8);
	case 0x4000108:
		updateCounter(2);
		return (u8)(timer[2].TIMCNT_L >> 0);
	case 0x4000109:
		updateCounter(2);
		return (u8)(timer[2].TIMCNT_L >> 8);
	case 0x400010A:
		return (u8)(timer[2].TIMCNT_H >> 0);
	case 0x400010B:
		return (u8)(timer[2].TIMCNT_H >> 8);
	case 0x400010C:
		updateCounter(3);
		return (u8)(timer[3].TIMCNT_L >> 0);
	case 0x400010D:
		updateCounter(3);
		return (u8)(timer[3].TIMCNT_L >> 8);
	case 0x400010E:
		return (u8)(timer[3].TIMCNT_H >> 0);
	case 0x400010F:
		return (u8)(timer[3].TIMCNT_H >> 8);
	default:
		log << fmt::format("[NDS{} Bus][Timer] Read from unknown IO register 0x{:0>7X}\n", timer9 ? 9 : 7, address);
		return 0;
	}
}

void Timer::writeIO(u32 address, u8 value) {
	switch (address) {
	case 0x4000100:
		timer[0].reload = (timer[0].reload & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000101:
		timer[0].reload = (timer[0].reload & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000102: {
		auto& tim = timer[0];
		bool oldStartStop = tim.startStop;

		updateCounter(0);
		tim.TIMCNT_H = (tim.TIMCNT_H & 0xFF00) | ((value & 0xC7) << 0);
		updateCounter(0);

		if (!oldStartStop && tim.startStop) // Rising edge
			tim.TIMCNT_L = tim.reload;

		if (tim.startStop && !tim.cascade)
			scheduleTimer(0);
		} break;
	case 0x4000103:
		break;
	case 0x4000104:
		timer[1].reload = (timer[1].reload & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000105:
		timer[1].reload = (timer[1].reload & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x4000106: {
		auto& tim = timer[1];
		bool oldStartStop = tim.startStop;

		updateCounter(1);
		tim.TIMCNT_H = (tim.TIMCNT_H & 0xFF00) | ((value & 0xC7) << 0);
		updateCounter(1);

		if (!oldStartStop && tim.startStop) // Rising edge
			tim.TIMCNT_L = tim.reload;

		if (tim.startStop && !tim.cascade)
			scheduleTimer(1);
	} break;
	case 0x4000107:
		break;
	case 0x4000108:
		timer[2].reload = (timer[2].reload & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x4000109:
		timer[2].reload = (timer[2].reload & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400010A: {
		auto& tim = timer[2];
		bool oldStartStop = tim.startStop;

		updateCounter(2);
		tim.TIMCNT_H = (tim.TIMCNT_H & 0xFF00) | ((value & 0xC7) << 0);
		updateCounter(2);

		if (!oldStartStop && tim.startStop) // Rising edge
			tim.TIMCNT_L = tim.reload;

		if (tim.startStop && !tim.cascade)
			scheduleTimer(2);
	} break;
	case 0x400010B:
		break;
	case 0x400010C:
		timer[3].reload = (timer[3].reload & 0xFF00) | ((value & 0xFF) << 0);
		break;
	case 0x400010D:
		timer[3].reload = (timer[3].reload & 0x00FF) | ((value & 0xFF) << 8);
		break;
	case 0x400010E: {
		auto& tim = timer[3];
		bool oldStartStop = tim.startStop;

		updateCounter(3);
		tim.TIMCNT_H = (tim.TIMCNT_H & 0xFF00) | ((value & 0xC7) << 0);
		updateCounter(3);

		if (!oldStartStop && tim.startStop) // Rising edge
			tim.TIMCNT_L = tim.reload;

		if (tim.startStop && !tim.cascade)
			scheduleTimer(3);
	} break;
	case 0x400010F:
		break;
	default:
		log << fmt::format("[NDS{} Bus][Timer] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", timer9 ? 9 : 7, address, value);
		break;
	}
}