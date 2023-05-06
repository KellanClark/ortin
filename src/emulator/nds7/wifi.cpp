#include "emulator/nds7/wifi.hpp"

WiFi::WiFi(std::shared_ptr<BusShared> shared) : shared(shared) {
	logWifi = false;

    wifiRam = new u8[0x2000]; // 8KB
}

WiFi::~WiFi() {
	delete[] wifiRam;
}

void WiFi::reset() {
	memset(wifiRam, 0, 0x2000);
}

u16 WiFi::readIO7(u32 address) {
	switch (address) {
    case 0x4808000:
        return 0xC340; // Chip ID (1440h=DS, C340h=DS-Lite)
    case 0x4808044: // W_RANDOM
        return rand() & 0x07FF; // This really uses a LFSR, but it runs very fast and GBATEK doesn't have enough information on it
    case 0x4804000 ... 0x4805FFF:
        return *(u16 *)(wifiRam + (address & 0x1FFF));
	default:
		shared->log << fmt::format("[NDS7 Bus][Wifi] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

void WiFi::writeIO7(u32 address, u16 value) {
	switch (address) {
    case 0x4804000 ... 0x4805FFF:
        *(u16 *)(wifiRam + (address & 0x1FFF)) = value;
        break;
	default:
		shared->log << fmt::format("[NDS7 Bus][Wifi] Write to unknown IO register 0x{:0>7X} with value 0x{:0>4X}\n", address, value);
		break;
	}
}
