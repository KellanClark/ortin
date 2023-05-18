#include "emulator/cartridge/gamecard.hpp"

Gamecard::Gamecard(std::shared_ptr<BusShared> shared) : shared(shared) {
	logGamecard = true;

	memset(level1.keyBuf, 0, sizeof(level1.keyBuf));
	memset(level2.keyBuf, 0, sizeof(level2.keyBuf));
	memset(level3.keyBuf, 0, sizeof(level3.keyBuf));

	chipId = 0xFFFFFFFF;
	romData = NULL;
	romSize = 0;
}

Gamecard::~Gamecard() {
	//
}

void Gamecard::reset() {
	AUXSPICNT = 0;
	ROMCTRL = 0x00800000;
	key2Seed0Low9 = key2Seed0Low7 = 0;
	key2Seed1Low9 = key2Seed1Low7 = 0;
	key2Seed0High9 = key2Seed0High7 = 0;
	key2Seed1High9 = key2Seed1High7 = 0;

	currentCommand = 0;
	bytesRead = 0;
	dataBlockSizeBytes = 0;

	encryptionMode = ENCRYPTION_UNENCRYPTED;

	nextCartridgeCommand = 0;
	cartridgeReadData = 0xFFFFFFFF;

	// Initialize encryption and secure area
	u32 gamecode = *(u32 *)(romData + 0xC);
	level1.initKeycode(gamecode, 1, 0x8);
	level2.initKeycode(gamecode, 2, 0x8);
	level3.initKeycode(gamecode, 3, 0x8);

	for (int i = 0; i < 0x800; i += 8)
		level3.encrypt((u64 *)(romData + 0x4000 + i));
	level2.encrypt((u64 *)(romData + 0x4000));
	//level1.encrypt((u64 *)(romData + 0x78));
}

void Gamecard::sendCommand() {
	currentCommand = std::byteswap(nextCartridgeCommand);
	bytesRead = 0;
	if (dataBlockSize == 0) {
		dataBlockSizeBytes = 0;
	} else if (dataBlockSize == 7) {
		dataBlockSizeBytes = 4;
	} else {
		dataBlockSizeBytes = 0x100 << dataBlockSize;
	}

	if (encryptionMode == ENCRYPTION_KEY1)
		level2.decrypt(&currentCommand);

	if (logGamecard) {
		shared->log << fmt::format("[Gamecard] Command 0x{:0>16X} (0x{:X} bytes) - ", currentCommand, dataBlockSizeBytes);
		switch (currentCommand >> 56) {
		case 0x00: shared->log << "Get Header (from address 00000000h)\n"; break;
		case 0x10 ... 0x1F: shared->log << "KEY1 Get ROM Chip ID\n"; break;
		case 0x20 ... 0x2F: shared->log << "Get Secure Area Block\n"; break;
		case 0x3C: shared->log << "Activate KEY1 Encryption Mode\n"; break;
		case 0x40 ... 0x4F: shared->log << "Activate KEY2 Encryption Mode\n"; break;
		case 0x90: shared->log << "Unencrypted Get ROM Chip ID\n"; break;
		case 0x9F: shared->log << "Dummy\n"; break;
		case 0xA0 ... 0xAF: shared->log << "Enter Main Data Mode\n"; break;
		case 0xB7: shared->log << "Get Data\n"; break;
		case 0xB8: shared->log << "KEY2 Get ROM Chip ID\n"; break;
		default: { u64 tmp = currentCommand; level2.decrypt(&tmp); shared->log << fmt::format("Unknown 0x{:0>16X}\n", tmp); } break;
		}
	}

	readMoreData();
}

