#include "emulator/nds7/spi.hpp"

// Some firmware commands don't have an effect if a write is in progress
#define rejectIfWrite() do { \
	if (firmware.writeBusy) { \
		firmware.currentCommand = 0; \
		SPIDATA = 0; \
		break; \
	} \
} while (0)

SPI::SPI(std::shared_ptr<BusShared> shared) : shared(shared) {
	logSpi = false;

	firmware.logFirmware = false;
	firmware.writeProtect = true;
	firmware.data = nullptr;
}

SPI::~SPI() {
	//
}

void SPI::reset() {
	SPICNT = 0;
	SPIDATA = 0;
	writeNumber = 0;

	firmware.status = 0;
	firmware.currentCommand = 0;
	firmware.address = 0;
	firmware.deepPowerDown = false;
	firmware.powerDownPending = firmware.powerUpPending = false;

	touchscreen.xPosition = 0;
	touchscreen.yPosition = 0;
}

void SPI::chipSelectLow() {
	writeNumber = 0;

	if (firmware.powerDownPending) firmware.deepPowerDown = true;
	if (firmware.powerUpPending) firmware.deepPowerDown = false;
	if ((firmware.currentCommand == 0x0A) || (firmware.currentCommand == 0x02) || (firmware.currentCommand == 0xDB) || (firmware.currentCommand == 0xD8)) {
		firmware.writeEnableLatch = false;
		firmware.writeBusy = false;
	}
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
		shared->log << fmt::format("[NDS7 Bus][SPI] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void SPI::writeIO7(u32 address, u8 value) {
	switch (address) {
	case 0x40001C0:
		SPICNT = (SPICNT & 0xFF80) | ((value & 0x03) << 0);
		break;
	case 0x40001C1:
		SPICNT = (SPICNT & 0x00FF) | ((value & 0xCF) << 8);

		if (!spiBusEnable)
			chipSelectLow();
		break;
	case 0x40001C2:
		SPIDATA = value;

		if (spiBusEnable) {
			if (logSpi) {
				shared->log << fmt::format("[NDS7][SPI] Transferring {:0>2X} to ", SPIDATA);
				switch (deviceSelect) {
				case 0: shared->log << "Power Manager"; break;
				case 1: shared->log << "Firmware"; break;
				case 2: shared->log << "Touchscreen"; break;
				case 3: shared->log << "Reserved"; break;
				}
				shared->log << fmt::format(" with control register {:0>4X}\n", SPICNT);
			}

			switch (deviceSelect) {
			case 1: // Firmware
				if (writeNumber == 0) { // Instruction code
					firmware.currentCommand = SPIDATA;
					SPIDATA = 0;
				}

				// Only RDP can be run in deep power dowm
				if (firmware.deepPowerDown && (firmware.currentCommand != 0xAB))
					return;

				switch (firmware.currentCommand) {
				case 0x06: // WREN - Write Enable
					if (writeNumber == 0) {
						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x06: WREN - Write Enable\n";

						firmware.writeEnableLatch = true;
					}
					break;
				case 0x04: // WRDI - Write Disable
					if (writeNumber == 0) {
						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x04: WRDI - Write Disable\n";

						firmware.writeEnableLatch = false;
					}
					break;
				case 0x95: // RDID - Read JEDEC Identification
					switch (writeNumber) {
					case 0:
						rejectIfWrite();

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x95: RDID - Read JEDEC Identification\n";
						break;
					case 1: // Manufacturer Identification
						SPIDATA = 0x20;

						if (firmware.logFirmware) {
							shared->log << "[NDS7][SPI][Firmware] Read Manufacturer Identification: 0x20\n";
						}
						break;
					case 2: // Memory Type
						SPIDATA = 0x40;

						if (firmware.logFirmware) {
							shared->log << "[NDS7][SPI][Firmware] Read Memory Type: 0x40\n";
						}
						break;
					case 3: // Manufacturer Identification
						SPIDATA = 0x12;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Read Memory Capacity: 0x12\n";
						break;
					}
					break;
				case 0x05: // RDSR - Read Status Register
					if (writeNumber == 0) {
						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x05: RDSR - Read Status Register\n";
					} else {
						SPIDATA = firmware.status;
					}
					break;
				case 0x03: // READ - Read Data Bytes
					switch (writeNumber) {
					case 0:
						rejectIfWrite();
						firmware.address = 0;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x03: READ - Read Data Bytes\n";
						break;
					case 1 ... 3: // Address
						firmware.address = (firmware.address << 8) | SPIDATA;
						firmware.address &= 0x3FFFF;

						if (firmware.logFirmware && (writeNumber == 3))
							shared->log << fmt::format("[NDS7][SPI][Firmware] Selected Address: 0x{:0>5X}\n", firmware.address);
						break;
					default: // Continuously read bytes
						SPIDATA = firmware.data[firmware.address++];
						firmware.address &= 0x3FFFF;

						if (firmware.logFirmware)
							shared->log << fmt::format("[NDS7][SPI][Firmware] Read Byte: 0x{:0>2X}\n", SPIDATA);
						break;
					}
					break;
				case 0x0B: // FAST - Read Data Bytes at Higher Speed
					switch (writeNumber) {
					case 0:
						rejectIfWrite();
						firmware.address = 0;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x0B: FAST - Read Data Bytes at Higher Speed\n";
						break;
					case 1 ... 3: // Address
						firmware.address = (firmware.address << 8) | SPIDATA;
						firmware.address &= 0x3FFFF;

						if (firmware.logFirmware && (writeNumber == 3))
							shared->log << fmt::format("[NDS7][SPI][Firmware] Selected Address: 0x{:0>5X}\n", firmware.address);
						break;
					case 4: // Dummy byte
						break;
					default: // Continuously read bytes
						SPIDATA = firmware.data[firmware.address++];
						firmware.address &= 0x3FFFF;

						if (firmware.logFirmware)
							shared->log << fmt::format("[NDS7][SPI][Firmware] Read Byte: 0x{:0>2X}\n", SPIDATA);
						break;
					}
					break;
				case 0x0A: // PW - Page Write
					switch (writeNumber) {
					case 0:
						rejectIfWrite();
						firmware.address = 0;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x0A: PW - Page Write\n";
						break;
					case 1 ... 3: // Address
						firmware.address = (firmware.address << 8) | SPIDATA;
						firmware.address &= 0x3FFFF;

						if (firmware.logFirmware && (writeNumber == 3))
							shared->log << fmt::format("[NDS7][SPI][Firmware] Selected Address: 0x{:0>5X}\n", firmware.address);
						break;
					case 4: // Erase page
						if (!firmware.writeEnableLatch || (firmware.writeProtect && ((firmware.address >> 16) == 0))) {
							firmware.currentCommand = 0;
							SPIDATA = 0;
							break;
						}

						for (int i = 0; i <= 0xFF; i++) {
							firmware.data[(firmware.address & 0x3FF00) | i] = 0xFF;
						}
					default: // Continuously write bytes
						firmware.data[firmware.address] = SPIDATA;
						firmware.address = (firmware.address & 0x3FF00) | ((firmware.address + 1) & 0xFF);

						if (firmware.logFirmware)
							shared->log << fmt::format("[NDS7][SPI][Firmware] Wrote Byte: 0x{:0>2X}\n", SPIDATA);
						break;
					}
					break;
				case 0x02: // PP - Page Program
					switch (writeNumber) {
					case 0:
						rejectIfWrite();
						firmware.address = 0;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0x02: PP - Page Program\n";
						break;
					case 1 ... 3: // Address
						firmware.address = (firmware.address << 8) | SPIDATA;
						firmware.address &= 0x3FFFF;

						if (firmware.logFirmware && (writeNumber == 3))
							shared->log << fmt::format("[NDS7][SPI][Firmware] Selected Address: 0x{:0>5X}\n", firmware.address);
						break;
					case 4:
						if (!firmware.writeEnableLatch || (firmware.writeProtect && ((firmware.address >> 16) == 0))) {
							firmware.currentCommand = 0;
							SPIDATA = 0;
							break;
						}
					default: // Continuously write bytes
						firmware.data[firmware.address] &= SPIDATA;
						firmware.address = (firmware.address & 0x3FF00) | ((firmware.address + 1) & 0xFF);

						if (firmware.logFirmware)
							shared->log << fmt::format("[NDS7][SPI][Firmware] Programmed Byte: 0x{:0>2X}\n", SPIDATA);
						break;
					}
					break;
				case 0xDB: // PE - Page Erase 100h bytes
					switch (writeNumber) {
					case 0:
						rejectIfWrite();
						firmware.address = 0;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0xDB: PE - Page Erase 100h bytes\n";
						break;
					case 1 ... 3: // Address
						firmware.address = (firmware.address << 8) | SPIDATA;
						firmware.address &= 0x3FFFF;

						if (writeNumber == 3) {
							if (!firmware.writeEnableLatch || (firmware.writeProtect && ((firmware.address >> 16) == 0))) {
								firmware.currentCommand = 0;
								SPIDATA = 0;
								break;
							}

							for (int i = 0; i <= 0xFF; i++) {
								firmware.data[(firmware.address & 0x3FF00) | i] = 0xFF;
							}

							if (firmware.logFirmware)
								shared->log << fmt::format("[NDS7][SPI][Firmware] Selected Page: 0x{:0>3X}\n", firmware.address >> 8);
						}
						break;
					}
					break;
				case 0xD8: // SE - Sector Erase 10000h bytes
					switch (writeNumber) {
					case 0:
						rejectIfWrite();
						firmware.address = 0;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0xD8: SE - Sector Erase 10000h bytes\n";
						break;
					case 1 ... 3: // Address
						firmware.address = (firmware.address << 8) | SPIDATA;
						firmware.address &= 0x3FFFF;

						if (writeNumber == 3) {
							if (!firmware.writeEnableLatch || (firmware.writeProtect && ((firmware.address >> 16) == 0))) {
								firmware.currentCommand = 0;
								SPIDATA = 0;
								break;
							}

							for (int i = 0; i <= 0xFFFF; i++) {
								firmware.data[(firmware.address & 0x30000) | i] = 0xFF;
							}

							if (firmware.logFirmware)
								shared->log << fmt::format("[NDS7][SPI][Firmware] Selected Sector: {}\n", firmware.address >> 16);
						}
						break;
					}
					break;
				case 0xB9: // DP - Deep Power-down
					if (writeNumber == 0) {
						rejectIfWrite();

						// >>> Chip  Select  (!S)  must  be  driven  High  after  the
						// eighth bit of the instruction code has been latched
						// in, otherwise the Deep Power-down (DP) instruc-
						// tion is not executed.
						firmware.powerUpPending = true;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0xB9: DP - Deep Power-down\n";
					} else {
						firmware.powerUpPending = false;
					}
					break;
				case 0xAB: // RDP - Release from Deep Power-down
					if (writeNumber == 0) {
						// >>> The  Release  from  Deep  Power-down  (RDP)  in-
						// struction is terminated by driving Chip Select (!S)
						// High.  Sending  additional  clock  cycles  on  Serial
						// Clock  (C),  while  Chip  Select  (S)  is  driven  Low,
						// cause the instruction to be rejected, and not exe-
						// cuted.
						firmware.powerDownPending = true;

						if (firmware.logFirmware)
							shared->log << "[NDS7][SPI][Firmware] Command 0xAB: RDP - Release from Deep Power-down\n";
					} else {
						firmware.powerDownPending = false;
					}
					break;
				default:
					SPIDATA = 0;
					break;
				}
				break;
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
				chipSelectLow();
			}
			if (interruptRequest)
				shared->addEvent(0, SPI_FINISHED);
		}
		break;
	case 0x40001C3:
		break;
	default:
		shared->log << fmt::format("[NDS7 Bus][SPI] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}