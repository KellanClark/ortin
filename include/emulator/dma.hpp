#ifndef ORTIN_DMA_HPP
#define ORTIN_DMA_HPP

#include "types.hpp"
//#include "emulator/nds9/busarm9.hpp"
//#include "emulator/nds7/busarm7.hpp"
#include "emulator/busshared.hpp"

// Forward declaration
class BusARM9;
class BusARM7;

template <bool dma9>
class DMA {
public:
	using ArchBus = std::conditional_t<dma9, BusARM9, BusARM7>;
	ArchBus &bus;
	BusShared &shared;
	std::stringstream &log;

	// External Use
	DMA(BusShared &shared, std::stringstream &log, ArchBus &bus);
	~DMA();
	void reset();

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
	};

	DmaChannel channel[4]; // 0x40000B0 - 0x40000DF

	u32 DMA0FILL; // NDS9 - 40000E0
	u32 DMA1FILL; // NDS9 - 40000E4
	u32 DMA2FILL; // NDS9 - 40000E8
	u32 DMA3FILL; // NDS9 - 40000EC
};
template class DMA<true>;
template class DMA<false>;

#endif //ORTIN_DMA_HPP