void Gamecard::readMoreData() {
	switch (encryptionMode){
	case ENCRYPTION_UNENCRYPTED:
		switch (currentCommand >> 56) {
		case 0x00: // Get Header (from address 00000000h)
			//shared->log << fmt::format("[Gamecard] Address 0x{:0>4X}\n", (bytesRead & 0xFFF));
			memcpy(&cartridgeReadData, romData + (bytesRead & 0xFFF), 4);
			break;
		case 0x3C: // Activate KEY1 Encryption Mode
			if (bytesRead == 0) {
				encryptionMode = ENCRYPTION_KEY1;
			}
			cartridgeReadData = 0xFFFFFFFF;
			break;
		case 0x90: // Chip ID - Unencrypted
			cartridgeReadData = chipId;
			break;
		case 0x9F: // Dummy
			cartridgeReadData = 0xFFFFFFFF;
			break;
		default:
			cartridgeReadData = 0xFFFFFFFF;
			break;
		}
		break;
	case ENCRYPTION_KEY1:
		switch (currentCommand >> 60) {
		case 0x1: // KEY1 Get ROM Chip ID
			cartridgeReadData = chipId;
			break;
		case 0x2: { // Get Secure Area Block
			if (cartFlags2 & 0x80) {
				if ((bytesRead % 0x218) >= 0x200) {
					cartridgeReadData = 0;
				} else {
					cartridgeReadData = *(u32 *)(romData + ((currentCommand >> 32) & 0xF000) + (bytesRead % 0x218) + ((bytesRead / 0x218) * 0x200));
				}
			} else {
				if ((bytesRead % 0x1018) >= 0x1000) {
					cartridgeReadData = 0;
				} else {
					cartridgeReadData = *(u32 *)(romData + ((currentCommand >> 32) & 0xF000) + (bytesRead % 0x1018));
				}
				shared->log << fmt::format("0x{:X} = 0x{:0>8X}\n", ((currentCommand >> 32) & 0xF000) + (bytesRead % 0x1018), cartridgeReadData);
			}
			} break;
		case 0x4: // Activate KEY2 Encryption Mode
			cartridgeReadData = 0xFFFFFFFF;
			break;
		case 0xA: // Enter Main Data Mode
			encryptionMode = ENCRYPTION_KEY2;
			cartridgeReadData = 0x00000000;
			break;
		default:
			cartridgeReadData = 0x00000000;
			break;
		}
		break;
	case ENCRYPTION_KEY2:
		switch (currentCommand >> 56) {
		case 0xB7: { // Get Data
			u32 address = (currentCommand >> 24) & 0xFFFFFFFF;
			address = (address & 0xFFFFF000) | ((address + bytesRead) & 0xFFF) & (romSizeMax - 1);

			if (address <= 0x7FFF) [[unlikely]] // Secure area can't be read
				address = 0x8000 + (address & 0x1FF);

			if (address >= romSize) { [[unlikely]]
				cartridgeReadData = 0xFFFFFFFF;
			} else {
				cartridgeReadData = *(u32 *)(romData + address);
			}
			} break;
		case 0xB8: // KEY2 Get ROM Chip ID
			cartridgeReadData = chipId;
			break;
		default:
			cartridgeReadData = 0xFFFFFFFF;
			break;
		}
		break;
	}

	//shared->log << fmt::format("[Gamecard] Bytes read: 0x{:0>8X} - Data: 0x{:0>8X}\n", bytesRead, cartridgeReadData);
	if (bytesRead >= dataBlockSizeBytes) {
		blockStart = false;
		//shared->log << fmt::format("[Gamecard] Transfer ready\n");
		if (transferReadyIrq)
			shared->addEvent(0, GAMECARD_COMMAND_COMPLETE);
	} else {
		shared->addEvent(0, GAMECARD_TRANSFER_READY); // All reads are instant
	}

	bytesRead += 4;
}

