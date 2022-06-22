
#ifndef ORTIN_NDS_HPP
#define ORTIN_NDS_HPP

#include "types.hpp"
#include "busshared.hpp"
#include "ppu.hpp"
#include "busarm9.hpp"
#include "busarm7.hpp"

#include <filesystem>
#include <mutex>
#include <system_error>

#include <mio/mmap.hpp>

class NDS {
public:
	BusShared shared;
	PPU ppu;
	BusARM9 nds9;
	BusARM7 nds7;
	std::stringstream log;

	struct {
		std::filesystem::path filePath;

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
		LOAD_BIOS_ARM9,
		LOAD_BIOS_ARM7,
		LOAD_ROM,
		CLEAR_LOG
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
	mio::mmap_source romMap;
	int loadRom(std::filesystem::path romFilePath_);
};

#endif //ORTIN_NDS_HPP
