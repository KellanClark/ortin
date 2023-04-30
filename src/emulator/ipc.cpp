#include "emulator/ipc.hpp"

IPC::IPC(std::shared_ptr<BusShared> shared) : shared(shared) {
	//
}

IPC::~IPC() {
	//
}

void IPC::reset() {
	IPCSYNC9 = IPCSYNC7 = 0;
	IPCFIFOCNT9 = IPCFIFOCNT7 = 0x0101;
	IPCFIFOSEND9 = IPCFIFOSEND7 = 0;
	IPCFIFORECV9 = IPCFIFORECV7 = 0;

	sendIrq9Status = recvIrq9Status = sendIrq7Status = recvIrq7Status = false;
	sendMask9 = sendMask7 = 0;
	fifo9to7 = {};
	fifo7to9 = {};
}

u8 IPC::readIO9(u32 address, bool final) {
	u8 val = 0;
	switch (address) {
	case 0x4000180:
		return (u8)(IPCSYNC9 >> 0);
	case 0x4000181:
		return (u8)(IPCSYNC9 >> 8);
	case 0x4000182:
	case 0x4000183:
		return 0;
	case 0x4000184:
		return (u8)(IPCFIFOCNT9 >> 0);
	case 0x4000185:
		return (u8)(IPCFIFOCNT9 >> 8);
	case 0x4000186:
	case 0x4000187:
		return 0;
	case 0x4100000:
		val = (u8)(IPCFIFORECV9 >> 0);
		break;
	case 0x4100001:
		val = (u8)(IPCFIFORECV9 >> 8);
		break;
	case 0x4100002:
		val = (u8)(IPCFIFORECV9 >> 16);
		break;
	case 0x4100003:
		val = (u8)(IPCFIFORECV9 >> 24);
		break;
	default:
		shared->log << fmt::format("[NDS9 Bus][IPC] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}

	// Pop value off the recieve FIFO
	if (final && fifoEnable9) {
		sendIrq7Status = sendFifoEmptyIrq7 && sendFifoEmpty7;

		bool wasEmpty = true;
		if (fifo7to9.empty()) {
			fifoError9 = true;
		} else {
			wasEmpty = false;
			fifo7to9.pop();
		}

		if (fifo7to9.empty()) {
			sendFifoEmpty7 = true;
			sendFifoFull7 = false;
			receiveFifoEmpty9 = true;
			receiveFifoFull9 = false;

			//IPCFIFORECV9 = 0;
		} else {
			sendFifoEmpty7 = false;
			sendFifoFull7 = false;
			receiveFifoEmpty9 = false;
			receiveFifoFull9 = false;

			IPCFIFORECV9 = fifo7to9.front();
		}

		if (!sendIrq7Status && sendFifoEmptyIrq7 && sendFifoEmpty7)
			shared->addEvent(0, EventType::IPC_SEND_FIFO7);
		sendIrq7Status = sendFifoEmptyIrq7 && sendFifoEmpty7;
	}

	return val;
}

void IPC::writeIO9(u32 address, u8 value, bool final) {
	switch (address) {
	case 0x4000180:
		return;
	case 0x4000181:
		IPCSYNC9 = (IPCSYNC9 & 0x00FF) | ((value & 0x6F) << 8);

		syncIn7 = syncOut9;
		if (sendIrq9) {
			sendIrq9 = false;

			if (enableIrq7)
				shared->addEvent(0, EventType::IPC_SYNC7);
		}
		return;
	case 0x4000182:
	case 0x4000183:
		return;
	case 0x4000184:
		sendIrq9Status = sendFifoEmptyIrq9 && sendFifoEmpty9;

		IPCFIFOCNT9 = (IPCFIFOCNT9 & 0xFF03) | ((value & 0x0C) << 0);

		if (sendFifoClear9) {
			sendFifoClear9 = false;

			sendFifoEmpty9 = true;
			sendFifoFull9 = false;
			receiveFifoEmpty7 = true;
			receiveFifoFull7 = false;

			IPCFIFORECV7 = 0;
			fifo9to7 = {};
		}

		if (!sendIrq9Status && sendFifoEmptyIrq9 && sendFifoEmpty9)
			shared->addEvent(0, EventType::IPC_SEND_FIFO9);
		sendIrq9Status = sendFifoEmptyIrq9 && sendFifoEmpty9;
		return;
	case 0x4000185:
		recvIrq9Status = receiveFifoNotEmptyIrq9 && !receiveFifoEmpty9;

		IPCFIFOCNT9 &= ~((value & 0x40) << 8); // Error acknowledge
		IPCFIFOCNT9 = (IPCFIFOCNT9 & 0x43FF) | ((value & 0x84) << 8);

		if (!recvIrq9Status && receiveFifoNotEmptyIrq9 && !receiveFifoEmpty9)
			shared->addEvent(0, EventType::IPC_RECV_FIFO9);
		recvIrq9Status = receiveFifoNotEmptyIrq9 && !receiveFifoEmpty9;
		return;
	case 0x4000186:
	case 0x4000187:
		return;
	case 0x4000188:
		IPCFIFOSEND9 |= value << 0;
		sendMask9 |= 0x000000FF;
		break;
	case 0x4000189:
		IPCFIFOSEND9 |= value << 8;
		sendMask9 |= 0x0000FF00;
		break;
	case 0x400018A:
		IPCFIFOSEND9 |= value << 16;
		sendMask9 |= 0x00FF0000;
		break;
	case 0x400018B:
		IPCFIFOSEND9 |= value << 24;
		sendMask9 |= 0xFF000000;
		break;
	default:
		shared->log << fmt::format("[NDS9 Bus][IPC] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		return;
	}

	// Push value into FIFO
	if (final) {
		if (fifoEnable9) {
			recvIrq7Status = receiveFifoNotEmptyIrq7 && !receiveFifoEmpty7;

			// Mirror value if write is 8 or 16 bits
			IPCFIFOSEND9 >>= std::countr_zero(sendMask9);
			sendMask9 >>= std::countr_zero(sendMask9);
			if (std::countl_zero(sendMask9) == 24) {
				IPCFIFOSEND9 |= IPCFIFOSEND9 << 8;
				IPCFIFOSEND9 |= IPCFIFOSEND9 << 16;
			} else if (std::countl_zero(sendMask9) == 16) {
				IPCFIFOSEND9 |= IPCFIFOSEND9 << 16;
			}

			bool wasEmpty = fifo9to7.empty();
			if (fifo9to7.size() == 16) {
				fifoError9 = true;
			} else {
				fifo9to7.push(IPCFIFOSEND9);
				IPCFIFORECV7 = fifo9to7.front();
				bool full = fifo9to7.size() == 16;

				sendFifoEmpty9 = false;
				sendFifoFull9 = full;
				receiveFifoEmpty7 = false;
				receiveFifoFull7 = full;
			}

			if (!recvIrq7Status && receiveFifoNotEmptyIrq7 && !receiveFifoEmpty7)
				shared->addEvent(0, EventType::IPC_RECV_FIFO7);
			recvIrq7Status = receiveFifoNotEmptyIrq7 && !receiveFifoEmpty7;
		}

		IPCFIFOSEND9 = 0;
		sendMask9 = 0;
	}
}

u8 IPC::readIO7(u32 address, bool final) {
	u8 val = 0;
	switch (address) {
	case 0x4000180:
		return (u8)(IPCSYNC7 >> 0);
	case 0x4000181:
		return (u8)(IPCSYNC7 >> 8);
	case 0x4000182:
	case 0x4000183:
		return 0;
	case 0x4000184:
		return (u8)(IPCFIFOCNT7 >> 0);
	case 0x4000185:
		return (u8)(IPCFIFOCNT7 >> 8);
	case 0x4000186:
	case 0x4000187:
		return 0;
	case 0x4100000:
		val = (u8)(IPCFIFORECV7 >> 0);
		break;
	case 0x4100001:
		val = (u8)(IPCFIFORECV7 >> 8);
		break;
	case 0x4100002:
		val = (u8)(IPCFIFORECV7 >> 16);
		break;
	case 0x4100003:
		val = (u8)(IPCFIFORECV7 >> 24);
		break;
	default:
		shared->log << fmt::format("[NDS7 Bus][IPC] Read from unknown IO register 0x{:0>7X}\n", address);
		return 0;
	}

	// Pop value off the recieve FIFO
	if (final && fifoEnable7) {
		sendIrq9Status = sendFifoEmptyIrq9 && sendFifoEmpty9;

		bool wasEmpty = true;
		if (fifo9to7.empty()) {
			fifoError7 = true;
		} else {
			wasEmpty = false;
			fifo9to7.pop();
		}

		if (fifo9to7.empty()) {
			sendFifoEmpty9 = true;
			sendFifoFull9 = false;
			receiveFifoEmpty7 = true;
			receiveFifoFull7 = false;

			//IPCFIFORECV7 = 0;
		} else {
			sendFifoEmpty9 = false;
			sendFifoFull9 = false;
			receiveFifoEmpty7 = false;
			receiveFifoFull7 = false;

			IPCFIFORECV7 = fifo9to7.front();
		}

		if (!sendIrq9Status && sendFifoEmptyIrq9 && sendFifoEmpty9)
			shared->addEvent(0, EventType::IPC_SEND_FIFO9);
		sendIrq9Status = sendFifoEmptyIrq9 && sendFifoEmpty9;
	}

	return val;
}

void IPC::writeIO7(u32 address, u8 value, bool final) {
	switch (address) {
	case 0x4000180:
		return;
	case 0x4000181:
		IPCSYNC7 = (IPCSYNC7 & 0x00FF) | ((value & 0x6F) << 8);

		syncIn9 = syncOut7;
		if (sendIrq7) {
			sendIrq7 = false;

			if (enableIrq9)
				shared->addEvent(0, EventType::IPC_SYNC9);
		}
		return;
	case 0x4000182:
	case 0x4000183:
		return;
	case 0x4000184:
		sendIrq7Status = sendFifoEmptyIrq7 && sendFifoEmpty7;

		IPCFIFOCNT7 = (IPCFIFOCNT7 & 0xFF03) | ((value & 0x0C) << 0);

		if (sendFifoClear7) {
			sendFifoClear7 = false;

			sendFifoEmpty7 = true;
			sendFifoFull7 = false;
			receiveFifoEmpty9 = true;
			receiveFifoFull9 = false;

			IPCFIFORECV9 = 0;
			fifo7to9 = {};
		}

		if (!sendIrq7Status && sendFifoEmptyIrq7 && sendFifoEmpty7)
			shared->addEvent(0, EventType::IPC_SEND_FIFO7);
		sendIrq7Status = sendFifoEmptyIrq7 && sendFifoEmpty7;
		return;
	case 0x4000185:
		recvIrq7Status = receiveFifoNotEmptyIrq7 && !receiveFifoEmpty7;

		IPCFIFOCNT7 &= ~((value & 0x40) << 8); // Error acknowledge
		IPCFIFOCNT7 = (IPCFIFOCNT7 & 0x43FF) | ((value & 0x84) << 8);

		if (!recvIrq7Status && receiveFifoNotEmptyIrq7 && !receiveFifoEmpty7)
			shared->addEvent(0, EventType::IPC_RECV_FIFO7);
		recvIrq7Status = receiveFifoNotEmptyIrq7 && !receiveFifoEmpty7;
		return;
	case 0x4000186:
	case 0x4000187:
		return;
	case 0x4000188:
		IPCFIFOSEND7 |= value << 0;
		sendMask7 |= 0x000000FF;
		break;
	case 0x4000189:
		IPCFIFOSEND7 |= value << 8;
		sendMask7 |= 0x0000FF00;
		break;
	case 0x400018A:
		IPCFIFOSEND7 |= value << 16;
		sendMask7 |= 0x00FF0000;
		break;
	case 0x400018B:
		IPCFIFOSEND7 |= value << 24;
		sendMask7 |= 0xFF000000;
		break;
	default:
		shared->log << fmt::format("[NDS7 Bus][IPC] Write to unknown IO register 0x{:0>7X} with value 0x{:0>2X}\n", address, value);
		return;
	}

	// Push value into FIFO
	if (final) {
		recvIrq9Status = receiveFifoNotEmptyIrq9 && !receiveFifoEmpty9;

		if (fifoEnable7) {
			// Mirror value if write is 8 or 16 bits
			IPCFIFOSEND7 >>= std::countr_zero(sendMask7);
			sendMask7 >>= std::countr_zero(sendMask7);
			if (std::countl_zero(sendMask7) == 24) {
				IPCFIFOSEND7 |= IPCFIFOSEND7 << 8;
				IPCFIFOSEND7 |= IPCFIFOSEND7 << 16;
			} else if (std::countl_zero(sendMask9) == 16) {
				IPCFIFOSEND7 |= IPCFIFOSEND7 << 16;
			}

			bool wasEmpty = fifo7to9.empty();
			if (fifo7to9.size() == 16) {
				fifoError7 = true;
			} else {
				fifo7to9.push(IPCFIFOSEND7);
				IPCFIFORECV9 = fifo7to9.front();
				bool full = fifo7to9.size() == 16;

				sendFifoEmpty7 = false;
				sendFifoFull7 = full;
				receiveFifoEmpty9 = false;
				receiveFifoFull9 = full;
			}
		}

		IPCFIFOSEND7 = 0;
		sendMask7 = 0;

		if (!recvIrq9Status && receiveFifoNotEmptyIrq9 && !receiveFifoEmpty9)
			shared->addEvent(0, EventType::IPC_RECV_FIFO9);
		recvIrq9Status = receiveFifoNotEmptyIrq9 && !receiveFifoEmpty9;
	}
}