u8 Gamecard::readIO9(u32 address, bool final) {
	//if (shared->ndsSlotAccess)
	//	return 0;
	//shared->log << fmt::format("[NDS9 Bus][Gamecard] Read from IO register 0x{:0>7X}\n", address);

	u8 val = 0;
	switch (address) {
	case 0x40001A0:
		return (u8)(AUXSPICNT >> 0);
	case 0x40001A1:
		return (u8)(AUXSPICNT >> 8);
	case 0x40001A3:
		return 0;
	case 0x40001A4:
		return (u8)(ROMCTRL >> 0);
	case 0x40001A5:
		return (u8)(ROMCTRL >> 8);
	case 0x40001A6:
		return (u8)(ROMCTRL >> 16);
	case 0x40001A7:
		return (u8)(ROMCTRL >> 24);
	case 0x40001A8:
		return (u8)(nextCartridgeCommand >> 0);
	case 0x40001A9:
		return (u8)(nextCartridgeCommand >> 8);
	case 0x40001AA:
		return (u8)(nextCartridgeCommand >> 16);
	case 0x40001AB:
		return (u8)(nextCartridgeCommand >> 24);
	case 0x40001AC:
		return (u8)(nextCartridgeCommand >> 32);
	case 0x40001AD:
		return (u8)(nextCartridgeCommand >> 40);
	case 0x40001AE:
		return (u8)(nextCartridgeCommand >> 48);
	case 0x40001AF:
		return (u8)(nextCartridgeCommand >> 56);
	case 0x4100010:
		val = (u8)(cartridgeReadData >> 0);
		break;
	case 0x4100011:
		val = (u8)(cartridgeReadData >> 8);
		break;
	case 0x4100012:
		val = (u8)(cartridgeReadData >> 16);
		break;
	case 0x4100013:
		val = (u8)(cartridgeReadData >> 24);
		break;
	default:
		shared->log << fmt::format("[NDS9 Bus][Gamecard] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}

	if (final)
		readMoreData();

	return val;
}

void Gamecard::writeIO9(u32 address, u8 value) {
	//shared->log << fmt::format("[NDS9 Bus][Gamecard] Write to IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
	if (shared->ndsSlotAccess) {
		shared->log << fmt::format("[NDS9 Bus][Gamecard] Write to IO register 0x{:0>7X} with value 0x{:0>2X} when access is set to NDS7\n", address, value);
		return;
	}

	switch (address) {
	case 0x40001A0:
		AUXSPICNT = (AUXSPICNT & 0xFF00) | ((value & 0xC3) << 0);
		break;
	case 0x40001A1:
		AUXSPICNT = (AUXSPICNT & 0x00FF) | ((value & 0xE0) << 8);
		break;
	case 0x40001A3:
		break;
	case 0x40001A4:
		ROMCTRL = (ROMCTRL & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40001A5:
		ROMCTRL = (ROMCTRL & 0xFFFF00FF) | ((value & 0xFF) << 8);

		if (key2ApplySeed) {
			//

			key2ApplySeed = false;
		}
		break;
	case 0x40001A6:
		ROMCTRL = (ROMCTRL & 0xFF00FFFF) | (((value & 0x7F) | 0x80) << 16);
		break;
	case 0x40001A7: {
		bool oldStart = blockStart;
		ROMCTRL = (ROMCTRL & 0x00FFFFFF) | (((value & 0xFF) | 0x20) << 24);

		if (blockStart && !oldStart) {
			if (logGamecard)
				shared->log << "[NDS9 Bus]";
			sendCommand();
		}
		} break;
	case 0x40001A8:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFFFFFFFF00) | ((u64)(value & 0xFF) << 0);
		break;
	case 0x40001A9:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFFFFFF00FF) | ((u64)(value & 0xFF) << 8);
		break;
	case 0x40001AA:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFFFF00FFFF) | ((u64)(value & 0xFF) << 16);
		break;
	case 0x40001AB:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFF00FFFFFF) | ((u64)(value & 0xFF) << 24);
		break;
	case 0x40001AC:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFF00FFFFFFFF) | ((u64)(value & 0xFF) << 32);
		break;
	case 0x40001AD:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFF00FFFFFFFFFF) | ((u64)(value & 0xFF) << 40);
		break;
	case 0x40001AE:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFF00FFFFFFFFFFFF) | ((u64)(value & 0xFF) << 48);
		break;
	case 0x40001AF:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0x00FFFFFFFFFFFFFF) | ((u64)(value & 0xFF) << 56);
		break;
	case 0x40001B0:
		key2Seed0Low9 = (key2Seed0Low9 & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40001B1:
		key2Seed0Low9 = (key2Seed0Low9 & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40001B2:
		key2Seed0Low9 = (key2Seed0Low9 & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40001B3:
		key2Seed0Low9 = (key2Seed0Low9 & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	case 0x40001B4:
		key2Seed1Low9 = (key2Seed1Low9 & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40001B5:
		key2Seed1Low9 = (key2Seed1Low9 & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40001B6:
		key2Seed1Low9 = (key2Seed1Low9 & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40001B7:
		key2Seed1Low9 = (key2Seed1Low9 & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	case 0x40001B8:
		key2Seed0High9 = (value & 0x7F);
		break;
	case 0x40001B9:
		break;
	case 0x40001BA:
		key2Seed1High9 = (value & 0x7F);
		break;
	case 0x40001BB:
		break;
	default:
		shared->log << fmt::format("[NDS9 Bus][Gamecard] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}

u8 Gamecard::readIO7(u32 address, bool final) {
	//shared->log << fmt::format("[NDS7 Bus][Gamecard] Read from IO register 0x{:0>7X}\n", address);
	if (!shared->ndsSlotAccess) {
		shared->log << fmt::format("[NDS7 Bus][Gamecard] Read from IO register 0x{:0>7X} when access is set to NDS9\n", address);
		return 0;
	}

	u8 val = 0;
	switch (address) {
	case 0x40001A0:
		return (u8)(AUXSPICNT >> 0);
	case 0x40001A1:
		return (u8)(AUXSPICNT >> 8);
	case 0x40001A3:
		return 0;
	case 0x40001A4:
		return (u8)(ROMCTRL >> 0);
	case 0x40001A5:
		return (u8)(ROMCTRL >> 8);
	case 0x40001A6:
		return (u8)(ROMCTRL >> 16);
	case 0x40001A7:
		return (u8)(ROMCTRL >> 24);
	case 0x40001A8:
		return (u8)(nextCartridgeCommand >> 0);
	case 0x40001A9:
		return (u8)(nextCartridgeCommand >> 8);
	case 0x40001AA:
		return (u8)(nextCartridgeCommand >> 16);
	case 0x40001AB:
		return (u8)(nextCartridgeCommand >> 24);
	case 0x40001AC:
		return (u8)(nextCartridgeCommand >> 32);
	case 0x40001AD:
		return (u8)(nextCartridgeCommand >> 40);
	case 0x40001AE:
		return (u8)(nextCartridgeCommand >> 48);
	case 0x40001AF:
		return (u8)(nextCartridgeCommand >> 56);
	case 0x4100010:
		val = (u8)(cartridgeReadData >> 0);
		break;
	case 0x4100011:
		val = (u8)(cartridgeReadData >> 8);
		break;
	case 0x4100012:
		val = (u8)(cartridgeReadData >> 16);
		break;
	case 0x4100013:
		val = (u8)(cartridgeReadData >> 24);
		break;
	default:
		shared->log << fmt::format("[NDS7 Bus][Gamecard] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}

	if (final)
		readMoreData();

	return val;
}

void Gamecard::writeIO7(u32 address, u8 value) {
	//shared->log << fmt::format("[NDS7 Bus][Gamecard] Write to IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
	if (!shared->ndsSlotAccess) {
		shared->log << fmt::format("[NDS7 Bus][Gamecard] Write to IO register 0x{:0>7X} with value 0x{:0>2X} when access is set to NDS9\n", address, value);
		return;
	}

	switch (address) {
	case 0x40001A0:
		AUXSPICNT = (AUXSPICNT & 0xFF00) | ((value & 0xC3) << 0);
		break;
	case 0x40001A1:
		AUXSPICNT = (AUXSPICNT & 0x00FF) | ((value & 0xE0) << 8);
		break;
	case 0x40001A3:
		break;
	case 0x40001A4:
		ROMCTRL = (ROMCTRL & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40001A5:
		ROMCTRL = (ROMCTRL & 0xFFFF00FF) | ((value & 0xFF) << 8);

		if (key2ApplySeed) {
			//

			key2ApplySeed = false;
		}
		break;
	case 0x40001A6:
		ROMCTRL = (ROMCTRL & 0xFF00FFFF) | (((value & 0x7F) | 0x80) << 16);
		break;
	case 0x40001A7: {
		bool oldStart = blockStart;
		ROMCTRL = (ROMCTRL & 0x00FFFFFF) | (((value & 0xFF) | 0x20) << 24);

		if (blockStart && !oldStart) {
			if (logGamecard)
				shared->log << "[NDS7 Bus]";
			sendCommand();
		}
		} break;
	case 0x40001A8:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFFFFFFFF00) | ((u64)(value & 0xFF) << 0);
		break;
	case 0x40001A9:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFFFFFF00FF) | ((u64)(value & 0xFF) << 8);
		break;
	case 0x40001AA:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFFFF00FFFF) | ((u64)(value & 0xFF) << 16);
		break;
	case 0x40001AB:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFFFF00FFFFFF) | ((u64)(value & 0xFF) << 24);
		break;
	case 0x40001AC:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFFFF00FFFFFFFF) | ((u64)(value & 0xFF) << 32);
		break;
	case 0x40001AD:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFFFF00FFFFFFFFFF) | ((u64)(value & 0xFF) << 40);
		break;
	case 0x40001AE:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0xFF00FFFFFFFFFFFF) | ((u64)(value & 0xFF) << 48);
		break;
	case 0x40001AF:
		nextCartridgeCommand = (nextCartridgeCommand & (u64)0x00FFFFFFFFFFFFFF) | ((u64)(value & 0xFF) << 56);
		break;
	case 0x40001B0:
		key2Seed0Low7 = (key2Seed0Low7 & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40001B1:
		key2Seed0Low7 = (key2Seed0Low7 & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40001B2:
		key2Seed0Low7 = (key2Seed0Low7 & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40001B3:
		key2Seed0Low7 = (key2Seed0Low7 & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	case 0x40001B4:
		key2Seed1Low7 = (key2Seed1Low7 & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40001B5:
		key2Seed1Low7 = (key2Seed1Low7 & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40001B6:
		key2Seed1Low7 = (key2Seed1Low7 & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40001B7:
		key2Seed1Low7 = (key2Seed1Low7 & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	case 0x40001B8:
		key2Seed0High7 = (value & 0x7F);
		break;
	case 0x40001B9:
		break;
	case 0x40001BA:
		key2Seed1High7 = (value & 0x7F);
		break;
	case 0x40001BB:
		break;
	default:
		shared->log << fmt::format("[NDS7 Bus][Gamecard] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}
