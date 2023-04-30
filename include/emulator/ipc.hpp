#pragma once

#include "types.hpp"
#include "emulator/busshared.hpp"

class IPC {
public:
	std::shared_ptr<BusShared> shared;

	// External Use
	IPC(std::shared_ptr<BusShared> shared);
	~IPC();
	void reset();

	// Memory Interface
	u8 readIO9(u32 address, bool final);
	void writeIO9(u32 address, u8 value, bool final);
	u8 readIO7(u32 address, bool final);
	void writeIO7(u32 address, u8 value, bool final);

	// I/O Registers
	union {
		struct {
			u16 syncIn9 : 4;
			u16 : 4;
			u16 syncOut9 : 4;
			u16 : 1;
			u16 sendIrq9 : 1;
			u16 enableIrq9 : 1;
			u16 : 1;
		};
		u16 IPCSYNC9; // NDS9 - 0x4000180
	};
	union {
		struct {
			u16 syncIn7 : 4;
			u16 : 4;
			u16 syncOut7 : 4;
			u16 : 1;
			u16 sendIrq7 : 1;
			u16 enableIrq7 : 1;
			u16 : 1;
		};
		u16 IPCSYNC7; // NDS7 - 0x4000180
	};

	union {
		struct {
			u16 sendFifoEmpty9 : 1;
			u16 sendFifoFull9 : 1;
			u16 sendFifoEmptyIrq9 : 1;
			u16 sendFifoClear9 : 1;
			u16 : 4;
			u16 receiveFifoEmpty9 : 1;
			u16 receiveFifoFull9 : 1;
			u16 receiveFifoNotEmptyIrq9 : 1;
			u16 : 3;
			u16 fifoError9 : 1;
			u16 fifoEnable9 : 1;
		};
		u16 IPCFIFOCNT9; // NDS9 - 0x4000184
	};
	union {
		struct {
			u16 sendFifoEmpty7 : 1;
			u16 sendFifoFull7 : 1;
			u16 sendFifoEmptyIrq7 : 1;
			u16 sendFifoClear7 : 1;
			u16 : 4;
			u16 receiveFifoEmpty7 : 1;
			u16 receiveFifoFull7 : 1;
			u16 receiveFifoNotEmptyIrq7 : 1;
			u16 : 3;
			u16 fifoError7 : 1;
			u16 fifoEnable7 : 1;
		};
		u16 IPCFIFOCNT7; // NDS7 - 0x4000184
	};

	u32 IPCFIFOSEND9; // NDS9 - 0x4000188
	u32 IPCFIFOSEND7; // NDS7 - 0x4000188

	u32 IPCFIFORECV9; // NDS9 - 0x4100000
	u32 IPCFIFORECV7; // NDS7 - 0x4100000

	bool sendIrq9Status;
	bool recvIrq9Status;
	bool sendIrq7Status;
	bool recvIrq7Status;
	u32 sendMask9;
	u32 sendMask7;
	std::queue<u32> fifo9to7;
	std::queue<u32> fifo7to9;
};
