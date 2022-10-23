
#include "emulator/cartridge/key1.hpp"

// KEY1 functions are adapted from GBATEK
void KEY1::encrypt(u64 *data) {
	u32 y = *data & 0xFFFFFFFF;
	u32 x = *data >> 32;

	for (int i = 0x00; i <= 0x0F; i++) {
		u32 z = keyBuf[i] ^ x;
		x = keyBuf[0x012 + ((z >> 24) & 0xFF)];
		x = keyBuf[0x112 + ((z >> 16) & 0xFF)] + x;
		x = keyBuf[0x212 + ((z >>  8) & 0xFF)] ^ x;
		x = keyBuf[0x312 + ((z >>  0) & 0xFF)] + x;
		x = y ^ x;
		y = z;
	}

	*data = ((u64)x ^ keyBuf[0x10]) | (((u64)y ^ keyBuf[0x11]) << 32);
}

void KEY1::decrypt(u64 *data) {
	u32 y = *data & 0xFFFFFFFF;
	u32 x = *data >> 32;

	for (int i = 0x11; i >= 0x02; i--) {
		u32 z = keyBuf[i] ^ x;
		x = keyBuf[0x012 + ((z >> 24) & 0xFF)];
		x = keyBuf[0x112 + ((z >> 16) & 0xFF)] + x;
		x = keyBuf[0x212 + ((z >>  8) & 0xFF)] ^ x;
		x = keyBuf[0x312 + ((z >>  0) & 0xFF)] + x;
		x = y ^ x;
		y = z;
	}

	*data = ((u64)x ^ keyBuf[1]) | (((u64)y ^ keyBuf[0]) << 32);
}

void KEY1::applyKeycode(u8 modulo) {
	encrypt((u64 *)&keycode[1]);
	encrypt((u64 *)&keycode[0]);
	u64 scratch = 0;

	for (int i = 0; i <= 0x11; i++) {
		keyBuf[i] ^= std::byteswap<u32>(keycode[i % (modulo >> 2)]);
	}

	for (int i = 0; i <= 0x410; i += 2) {
		encrypt(&scratch);
		keyBuf[i] = scratch >> 32;
		keyBuf[i + 1] = scratch & 0xFFFFFFFF;
	}
}

void KEY1::initKeycode(u32 idcode, int level, u8 modulo) {
	keycode[0] = idcode;
	keycode[1] = idcode / 2;
	keycode[2] = idcode * 2;
	if (level >= 1) applyKeycode(modulo);
	if (level >= 2) applyKeycode(modulo);
	keycode[1] *= 2;
	keycode[2] /= 2;
	if (level >= 3) applyKeycode(modulo);
}