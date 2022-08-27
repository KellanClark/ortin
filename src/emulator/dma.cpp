
#include "emulator/dma.hpp"

template <bool dma9>
DMA<dma9>::DMA(BusShared &shared, std::stringstream &log, ArchBus &bus) : shared(shared), log(log), bus(bus) {
	//
}

template <bool dma9>
DMA<dma9>::~DMA() {
	//
}
template <bool dma9>
void DMA<dma9>::reset() {
	//
}

template <bool dma9>
void DMA<dma9>::reloadInternalRegisters(int channelNum) {
	channel[channelNum].sourceAddress = channel[channelNum].DMASAD;
	channel[channelNum].destinationAddress = channel[channelNum].DMADAD;
	channel[channelNum].realLength = channel[channelNum].length;
}

template <bool dma9>
void DMA<dma9>::checkDma(DmaStart event) {
	for (int i = 0; i < 4; i++) {
		if (channel[i].enable) {
			if constexpr (dma9) {
				if (channel[i].startTiming == (int)event) {
					doDma(i);
				}
			} else {
				if (((event == DMA_IMMEDIATE) && (channel[i].startTiming == 0)) ||
					((event == DMA_VBLANK) && (channel[i].startTiming == 1)) ||
					((event == DMA_DS_SLOT) && (channel[i].startTiming == 2)) ||
					((event == DMA_WIRELESS) && (channel[i].startTiming == 3) && !(i & 1)) ||
					((event == DMA_GBA_SLOT) && (channel[i].startTiming == 3) && (i & 1)))
					doDma(i);
			}
		}
	}
}

template <bool dma9>
void DMA<dma9>::doDma(int channelNum) {
	DmaChannel& info = channel[channelNum]; // So I don't have to constantly index the array.

	// Adjust length
	if (info.realLength == 0) {
		if constexpr (dma9) {
			info.realLength = 0x200000;
		} else {
			if (channelNum == 3) {
				info.realLength = 0x10000;
			} else {
				info.realLength = 0x4000;
			}
		}
	}

	// Log info
	if (logDma) {
		log << fmt::format("[NDS{} Bus][DMA] DMA Channel {} from 0x{:0>7X} to 0x{:0>7X} of length 0x{:0>4X} with control = 0x{:0>4X}\n", dma9 ? 9 : 7, channelNum, info.sourceAddress, info.destinationAddress, info.realLength, (info.DMACNT >> 16) & 0xFFE0);
		log << "Request Interrupt: " << (info.irqEnable ? "True" : "False") << "  Timing: ";
		if constexpr (dma9) {
			switch (info.startTiming) {
			case 0: log << "Immediately"; break;
			case 1: log << "V-Blank"; break;
			case 2: log << "H-Blank"; break;
			case 3: log << "Synchronize to start of display"; break;
			case 4: log << "Main memory display"; break;
			case 5: log << "DS Cartridge Slot"; break;
			case 6: log << "GBA Cartridge Slot"; break;
			case 7: log << "Geometry Command FIFO"; break;
			}
		} else {
			switch (info.startTiming) {
			case 0: log << "Immediately"; break;
			case 1: log << "V-Blank"; break;
			case 2: log << "DS Cartridge Slot"; break;
			case 3: log << ((channelNum & 1) ? "GBA Cartridge Slot" : "Wireless interrupt"); break;
			}
		}
		log << "  Chunk Size: " << (16 << info.transferType) << "-bit  Repeat: " << (info.repeat ? "True" : "False") << "\n";
		log << "Source Adjustment: ";
		switch (info.sourceAddress) {
		case 0: log << "Increment"; break;
		case 1: log << "Decrement"; break;
		case 2: log << "Fixed"; break;
		case 3: log << "Prohibeted"; break;
		}
		log << "  Destination Adjustment: ";
		switch (info.destinationAddress) {
		case 0: log << "Increment"; break;
		case 1: log << "Decrement"; break;
		case 2: log << "Fixed"; break;
		case 3: log << "Increment/Reload"; break;
		}
	}

	// Get offsets
	// TODO: Force increment and extra sequential for GBA cartridge, force non-sequential for Main RAM
	int sourceOffset;
	switch (info.sourceControl) {
	case 0: sourceOffset = 2; break; // Increment
	case 1: sourceOffset = -2; break; // Decrement
	case 2: sourceOffset = 0; break; // Fixed
	case 3: sourceOffset = 0; break; // Prohibited TODO: Is this right?
	}
	int destinationOffset;
	switch (info.destinationControl) {
	case 0: destinationOffset = 2; break; // Increment
	case 1: destinationOffset = -2; break; // Decrement
	case 2: destinationOffset = 0; break; // Fixed
	case 3: destinationOffset = 2; break; // Increment/Reload
	}
	if (info.transferType) { // 32-bit transfer
		sourceOffset *= 2;
		destinationOffset *= 2;
	}

	// Main transfer
	bool nonsequential = true;
	for (int i = 0; i < info.realLength; i++) {
		if (info.transferType) {
			bus.template write<u32>(info.destinationAddress, bus.template read<u32, false>(info.sourceAddress, !nonsequential), !nonsequential);
		} else {
			bus.template write<u16>(info.destinationAddress, bus.template read<u16, false>(info.sourceAddress, !nonsequential), !nonsequential);
		}

		info.sourceAddress = (info.sourceAddress + sourceOffset) & ((!dma9 && (channelNum == 0)) ? 0x07FFFFFF : 0x0FFFFFFF);
		info.destinationAddress = (info.destinationAddress + destinationOffset) & ((!dma9 && (channelNum != 3)) ? 0x07FFFFFF : 0x0FFFFFFF);
		nonsequential = false;
	}

	// End
	if (info.destinationControl == 3) // Increment/Reload
		info.destinationAddress = info.DMADAD;

	if (info.repeat) {
		info.realLength = info.length;
	} else {
		info.enable = false;
	}

	if (info.irqEnable)
		bus.requestInterrupt(static_cast<typename ArchBus::InterruptType>(ArchBus::INT_DMA_0 + channelNum));
}

