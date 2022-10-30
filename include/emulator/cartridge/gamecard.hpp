#ifndef ORTIN_GAMECARD_HPP
#define ORTIN_GAMECARD_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"
#include "key1.hpp"

class Gamecard {
public:
	std::shared_ptr<BusShared> shared;

	// External Use
	bool logGamecard;

	Gamecard(std::shared_ptr<BusShared> shared);
	~Gamecard();
	void reset();

	// Internal Use
	u64 currentCommand;
	u32 bytesRead;
	u32 dataBlockSizeBytes;
	KEY1 level2;
	KEY1 level3;

	void sendCommand();
	void readMoreData();

	// Cartridge State
	enum {
		UNENCRYPTED,
		KEY1,
		KEY2
	} encryptionMode;
	union {
		struct {
			u32 manufacturer : 8;
			u32 cartSize : 8;
			u32 cartFlags1 : 8;
			u32 cartFlags2 : 8;
		};
		u32 chipId;
	};
	u8 *romData;
	size_t romSize;
	size_t romSizeMax;

	// Memory Interface
	u8 readIO9(u32 address, bool final);
	void writeIO9(u32 address, u8 value);
	u8 readIO7(u32 address, bool final);
	void writeIO7(u32 address, u8 value);

	// I/O Registers
	union {
		struct {
			u16 spiBaudrate : 2;
			u16 : 4;
			u16 spiHoldChipselect : 1;
			u16 spiBusy : 1;
			u16 : 5;
			u16 slotMode : 1;
			u16 transferReadyIrq : 1;
			u16 slotEnable : 1;
		};
		u16 AUXSPICNT; // NDS9/NDS7 - 0x40001A0
	};

	union {
		struct {
			u32 key1Gap1Length : 13;
			u32 key2EncryptData : 1;
			u32 : 1;
			u32 key2ApplySeed : 1;
			u32 key1Gap2Length : 6;
			u32 key2EncryptCommand : 1;
			u32 dataReady : 1;
			u32 dataBlockSize : 3;
			u32 transferClockRate : 1;
			u32 key1GapClocks : 1;
			u32 releaseReset : 1;
			u32 dataDirection : 1;
			u32 blockStart : 1;
		};
		u32 ROMCTRL; // NDS9/NDS7 - 0x40001A4
	};
	u64 nextCartridgeCommand; // NDS9/NDS7 0x40001A8
	u32 key2Seed0Low9; // NDS9 0x40001B0
	u32 key2Seed0Low7; // NDS7 0x40001B0
	u32 key2Seed1Low9; // NDS9 0x40001B4
	u32 key2Seed1Low7; // NDS7 0x40001B4
	u16 key2Seed0High9; // NDS9 0x40001B8
	u16 key2Seed0High7; // NDS7 0x40001B8
	u16 key2Seed1High9; // NDS9 0x40001BA
	u16 key2Seed1High7; // NDS7 0x40001BA
	u32 cartridgeReadData; // NDS9/NDS7 0x4100010
};

#endif //ORTIN_GAMECARD_HPP