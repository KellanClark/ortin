
#include "emulator/nds7/rtc.hpp"

#include <cassert>

// https://stackoverflow.com/a/2602885
u8 reverseBits(u8 b) {
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}

u8 toBcd(int num) {
	return (((num % 100) / 10) << 4) | (num % 10);
}

int fromBcd(u8 num) {
	return (((num & 0xF0) >> 4) * 10) + (num & 0xF);
}

static constexpr u64 toRtcTime(u64 time) {
	return time >> 10; // RTC works in 64kHz increments
}

static constexpr u64 fromRtcTime(u64 time) {
	return time << 10;
}

RTC::RTC(BusShared &shared, std::stringstream &log) : shared(shared), log(log) {
	logRtc = true;
}

RTC::~RTC() {
	//
}

void RTC::reset() {
	bus = 0;

	clockAdjustmentRegister = 0;
	freeRegister = 0;

	bitsSent = 0;
	sentData = 0;
	readBuf = 0;
	rtcTime = -1;
	refresh<false>();

	commandRegister = 0;
	statusRegister1 = 0;
	hourMode = 1;
	statusRegister2 = 0;
	auto tt = time(0);
	syncToRealTime(&tt);
	alarm1.raw = alarm2.raw = 0;
	alarm1.frequency = 0;
	clockAdjustmentRegister = 0;
	freeRegister = 0;
}

void RTC::syncToRealTime(time_t *tt) {
	std::tm* tm = localtime(tt);

	year = toBcd(tm->tm_year);
	month = toBcd(tm->tm_mon + 1);
	day = toBcd(tm->tm_mday);
	weekday = tm->tm_wday;
	minute = toBcd(tm->tm_min);
	second = toBcd(tm->tm_sec);

	hour = tm->tm_hour;
	pmAm = (hour >= 12);
	hour = toBcd(hour);
	//std::cout << fmt::format("{} {} {:0>8b}\n", tm->tm_hour, (u8)pmAm, (u8)hour);
}

