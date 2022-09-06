
#ifndef ORTIN_NDS_HPP
#define ORTIN_NDS_HPP

#include "types.hpp"
#include "busshared.hpp"
#include "ipc.hpp"
#include "ppu.hpp"
#include "emulator/nds9/busarm9.hpp"
#include "emulator/nds7/busarm7.hpp"
#include "arm946e/arm946edisasm.hpp"
#include "arm7tdmi/arm7tdmidisasm.hpp"

#include <filesystem>
#include <mutex>
#include <system_error>

#include "mio/mmap.hpp"

class NDS {
public:
	BusShared shared;
	IPC ipc;
	PPU ppu;
	BusARM9 nds9;
	BusARM7 nds7;
	std::stringstream log;
	ARM946EDisassembler disassembler9;
	ARM7TDMIDisassembler disassembler7;

	struct {
		bool romLoaded, bios9Loaded, bios7Loaded;
		std::filesystem::path filePath;
		std::filesystem::path bios9FilePath;
		std::filesystem::path bios7FilePath;

		std::string name; // 12 bytes - 0x000
		u8 version; // 1 byte - 0x01E
		bool autostart; // 1 byte - 0x01F

		u32 arm9RomOffset; // 4 bytes - 0x020
		u32 arm9EntryPoint; // 4 bytes - 0x024
		u32 arm9CopyDestination; // 4 bytes - 0x028
		u32 arm9CopySize; // 4 bytes - 0x02C

		u32 arm7RomOffset; // 4 bytes - 0x030
		u32 arm7EntryPoint; // 4 bytes - 0x034
		u32 arm7CopyDestination; // 4 bytes - 0x038
		u32 arm7CopySize; // 4 bytes - 0x03C
	} romInfo;

	NDS();
	~NDS();
	void reset();
	void run();

	// Thread-safe queue
	enum threadEventType {
		STOP,
		START,
		RESET,
		STEP_ARM9,
		STEP_ARM7,
		LOAD_ROM,
		LOAD_BIOS9,
		LOAD_BIOS7,
		CLEAR_LOG,
		UPDATE_KEYS
	};
	struct threadEvent {
		threadEventType type;
		u64 intArg;
		void *ptrArg;
	};
	std::queue<threadEvent> threadQueue;
	std::mutex threadQueueMutex;
	void handleThreadQueue();
	void addThreadEvent(threadEventType type);
	void addThreadEvent(threadEventType type, u64 intArg);
	void addThreadEvent(threadEventType type, void *ptrArg);
	void addThreadEvent(threadEventType type, u64 intArg, void *ptrArg);

	bool running;
	bool stepArm9;
	bool stepArm7;
	mio::mmap_source romMap;
	mio::mmap_source bios9Map;
	mio::mmap_source bios7Map;
	int loadRom(std::filesystem::path romFilePath);
	int loadBios9(std::filesystem::path bios9FilePath);
	int loadBios7(std::filesystem::path bios7FilePath);
};

#endif //ORTIN_NDS_HPP
