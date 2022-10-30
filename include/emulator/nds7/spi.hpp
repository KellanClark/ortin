#ifndef ORTIN_SPI_HPP
#define ORTIN_SPI_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"

class SPI {
public:
	std::shared_ptr<BusShared> shared;

	// External Use
	SPI(std::shared_ptr<BusShared> shared);
	~SPI();
	void reset();

	bool logSpi;

	void chipSelectLow();

	// Memory Interface
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	// I/O Registers
	int writeNumber;
	union {
		struct {
			u16 baudrate : 2;
			u16 : 5;
			u16 busy : 1;
			u16 deviceSelect : 2;
			u16 transferSize : 1;
			u16 chipselectHold : 1;
			u16 : 2;
			u16 interruptRequest : 1;
			u16 spiBusEnable : 1;
		};
		u16 SPICNT; // NDS7 - 0x40001C0
	};
	u16 SPIDATA; // NDS7 - 0x40001C2

	// SPI Devices (too simple to make their own classes)
	struct {
		union {
			struct {
				u8 powerDownMode : 2;
				u8 referenceSelect : 1;
				u8 conversionMode : 1;
				u8 channelSelect : 3;
				u8 start;
			};
			u8 control;
		};

		u16 xPosition;
		u16 yPosition;
	} touchscreen;

	struct {
		union {
			struct {
				u8 writeBusy : 1;
				u8 writeEnableLatch : 1;
				u8 : 6;
			};
			u8 status;
		};

		u8 currentCommand;
		u32 address;
		bool deepPowerDown;
		bool powerDownPending;
		bool powerUpPending;

		bool logFirmware;
		u8 writeProtect;
		u8 *data;
	} firmware;
};

#endif //ORTIN_SPI_HPP