static const int monthLengths[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

template <bool scheduled> // My code really should be structured to work without this, but it allows me to ignore a lot of special cases in normal operation.
void RTC::refresh() {
	if (scheduled && (toRtcTime(shared.currentTime) == rtcTime))
		return;
	rtcTime = toRtcTime(shared.currentTime);
	bool oldInterruptRequested = interrupt1Flag || interrupt2Flag;

	bool newSecond = (toRtcTime(shared.currentTime) & 0xFFFF) == 0; // One second has passed
	if (newSecond) { // Update time
		second = fromBcd(second) + 1;
		if (second == 60) {
			second = 0;

			minute = fromBcd(minute) + 1;
			if (minute == 60) {
				minute = 0;

				hour = fromBcd(hour) + 1;
				if (hour == 24) {
					hour = 0;

					weekday = weekday + 1;
					if (weekday == 7)
						weekday = 0;

					day = fromBcd(day);
					if (((day == 29) && (year & 3)) || (day > monthLengths[month])) {
						day = 1;

						month = fromBcd(month) + 1;
						if (month == 13) {
							month = 1;

							year = fromBcd(year) + 1;
							if (year == 100) {
								year = 0;
							} else {
								year = toBcd(year);
							}
						} else {
							month = toBcd(month);
						}
					} else {
						day = toBcd(day);
					}
				} else {
					hour = toBcd(hour);
				}
				pmAm = hour >= 0x12;
			} else {
				minute = fromBcd(minute);
			}
		} else {
			second = fromBcd(second);
		}

		shared.addEvent(fromRtcTime(0x10000), RTC_REFRESH);
	}

	switch (interrupt1Mode) {
	case 0b0000: // Disable
		interrupt1Flag = 0;
		break;
	case 0b0001:
	case 0b0101: { // Selected frequency interrupt
		u64 adjustedFrequency = (reverseBits(alarm1.frequency) & 0xF8) << 8;
		u64 mask = ~((adjustedFrequency & -adjustedFrequency) - 1);
		if ((rtcTime & 1) || (adjustedFrequency == 0)) [[unlikely]]
			break;

		if ((adjustedFrequency & rtcTime) == adjustedFrequency) {
			interrupt1Flag = true;
			//u64 temp = _pdep_u64((_pext_u64(rtcTime, ~adjustedFrequency) + (1 << std::countr_zero(adjustedFrequency))) & ~((1 << std::countr_zero(adjustedFrequency)) - 1), ~adjustedFrequency) | adjustedFrequency;
			u64 temp = (((rtcTime | adjustedFrequency) + (adjustedFrequency & -adjustedFrequency)) | adjustedFrequency) & mask;
			shared.addEventAbsolute(fromRtcTime(temp), RTC_REFRESH);
			if (scheduled) shared.addEvent(0, SERIAL_INTERRUPT); // Stupid workaround
			//printf("%02llX  %016llX  %016llX\n", adjustedFrequency, rtcTime, temp);
		} else {
			interrupt1Flag = false;
			u64 temp = (rtcTime | adjustedFrequency) & mask;
			shared.addEventAbsolute(fromRtcTime(temp), RTC_REFRESH);
			//printf("%02llX  %016llX  %016llX\n", adjustedFrequency, rtcTime, temp);
		}
		} break;
	case 0b0010:
	case 0b0110: // Per-minute edge interrupt
		if (newSecond && (second == 0))
			interrupt1Flag = true;
		break;
	case 0b0011: // Per-minute steady interrupt 1
		interrupt1Flag = second >= 30;
		break;
	case 0b0100: // Alarm 1 interrupt
		if (!(alarm1.dayCompareEnable ^ (alarm1.weekday == weekday)) &&
			!(alarm1.hourCompareEnable ^ ((alarm1.pmAm == pmAm) && (alarm1.hour == ((!hourMode && pmAm) ? (hour - 0x12) : hour)))) &&
			!(alarm1.minuteCompareEnable ^ (alarm1.minute == minute))) {
			interrupt1Flag = true;
		}
		break;
	case 0b0111: // Per-minute steady interrupt 2
		// Documentation says 7.9ms high. I'm going with 7.8125 because it makes more sense.
		if ((second == 0) && ((rtcTime & 0xFFFF) < 512)) {
			interrupt1Flag = true;
			shared.addEvent(fromRtcTime(512 - (rtcTime & 0xFFFF)), RTC_REFRESH);
		} else {
			interrupt1Flag = false;
		}
		break;
	case 0b1000 ... 0b1111: // 32kHz interrupt
		interrupt1Flag = rtcTime & 1;
		shared.addEvent(fromRtcTime(1), RTC_REFRESH);
		break;
	}

	if (interrupt2Mode) {
		if (!(alarm2.dayCompareEnable ^ (alarm2.weekday == weekday)) &&
			!(alarm2.hourCompareEnable ^ ((alarm2.pmAm == pmAm) && (alarm2.hour == ((!hourMode && pmAm) ? (hour - 0x12) : hour)))) &&
			!(alarm2.minuteCompareEnable ^ (alarm2.minute == minute))) {
			interrupt2Flag = true;
		}
	} else {
		interrupt2Flag = false;
	}

	if (!oldInterruptRequested && (interrupt1Flag || interrupt2Flag))
		shared.addEvent(0, SERIAL_INTERRUPT);
}
template void RTC::refresh<false>();
template void RTC::refresh<true>();

// It can be assumed the address will always be 0x4000138
u8 RTC::readIO7() {
	if (dataDirection) {
		return bus;
	} else {
		if (logRtc)
			log << fmt::format("[NDS7][RTC] Read bit: {}\n", readBuf & 1);
		return (bus & 0xFE) | (readBuf & 1);
	}
}

void RTC::writeIO7(u8 value) {
	//std::cout << fmt::format("Wrote {:0>8b}\n", value);
	bool oldClock = clockOut;
	bool oldSelect = selectOut;

	bus = value;

	if (selectOut) {
		if (oldClock && !clockOut) { // Continue read/write
			readBuf >>= 1;

			if (dataDirection) { // Write
				sentData |= data << bitsSent++;

				int byteNum = bitsSent / 8;

				if ((bitsSent % 8) == 0) {
					if (byteNum == 1) {
						commandRegister = sentData & 0xFF;

						if ((commandRegister & 0xF0) != 0b0110'0000)
							commandRegister = reverseBits(commandRegister);
						if ((commandRegister & 0xF0) != 0b0110'0000)
							log << fmt::format("[NDS7][RTC] Invalid control command {:0>8b}\n", reverseBits(commandRegister));
						if (logRtc)
							log << fmt::format("[NDS7][RTC] Command: {:0>8b} {} register {}\n", (u8)command, parameterReadWrite ? "Reading" : "Writing", commandRegister);

						if (parameterReadWrite) { // Reading register
							switch (command) {
							case 0:
								readBuf = statusRegister1 & 0xFE;
								statusRegister1 &= 0x0F;
								break;
							case 1:
								readBuf = statusRegister2;
								break;
							case 2:
								if (hourMode) {
									readBuf = dateTimeRegister;
								} else {
									readBuf = (dateTimeRegister & (u64)0xFF'FF'00'FF'FF'FF'FF) | ((u64)((pmAm << 6) | (pmAm ? (hour - 12) : hour)) << 32);
								}
								break;
							case 3:
								if (hourMode) {
									readBuf = dateTimeRegister >> 32;
								} else {
									readBuf = ((dateTimeRegister >> 32) & 0xFF'FF'00) | (pmAm << 6) | (pmAm ? (hour - 12) : hour);
								}
								break;
							case 4:
								if (interrupt1Mode & 4) {
									readBuf = alarm1.raw;
								} else {
									readBuf = alarm1.frequency;
								}
								break;
							case 5:
								readBuf = alarm2.raw;
								break;
							case 6:
								readBuf = clockAdjustmentRegister;
								break;
							case 7:
								readBuf = freeRegister;
								break;
							}
							readBuf <<= 1;
						}
					} else {
						u8 writtenData = sentData >> (bitsSent - 8);
						if (logRtc)
							log << fmt::format("[NDS7][RTC] Parameter: {:0>8b}\n", writtenData);

						switch (command) {
						case 0:
							statusRegister1 = writtenData & 0x0F;

							if (resetBit) { // TODO: Figure out what reset does
								resetBit = 0;
							}
							break;
						case 1:
							statusRegister2 = writtenData & 0x7F;
							refresh<false>();
							break;
						case 4:
							//assert(((interrupt1Mode & 7) == 4) || ((interrupt1Mode & 3) == 1));

							if (interrupt1Mode & 4) {
								readBuf = alarm1.raw;
								switch (byteNum) {
								case 2:
									alarm1.raw = (alarm1.raw & 0xFFFF00) | (writtenData & 0x87);
									break;
								case 3:
									alarm1.raw = (alarm1.raw & 0xFF00FF) | (writtenData << 8);
									break;
								case 4:
									alarm1.raw = (alarm1.raw & 0x00FFFF) | (writtenData << 16);
									break;
								}
							} else {
								if (byteNum == 2)
									alarm1.frequency = writtenData;
							}
							refresh<false>();
							break;
						case 6:
							clockAdjustmentRegister = writtenData & 0xFF;
							break;
						case 7:
							freeRegister = writtenData & 0xFF;
							break;
						}
					}
				}
			}
		}
	} else if (oldSelect && !selectOut) { // End command
		bitsSent = 0;
		sentData = 0;
		readBuf = 0;
	}
}