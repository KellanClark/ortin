#ifndef ORTIN_SPI_HPP
#define ORTIN_SPI_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"

class SPI {
public:
	BusShared &shared;
	std::stringstream &log;

	// External Use
	SPI(BusShared &shared, std::stringstream &log);
	~SPI();
	void reset();

	bool logSpi;

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
};

#endif //ORTIN_SPI_HPP