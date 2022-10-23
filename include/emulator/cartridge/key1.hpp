
#ifndef ORTIN_KEY1_HPP
#define ORTIN_KEY1_HPP

#include "types.hpp"

class KEY1 {
public:
	u32 keyBuf[0x1048 / 4];
	u32 keycode[3];

	void encrypt(u64 *data);
	void decrypt(u64 *data);
	void applyKeycode(u8 modulo);
	void initKeycode(u32 idcode, int level, u8 modulo);
};

#endif //ORTIN_KEY1_HPP
