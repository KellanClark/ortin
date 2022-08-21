
#include "emulator/nds9/dsmath.hpp"

DSMath::DSMath(BusShared &shared, std::stringstream &log) : shared(shared), log(log) {
	//
}

DSMath::~DSMath() {
	//
}

void DSMath::reset() {
	DIVCNT = DIV_NUMER = DIV_DENOM = DIV_RESULT = DIVREM_RESULT = 0;
	SQRTCNT = SQRT_RESULT = SQRT_PARAM = 0;

	divFinishTimestamp = sqrtFinishTimestamp = 0;
}

u8 DSMath::readIO9(u32 address, bool final) {
	switch (address) {
	case 0x4000280:
		return (u8)(DIVCNT >> 0);
	case 0x4000281:
		divBusy = divFinishTimestamp < shared.currentTime;

		return (u8)(DIVCNT >> 8);
	case 0x4000282:
	case 0x4000283:
		return 0;
	case 0x4000290:
		return (u8)(DIV_NUMER >> 0);
	case 0x4000291:
		return (u8)(DIV_NUMER >> 8);
	case 0x4000292:
		return (u8)(DIV_NUMER >> 16);
	case 0x4000293:
		return (u8)(DIV_NUMER >> 24);
	case 0x4000294:
		return (u8)(DIV_NUMER >> 32);
	case 0x4000295:
		return (u8)(DIV_NUMER >> 40);
	case 0x4000296:
		return (u8)(DIV_NUMER >> 48);
	case 0x4000297:
		return (u8)(DIV_NUMER >> 56);
	case 0x4000298:
		return (u8)(DIV_DENOM >> 0);
	case 0x4000299:
		return (u8)(DIV_DENOM >> 8);
	case 0x400029A:
		return (u8)(DIV_DENOM >> 16);
	case 0x400029B:
		return (u8)(DIV_DENOM >> 24);
	case 0x400029C:
		return (u8)(DIV_DENOM >> 32);
	case 0x400029D:
		return (u8)(DIV_DENOM >> 40);
	case 0x400029E:
		return (u8)(DIV_DENOM >> 48);
	case 0x400029F:
		return (u8)(DIV_DENOM >> 56);
	case 0x40002A0:
		return (u8)(DIV_RESULT >> 0);
	case 0x40002A1:
		return (u8)(DIV_RESULT >> 8);
	case 0x40002A2:
		return (u8)(DIV_RESULT >> 16);
	case 0x40002A3:
		return (u8)(DIV_RESULT >> 24);
	case 0x40002A4:
		return (u8)(DIV_RESULT >> 32);
	case 0x40002A5:
		return (u8)(DIV_RESULT >> 40);
	case 0x40002A6:
		return (u8)(DIV_RESULT >> 48);
	case 0x40002A7:
		return (u8)(DIV_RESULT >> 56);
	case 0x40002A8:
		return (u8)(DIVREM_RESULT >> 0);
	case 0x40002A9:
		return (u8)(DIVREM_RESULT >> 8);
	case 0x40002AA:
		return (u8)(DIVREM_RESULT >> 16);
	case 0x40002AB:
		return (u8)(DIVREM_RESULT >> 24);
	case 0x40002AC:
		return (u8)(DIVREM_RESULT >> 32);
	case 0x40002AD:
		return (u8)(DIVREM_RESULT >> 40);
	case 0x40002AE:
		return (u8)(DIVREM_RESULT >> 48);
	case 0x40002AF:
		return (u8)(DIVREM_RESULT >> 56);
	case 0x40002B0:
		return (u8)(SQRTCNT >> 0);
	case 0x40002B1:
		sqrtBusy = sqrtFinishTimestamp < shared.currentTime;

		return (u8)(SQRTCNT >> 8);
	case 0x40002B2:
	case 0x40002B3:
		return 0;
	case 0x40002B4:
		return (u8)(SQRT_RESULT >> 0);
	case 0x40002B5:
		return (u8)(SQRT_RESULT >> 8);
	case 0x40002B6:
		return (u8)(SQRT_RESULT >> 16);
	case 0x40002B7:
		return (u8)(SQRT_RESULT >> 24);
	case 0x40002B8:
		return (u8)(SQRT_PARAM >> 0);
	case 0x40002B9:
		return (u8)(SQRT_PARAM >> 8);
	case 0x40002BA:
		return (u8)(SQRT_PARAM >> 16);
	case 0x40002BB:
		return (u8)(SQRT_PARAM >> 24);
	case 0x40002BC:
		return (u8)(SQRT_PARAM >> 32);
	case 0x40002BD:
		return (u8)(SQRT_PARAM >> 40);
	case 0x40002BE:
		return (u8)(SQRT_PARAM >> 48);
	case 0x40002BF:
		return (u8)(SQRT_PARAM >> 56);
	default:
		log << fmt::format("[NDS9 Bus][DSMath] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void DSMath::writeIO9(u32 address, u8 value, bool final) {
	switch (address) {
	case 0x4000280:
		DIVCNT = (DIVCNT & 0xFF00) | ((value & 0x03) << 0);
		break;
	case 0x4000290:
		DIV_NUMER = (DIV_NUMER & 0xFFFFFFFFFFFFFF00) | ((u64)value << 0);
		break;
	case 0x4000291:
		DIV_NUMER = (DIV_NUMER & 0xFFFFFFFFFFFF00FF) | ((u64)value << 8);
		break;
	case 0x4000292:
		DIV_NUMER = (DIV_NUMER & 0xFFFFFFFFFF00FFFF) | ((u64)value << 16);
		break;
	case 0x4000293:
		DIV_NUMER = (DIV_NUMER & 0xFFFFFFFF00FFFFFF) | ((u64)value << 24);
		break;
	case 0x4000294:
		DIV_NUMER = (DIV_NUMER & 0xFFFFFF00FFFFFFFF) | ((u64)value << 32);
		break;
	case 0x4000295:
		DIV_NUMER = (DIV_NUMER & 0xFFFF00FFFFFFFFFF) | ((u64)value << 40);
		break;
	case 0x4000296:
		DIV_NUMER = (DIV_NUMER & 0xFF00FFFFFFFFFFFF) | ((u64)value << 48);
		break;
	case 0x4000297:
		DIV_NUMER = (DIV_NUMER & 0x00FFFFFFFFFFFFFF) | ((u64)value << 56);
		break;
	case 0x4000298:
		DIV_DENOM = (DIV_DENOM & 0xFFFFFFFFFFFFFF00) | ((u64)value << 0);
		break;
	case 0x4000299:
		DIV_DENOM = (DIV_DENOM & 0xFFFFFFFFFFFF00FF) | ((u64)value << 8);
		break;
	case 0x400029A:
		DIV_DENOM = (DIV_DENOM & 0xFFFFFFFFFF00FFFF) | ((u64)value << 16);
		break;
	case 0x400029B:
		DIV_DENOM = (DIV_DENOM & 0xFFFFFFFF00FFFFFF) | ((u64)value << 24);
		break;
	case 0x400029C:
		DIV_DENOM = (DIV_DENOM & 0xFFFFFF00FFFFFFFF) | ((u64)value << 32);
		break;
	case 0x400029D:
		DIV_DENOM = (DIV_DENOM & 0xFFFF00FFFFFFFFFF) | ((u64)value << 40);
		break;
	case 0x400029E:
		DIV_DENOM = (DIV_DENOM & 0xFF00FFFFFFFFFFFF) | ((u64)value << 48);
		break;
	case 0x400029F:
		DIV_DENOM = (DIV_DENOM & 0x00FFFFFFFFFFFFFF) | ((u64)value << 56);
		break;
	case 0x40002B0:
		SQRTCNT = (SQRTCNT & 0xFF00) | ((value & 0x01) << 0);
		break;
	case 0x40002B8:
		SQRT_PARAM = (SQRT_PARAM & 0xFFFFFFFFFFFFFF00) | ((u64)value << 0);
		break;
	case 0x40002B9:
		SQRT_PARAM = (SQRT_PARAM & 0xFFFFFFFFFFFF00FF) | ((u64)value << 8);
		break;
	case 0x40002BA:
		SQRT_PARAM = (SQRT_PARAM & 0xFFFFFFFFFF00FFFF) | ((u64)value << 16);
		break;
	case 0x40002BB:
		SQRT_PARAM = (SQRT_PARAM & 0xFFFFFFFF00FFFFFF) | ((u64)value << 24);
		break;
	case 0x40002BC:
		SQRT_PARAM = (SQRT_PARAM & 0xFFFFFF00FFFFFFFF) | ((u64)value << 32);
		break;
	case 0x40002BD:
		SQRT_PARAM = (SQRT_PARAM & 0xFFFF00FFFFFFFFFF) | ((u64)value << 40);
		break;
	case 0x40002BE:
		SQRT_PARAM = (SQRT_PARAM & 0xFF00FFFFFFFFFFFF) | ((u64)value << 48);
		break;
	case 0x40002BF:
		SQRT_PARAM = (SQRT_PARAM & 0x00FFFFFFFFFFFFFF) | ((u64)value << 56);
		break;
	default:
		log << fmt::format("[NDS9 Bus][DSMath] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}

	if (final) {
		if (((address >= 0x4000280) && (address <= 0x4000283)) || ((address >= 0x4000290) && (address <= 0x400029F))) { // Write to division register
			// > The DENOM=0 check relies on the full 64bit value (so, in 32bit mode, the flag works only if the unused upper 32bit of DENOM are zero).
			div0 = DIV_DENOM == 0;

			switch (divMode) {
			case 0: // 32/32 = 32,32
				divFinishTimestamp = shared.currentTime + (18 * 2);

				if ((i32)DIV_DENOM == 0) { [[unlikely]]
					DIV_RESULT = ((i32)DIV_NUMER > -1) ? 0x00000000FFFFFFFF : 0xFFFFFFFF00000001;
					DIVREM_RESULT = (i32)DIV_NUMER;
				} else if (((i32)DIV_NUMER == 0x80000000) && ((i32)DIV_DENOM == -1)) { [[unlikely]]
					DIV_RESULT = 0x80000000;
					DIVREM_RESULT = 0;
				} else {
					DIV_RESULT = (i32)DIV_NUMER / (i32)DIV_DENOM;
					DIVREM_RESULT = (i32)DIV_NUMER % (i32)DIV_DENOM;
				}
				break;
			case 3:
			case 1: // 64/32 = 64,32
				divFinishTimestamp = shared.currentTime + (34 * 2);

				if ((i32)DIV_DENOM == 0) { [[unlikely]]
					DIV_RESULT = (DIV_NUMER > -1) ? -1 : 1;
					DIVREM_RESULT = DIV_NUMER;
				} else if ((DIV_NUMER == 0x8000000000000000) && ((i32)DIV_DENOM == -1)) { [[unlikely]]
					DIV_RESULT = 0x8000000000000000;
					DIVREM_RESULT = 0;
				} else {
					DIV_RESULT = DIV_NUMER / (i32)DIV_DENOM;
					DIVREM_RESULT = DIV_NUMER % (i32)DIV_DENOM;
				}
				break;
			case 2: // 64/64 = 64,64
				divFinishTimestamp = shared.currentTime + (34 * 2);

				if (DIV_DENOM == 0) { [[unlikely]]
					DIV_RESULT = (DIV_NUMER > -1) ? -1 : 1;
					DIVREM_RESULT = DIV_NUMER;
				} else if ((DIV_NUMER == 0x8000000000000000) && (DIV_DENOM == -1)) { [[unlikely]]
					DIV_RESULT = 0x8000000000000000;
					DIVREM_RESULT = 0;
				} else {
					DIV_RESULT = DIV_NUMER / DIV_DENOM;
					DIVREM_RESULT = DIV_NUMER % DIV_DENOM;
				}
				break;
			}
		}

		if (((address >= 0x40002B0) && (address <= 0x40002B3)) || ((address >= 0x40002B8) && (address <= 0x40002BF))) { // Write to square root register
			sqrtFinishTimestamp = shared.currentTime + (13 * 2);

			if (sqrtMode) { // 64 bit
				// I'm not going to write inline x87 just for a 64 bit mantissa, so software it is.
				// https://www.cplusplus.in/square-root-of-an-integer-in-c/
				auto floorSqrt = [](u64 n) -> u32 {
					if (n == 0) return 0;
					u64 left = 1;
					u64 right = (n / 2) + 1;
					u64 res;

					while (left <= right) {
						u64 mid = left + ((right - left) / 2);
						if (mid <= (n / mid)){
							left = mid + 1;
							res = mid;
						}
						else {
							right = mid - 1;
						}
					}

					return res;
				};

				SQRT_RESULT = floorSqrt(SQRT_PARAM);
			} else { // 32 bit
				// I can get away with using the standard library here as long as I cast to a double
				SQRT_RESULT = std::sqrt((double)(u32)SQRT_PARAM);
			}
		}
	}
}