template <bool dma9>
u8 DMA<dma9>::readIO9(u32 address) {
	switch (address) {
	case 0x40000B0:
		return (u8)(channel[0].DMASAD >> 0);
	case 0x40000B1:
		return (u8)(channel[0].DMASAD >> 8);
	case 0x40000B2:
		return (u8)(channel[0].DMASAD >> 16);
	case 0x40000B3:
		return (u8)(channel[0].DMASAD >> 24);
	case 0x40000B4:
		return (u8)(channel[0].DMADAD >> 0);
	case 0x40000B5:
		return (u8)(channel[0].DMADAD >> 8);
	case 0x40000B6:
		return (u8)(channel[0].DMADAD >> 16);
	case 0x40000B7:
		return (u8)(channel[0].DMADAD >> 24);
	case 0x40000B8:
		return (u8)(channel[0].DMACNT >> 0);
	case 0x40000B9:
		return (u8)(channel[0].DMACNT >> 8);
	case 0x40000BA:
		return (u8)(channel[0].DMACNT >> 16);
	case 0x40000BB:
		return (u8)(channel[0].DMACNT >> 24);
	case 0x40000BC:
		return (u8)(channel[1].DMASAD >> 0);
	case 0x40000BD:
		return (u8)(channel[1].DMASAD >> 8);
	case 0x40000BE:
		return (u8)(channel[1].DMASAD >> 16);
	case 0x40000BF:
		return (u8)(channel[1].DMASAD >> 24);
	case 0x40000C0:
		return (u8)(channel[1].DMADAD >> 0);
	case 0x40000C1:
		return (u8)(channel[1].DMADAD >> 8);
	case 0x40000C2:
		return (u8)(channel[1].DMADAD >> 16);
	case 0x40000C3:
		return (u8)(channel[1].DMADAD >> 24);
	case 0x40000C4:
		return (u8)(channel[1].DMACNT >> 0);
	case 0x40000C5:
		return (u8)(channel[1].DMACNT >> 8);
	case 0x40000C6:
		return (u8)(channel[1].DMACNT >> 16);
	case 0x40000C7:
		return (u8)(channel[1].DMACNT >> 24);
	case 0x40000C8:
		return (u8)(channel[2].DMASAD >> 0);
	case 0x40000C9:
		return (u8)(channel[2].DMASAD >> 8);
	case 0x40000CA:
		return (u8)(channel[2].DMASAD >> 16);
	case 0x40000CB:
		return (u8)(channel[2].DMASAD >> 24);
	case 0x40000CC:
		return (u8)(channel[2].DMADAD >> 0);
	case 0x40000CD:
		return (u8)(channel[2].DMADAD >> 8);
	case 0x40000CE:
		return (u8)(channel[2].DMADAD >> 16);
	case 0x40000CF:
		return (u8)(channel[2].DMADAD >> 24);
	case 0x40000D0:
		return (u8)(channel[2].DMACNT >> 0);
	case 0x40000D1:
		return (u8)(channel[2].DMACNT >> 8);
	case 0x40000D2:
		return (u8)(channel[2].DMACNT >> 16);
	case 0x40000D3:
		return (u8)(channel[2].DMACNT >> 24);
	case 0x40000D4:
		return (u8)(channel[3].DMASAD >> 0);
	case 0x40000D5:
		return (u8)(channel[3].DMASAD >> 8);
	case 0x40000D6:
		return (u8)(channel[3].DMASAD >> 16);
	case 0x40000D7:
		return (u8)(channel[3].DMASAD >> 24);
	case 0x40000D8:
		return (u8)(channel[3].DMADAD >> 0);
	case 0x40000D9:
		return (u8)(channel[3].DMADAD >> 8);
	case 0x40000DA:
		return (u8)(channel[3].DMADAD >> 16);
	case 0x40000DB:
		return (u8)(channel[3].DMADAD >> 24);
	case 0x40000DC:
		return (u8)(channel[3].DMACNT >> 0);
	case 0x40000DD:
		return (u8)(channel[3].DMACNT >> 8);
	case 0x40000DE:
		return (u8)(channel[3].DMACNT >> 16);
	case 0x40000DF:
		return (u8)(channel[3].DMACNT >> 24);
	case 0x40000E0:
		return (u8)(DMA0FILL >> 0);
	case 0x40000E1:
		return (u8)(DMA0FILL >> 8);
	case 0x40000E2:
		return (u8)(DMA0FILL >> 16);
	case 0x40000E3:
		return (u8)(DMA0FILL >> 24);
	case 0x40000E4:
		return (u8)(DMA1FILL >> 0);
	case 0x40000E5:
		return (u8)(DMA1FILL >> 8);
	case 0x40000E6:
		return (u8)(DMA1FILL >> 16);
	case 0x40000E7:
		return (u8)(DMA1FILL >> 24);
	case 0x40000E8:
		return (u8)(DMA2FILL >> 0);
	case 0x40000E9:
		return (u8)(DMA2FILL >> 8);
	case 0x40000EA:
		return (u8)(DMA2FILL >> 16);
	case 0x40000EB:
		return (u8)(DMA2FILL >> 24);
	case 0x40000EC:
		return (u8)(DMA3FILL >> 0);
	case 0x40000ED:
		return (u8)(DMA3FILL >> 8);
	case 0x40000EE:
		return (u8)(DMA3FILL >> 16);
	case 0x40000EF:
		return (u8)(DMA3FILL >> 24);
	default:
		log << fmt::format("[NDS9 Bus][DMA] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

template <bool dma9>
void DMA<dma9>::writeIO9(u32 address, u8 value) {
	bool oldEnable;

	switch (address) {
	case 0x40000B0:
		channel[0].DMASAD = (channel[0].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000B1:
		channel[0].DMASAD = (channel[0].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000B2:
		channel[0].DMASAD = (channel[0].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000B3:
		channel[0].DMASAD = (channel[0].DMASAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000B4:
		channel[0].DMADAD = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000B5:
		channel[0].DMADAD = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000B6:
		channel[0].DMADAD = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000B7:
		channel[0].DMADAD = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000B8:
		channel[0].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000B9:
		channel[0].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000BA:
		channel[0].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000BB:
		oldEnable = channel[0].enable;
		channel[0].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xFF) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(0);

			if (channel[0].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	case 0x40000BC:
		channel[1].DMASAD = (channel[1].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000BD:
		channel[1].DMASAD = (channel[1].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000BE:
		channel[1].DMASAD = (channel[1].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000BF:
		channel[1].DMASAD = (channel[1].DMASAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000C0:
		channel[1].DMADAD = (channel[1].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000C1:
		channel[1].DMADAD = (channel[1].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000C2:
		channel[1].DMADAD = (channel[1].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000C3:
		channel[1].DMADAD = (channel[1].DMADAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000C4:
		channel[1].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000C5:
		channel[1].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000C6:
		channel[1].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000C7:
		oldEnable = channel[1].enable;
		channel[1].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xFF) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(1);

			if (channel[1].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	case 0x40000C8:
		channel[2].DMASAD = (channel[2].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000C9:
		channel[2].DMASAD = (channel[2].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000CA:
		channel[2].DMASAD = (channel[2].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000CB:
		channel[2].DMASAD = (channel[2].DMASAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000CC:
		channel[2].DMADAD = (channel[2].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000CD:
		channel[2].DMADAD = (channel[2].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000CE:
		channel[2].DMADAD = (channel[2].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000CF:
		channel[2].DMADAD = (channel[2].DMADAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000D0:
		channel[2].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000D1:
		channel[2].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000D2:
		channel[2].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000D3:
		oldEnable = channel[2].enable;
		channel[2].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xFF) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(2);

			if (channel[2].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	case 0x40000D4:
		channel[3].DMASAD = (channel[3].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000D5:
		channel[3].DMASAD = (channel[3].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000D6:
		channel[3].DMASAD = (channel[3].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000D7:
		channel[3].DMASAD = (channel[3].DMASAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000D8:
		channel[3].DMADAD = (channel[3].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000D9:
		channel[3].DMADAD = (channel[3].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000DA:
		channel[3].DMADAD = (channel[3].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000DB:
		channel[3].DMADAD = (channel[3].DMADAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000DC:
		channel[3].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000DD:
		channel[3].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000DE:
		channel[3].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000DF:
		oldEnable = channel[3].enable;
		channel[3].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xFF) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(3);

			if (channel[3].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	case 0x40000E0:
		DMA0FILL = (DMA0FILL & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000E1:
		DMA0FILL = (DMA0FILL & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000E2:
		DMA0FILL = (DMA0FILL & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000E3:
		DMA0FILL = (DMA0FILL & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	case 0x40000E4:
		DMA1FILL = (DMA1FILL & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000E5:
		DMA1FILL = (DMA1FILL & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000E6:
		DMA1FILL = (DMA1FILL & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000E7:
		DMA1FILL = (DMA1FILL & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	case 0x40000E8:
		DMA2FILL = (DMA2FILL & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000E9:
		DMA2FILL = (DMA2FILL & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000EA:
		DMA2FILL = (DMA2FILL & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000EB:
		DMA2FILL = (DMA2FILL & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	case 0x40000EC:
		DMA3FILL = (DMA3FILL & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000ED:
		DMA3FILL = (DMA3FILL & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000EE:
		DMA3FILL = (DMA3FILL & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000EF:
		DMA3FILL = (DMA3FILL & 0x00FFFFFF) | ((value & 0xFF) << 24);
		break;
	default:
		log << fmt::format("[NDS9 Bus][DMA] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}

template <bool dma9>
u8 DMA<dma9>::readIO7(u32 address) {
	switch (address) {
	case 0x40000B0:
		return (u8)(channel[0].DMASAD >> 0);
	case 0x40000B1:
		return (u8)(channel[0].DMASAD >> 8);
	case 0x40000B2:
		return (u8)(channel[0].DMASAD >> 16);
	case 0x40000B3:
		return (u8)(channel[0].DMASAD >> 24);
	case 0x40000B4:
		return (u8)(channel[0].DMADAD >> 0);
	case 0x40000B5:
		return (u8)(channel[0].DMADAD >> 8);
	case 0x40000B6:
		return (u8)(channel[0].DMADAD >> 16);
	case 0x40000B7:
		return (u8)(channel[0].DMADAD >> 24);
	case 0x40000B8:
		return (u8)(channel[0].DMACNT >> 0);
	case 0x40000B9:
		return (u8)(channel[0].DMACNT >> 8);
	case 0x40000BA:
		return (u8)(channel[0].DMACNT >> 16);
	case 0x40000BB:
		return (u8)(channel[0].DMACNT >> 24);
	case 0x40000BC:
		return (u8)(channel[1].DMASAD >> 0);
	case 0x40000BD:
		return (u8)(channel[1].DMASAD >> 8);
	case 0x40000BE:
		return (u8)(channel[1].DMASAD >> 16);
	case 0x40000BF:
		return (u8)(channel[1].DMASAD >> 24);
	case 0x40000C0:
		return (u8)(channel[1].DMADAD >> 0);
	case 0x40000C1:
		return (u8)(channel[1].DMADAD >> 8);
	case 0x40000C2:
		return (u8)(channel[1].DMADAD >> 16);
	case 0x40000C3:
		return (u8)(channel[1].DMADAD >> 24);
	case 0x40000C4:
		return (u8)(channel[1].DMACNT >> 0);
	case 0x40000C5:
		return (u8)(channel[1].DMACNT >> 8);
	case 0x40000C6:
		return (u8)(channel[1].DMACNT >> 16);
	case 0x40000C7:
		return (u8)(channel[1].DMACNT >> 24);
	case 0x40000C8:
		return (u8)(channel[2].DMASAD >> 0);
	case 0x40000C9:
		return (u8)(channel[2].DMASAD >> 8);
	case 0x40000CA:
		return (u8)(channel[2].DMASAD >> 16);
	case 0x40000CB:
		return (u8)(channel[2].DMASAD >> 24);
	case 0x40000CC:
		return (u8)(channel[2].DMADAD >> 0);
	case 0x40000CD:
		return (u8)(channel[2].DMADAD >> 8);
	case 0x40000CE:
		return (u8)(channel[2].DMADAD >> 16);
	case 0x40000CF:
		return (u8)(channel[2].DMADAD >> 24);
	case 0x40000D0:
		return (u8)(channel[2].DMACNT >> 0);
	case 0x40000D1:
		return (u8)(channel[2].DMACNT >> 8);
	case 0x40000D2:
		return (u8)(channel[2].DMACNT >> 16);
	case 0x40000D3:
		return (u8)(channel[2].DMACNT >> 24);
	case 0x40000D4:
		return (u8)(channel[3].DMASAD >> 0);
	case 0x40000D5:
		return (u8)(channel[3].DMASAD >> 8);
	case 0x40000D6:
		return (u8)(channel[3].DMASAD >> 16);
	case 0x40000D7:
		return (u8)(channel[3].DMASAD >> 24);
	case 0x40000D8:
		return (u8)(channel[3].DMADAD >> 0);
	case 0x40000D9:
		return (u8)(channel[3].DMADAD >> 8);
	case 0x40000DA:
		return (u8)(channel[3].DMADAD >> 16);
	case 0x40000DB:
		return (u8)(channel[3].DMADAD >> 24);
	case 0x40000DC:
		return (u8)(channel[3].DMACNT >> 0);
	case 0x40000DD:
		return (u8)(channel[3].DMACNT >> 8);
	case 0x40000DE:
		return (u8)(channel[3].DMACNT >> 16);
	case 0x40000DF:
		return (u8)(channel[3].DMACNT >> 24);
	default:
		log << fmt::format("[NDS7 Bus][DMA] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}
}

template <bool dma9>
void DMA<dma9>::writeIO7(u32 address, u8 value) {
	bool oldEnable;

	switch (address) {
	case 0x40000B0:
		channel[0].DMASAD = (channel[0].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000B1:
		channel[0].DMASAD = (channel[0].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000B2:
		channel[0].DMASAD = (channel[0].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000B3:
		channel[0].DMASAD = (channel[0].DMASAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000B4:
		channel[0].DMADAD = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000B5:
		channel[0].DMADAD = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000B6:
		channel[0].DMADAD = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000B7:
		channel[0].DMADAD = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000B8:
		channel[0].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000B9:
		channel[0].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0x3F) << 8);
		break;
	case 0x40000BA:
		channel[0].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000BB:
		oldEnable = channel[0].enable;
		channel[0].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xF7) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(0);

			if (channel[0].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	case 0x40000BC:
		channel[1].DMASAD = (channel[1].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000BD:
		channel[1].DMASAD = (channel[1].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000BE:
		channel[1].DMASAD = (channel[1].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000BF:
		channel[1].DMASAD = (channel[1].DMASAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000C0:
		channel[1].DMADAD = (channel[1].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000C1:
		channel[1].DMADAD = (channel[1].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000C2:
		channel[1].DMADAD = (channel[1].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000C3:
		channel[1].DMADAD = (channel[1].DMADAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000C4:
		channel[1].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000C5:
		channel[1].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0x3F) << 8);
		break;
	case 0x40000C6:
		channel[1].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000C7:
		oldEnable = channel[1].enable;
		channel[1].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xF7) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(1);

			if (channel[1].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	case 0x40000C8:
		channel[2].DMASAD = (channel[2].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000C9:
		channel[2].DMASAD = (channel[2].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000CA:
		channel[2].DMASAD = (channel[2].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000CB:
		channel[2].DMASAD = (channel[2].DMASAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000CC:
		channel[2].DMADAD = (channel[2].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000CD:
		channel[2].DMADAD = (channel[2].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000CE:
		channel[2].DMADAD = (channel[2].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000CF:
		channel[2].DMADAD = (channel[2].DMADAD & 0x00FFFFFF) | ((value & 0x07) << 24);
		break;
	case 0x40000D0:
		channel[2].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000D1:
		channel[2].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0x3F) << 8);
		break;
	case 0x40000D2:
		channel[2].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000D3:
		oldEnable = channel[2].enable;
		channel[2].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xF7) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(2);

			if (channel[2].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	case 0x40000D4:
		channel[3].DMASAD = (channel[3].DMASAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000D5:
		channel[3].DMASAD = (channel[3].DMASAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000D6:
		channel[3].DMASAD = (channel[3].DMASAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000D7:
		channel[3].DMASAD = (channel[3].DMASAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000D8:
		channel[3].DMADAD = (channel[3].DMADAD & 0xFFFFFF00) | ((value & 0xFE) << 0);
		break;
	case 0x40000D9:
		channel[3].DMADAD = (channel[3].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000DA:
		channel[3].DMADAD = (channel[3].DMADAD & 0xFF00FFFF) | ((value & 0xFF) << 16);
		break;
	case 0x40000DB:
		channel[3].DMADAD = (channel[3].DMADAD & 0x00FFFFFF) | ((value & 0x0F) << 24);
		break;
	case 0x40000DC:
		channel[3].DMACNT = (channel[0].DMADAD & 0xFFFFFF00) | ((value & 0xFF) << 0);
		break;
	case 0x40000DD:
		channel[3].DMACNT = (channel[0].DMADAD & 0xFFFF00FF) | ((value & 0xFF) << 8);
		break;
	case 0x40000DE:
		channel[3].DMACNT = (channel[0].DMADAD & 0xFF00FFFF) | ((value & 0xE0) << 16);
		break;
	case 0x40000DF:
		oldEnable = channel[3].enable;
		channel[3].DMACNT = (channel[0].DMADAD & 0x00FFFFFF) | ((value & 0xF7) << 24);

		if ((value & 0x80) && !oldEnable) {
			reloadInternalRegisters(3);

			if (channel[3].startTiming == 0) // Immediately
				checkDma(DMA_IMMEDIATE);
		}
		break;
	default:
		log << fmt::format("[NDS7 Bus][DMA] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		break;
	}
}

template class DMA<true>;
template class DMA<false>;