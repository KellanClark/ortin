#pragma once

#include "types.hpp"
#include "emulator/busshared.hpp"

class WiFi {
public:
	std::shared_ptr<BusShared> shared;

    // External Use
	WiFi(std::shared_ptr<BusShared> shared);
	~WiFi();
	void reset();

    // Internal Use
	bool logWifi;

    // Memory Interface
    u16 readIO7(u32 address);
	void writeIO7(u32 address, u16 value);

    // I/O
    u8 *wifiRam; // NDS7 - 0x4804000 to 0x4805FFF

private:
};
