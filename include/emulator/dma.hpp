#ifndef ORTIN_DMA_HPP
#define ORTIN_DMA_HPP

#include "types.hpp"
#include "emulator/busshared.hpp"

// Forward declarations
class BusARM9;
class BusARM7;

// These match up with 9 and will need an if chain for 7
enum DmaStart {
	DMA_IMMEDIATE = 0,
	DMA_VBLANK = 1,
	DMA_HBLANK = 2,
	DMA_DISPLAY_START = 3,
	DMA_MAIN_MEMORY_DISPLAY = 4,
	DMA_DS_SLOT = 5,
	DMA_GBA_SLOT = 6,
	DMA_GEOMETRY_FIFO = 7,
	DMA_WIRELESS
};

template <bool dma9>
class DMA {
public:
	std::shared_ptr<BusShared> shared;
	using ArchBus = std::conditional_t<dma9, BusARM9, BusARM7>;
	ArchBus &bus;

	// External Use
	bool logDma;

	DMA(std::shared_ptr<BusShared> shared, ArchBus &bus);
	~DMA();
	void reset();
	void checkDma(DmaStart event);

	void reloadInternalRegisters(int channel);
	void doDma(int channel);

	// Memory Interface
	u8 readIO9(u32 address);
	void writeIO9(u32 address, u8 value);
	u8 readIO7(u32 address);
	void writeIO7(u32 address, u8 value);

	// I/O Registers
	struct DmaChannel {
		u32 DMASAD;
		u32 DMADAD;
		union {
			struct {
				u32 length : 21;
				u32 destinationControl : 2;
				u32 sourceControl : 2;
				u32 repeat : 1;
				u32 transferType : 1;
				u32 startTiming : 3;
				u32 irqEnable : 1;
				u32 enable : 1;
			};
			u32 DMACNT;
		};

		// Internal registers
		u32 sourceAddress;
		u32 destinationAddress;
		u32 realLength; // What else am I supposed to name this? length is already taken.
	};

	DmaChannel channel[4]; // 0x40000B0 - 0x40000DF

	u32 DMA0FILL; // NDS9 - 40000E0
	u32 DMA1FILL; // NDS9 - 40000E4
	u32 DMA2FILL; // NDS9 - 40000E8
	u32 DMA3FILL; // NDS9 - 40000EC
};

#include "emulator/nds9/busarm9.hpp"
#include "emulator/nds7/busarm7.hpp"

#endif //ORTIN_DMA_HPP