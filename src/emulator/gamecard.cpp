#include "emulator/gamecard.hpp"

Gamecard::Gamecard(BusShared &shared, std::stringstream &log) : shared(shared), log(log) {
	//
}

Gamecard::~Gamecard() {
	//
}

void Gamecard::reset() {
	AUXSPICNT = 0;
	ROMCTRL = 0x00800000;

	currentCommand = 0;
	bytesRead = 0;
	dataBlockSizeBytes = 0;
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

	readMoreData();
}

void Gamecard::readMoreData() {
	/*switch (currentCommand >> 56) {
	case
	}*/

	if ((bytesRead == dataBlockSizeBytes) && transferReadyIrq) {
		blockStart = false;
		shared.addEvent(0, GAMECARD_TRANSFER_READY);
	}

	bytesRead += 4;
}

u8 Gamecard::readIO9(u32 address, bool final) {
	//if (shared.ndsSlotAccess)
	//	return 0;

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
	case 0x4100010 ... 0x4100013:
		return 0xFF;
	default:
		log << fmt::format("[NDS9 Bus][Gamecard] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void Gamecard::writeIO9(u32 address, u8 value) {
	if (shared.ndsSlotAccess) {
		log << fmt::format("[NDS9 Bus][Gamecard] Write to IO register 0x{:0>7X} with value 0x{:0>2X} when access is set to NDS7\n", address, value);
		return;
	}

	switch (address) {
	case 0x40001A0:
		AUXSPICNT = (AUXSPICNT & 0xFF00) | ((value & 0xC3) << 0);
		break;
	case 0x40001A1:
		AUXSPICNT = (AUXSPICNT & 0x00FF) | ((value & 0xE0) << 8);
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
	case 0x40001A7:
		ROMCTRL = (ROMCTRL & 0x00FFFFFF) | (((value & 0xFF) | 0x20) << 24);

		if (blockStart) {
			sendCommand();
		}
		break;
	default:
		log << fmt::format("[NDS9 Bus][Gamecard] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}

u8 Gamecard::readIO7(u32 address, bool final) {
	if (!shared.ndsSlotAccess) {
		log << fmt::format("[NDS7 Bus][Gamecard] Read from IO register 0x{:0>7X} when access is set to NDS9\n", address);
		return 0;
	}
	//std::cout << fmt::format("[NDS7 Bus][Gamecard] Read from unknown IO register 0x{:0>7X}\n", address);

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
	case 0x4100010 ... 0x4100013:
		if (final)
			readMoreData();
		return 0xFF;
	default:
		log << fmt::format("[NDS7 Bus][Gamecard] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void Gamecard::writeIO7(u32 address, u8 value) {
	if (!shared.ndsSlotAccess) {
		log << fmt::format("[NDS7 Bus][Gamecard] Write to IO register 0x{:0>7X} with value 0x{:0>2X} when access is set to NDS9\n", address, value);
		return;
	}
	//std::cout << fmt::format("[NDS7 Bus][Gamecard] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);

	switch (address) {
	case 0x40001A0:
		AUXSPICNT = (AUXSPICNT & 0xFF00) | ((value & 0xC3) << 0);
		break;
	case 0x40001A1:
		AUXSPICNT = (AUXSPICNT & 0x00FF) | ((value & 0xE0) << 8);
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
	case 0x40001A7:
		ROMCTRL = (ROMCTRL & 0x00FFFFFF) | (((value & 0xFF) | 0x20) << 24);

		if (blockStart) {
			sendCommand();
		}
		break;
	default:
		log << fmt::format("[NDS7 Bus][Gamecard] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}