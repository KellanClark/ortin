#include "emulator/nds7/spi.hpp"

SPI::SPI(BusShared &shared, std::stringstream &log) : shared(shared), log(log) {
	logSpi = true;

	firmware.data = nullptr;
}

SPI::~SPI() {
	//
}

void SPI::reset() {
	SPICNT = 0;
	SPIDATA = 0;

	writeNumber = 0;
	touchscreen.xPosition = 0;
	touchscreen.yPosition = 0;
}

u8 SPI::readIO7(u32 address) {
	switch (address) {
	case 0x40001C0:
		return (u8)SPICNT;
	case 0x40001C1:
		return (u8)(SPICNT >> 8);
	case 0x40001C2:
		return (u8)SPIDATA;
	case 0x40001C3:
		return 0;
	default:
		log << fmt::format("[NDS7 Bus][SPI] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void SPI::writeIO7(u32 address, u8 value) {
	switch (address) {
	case 0x40001C0:
		SPICNT = (SPICNT & 0xFF80) | ((value & 0x03) << 0);
		break;
	case 0x40001C1: {
		bool wasHeld = chipselectHold;

		SPICNT = (SPICNT & 0x00FF) | ((value & 0xCF) << 8);
		} break;
	case 0x40001C2:
		SPIDATA = value;

		if (spiBusEnable) {
			if (logSpi) {
				log << fmt::format("[NDS7][SPI] Transferring {:0>2X} to ", SPIDATA);
				switch (deviceSelect) {
				case 0: log << "Power Manager"; break;
				case 1: log << "Firmware"; break;
				case 2: log << "Touchscreen"; break;
				case 3: log << "Reserved"; break;
				}
				log << fmt::format(" with control register {:0>4X}\n", SPICNT);
			}

			switch (deviceSelect) {
			case 2: // Touchscreen
				if (SPIDATA & 0x80) { // Start bit accesses control byte
					touchscreen.control = SPIDATA & 0xFF;
					SPIDATA = 0;
				}

				switch (touchscreen.channelSelect) {
				case 1: // Touchscreen Y-Position
					// Thanks to NooDS because the GBATEK section on this is unreadable
					if (writeNumber & 1) {
						SPIDATA = touchscreen.yPosition >> 5;
					} else {
						SPIDATA = touchscreen.yPosition << 3;
					}
					break;
				case 5: // Touchscreen X-Position
					if (writeNumber & 1) {
						SPIDATA = touchscreen.xPosition >> 5;
					} else {
						SPIDATA = touchscreen.xPosition << 3;
					}
					break;
				default:
					SPIDATA = 0;
					break;
				}
				break;
			}

			if (chipselectHold) {
				++writeNumber;
			} else {
				writeNumber = 0;
			}
			if (interruptRequest)
				shared.addEvent(0, SPI_FINISHED);
		}
		break;
	case 0x40001C3:
		break;
	default:
		log << fmt::format("[NDS7 Bus][SPI] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}