
#ifndef ORTIN_MENU_DEBUG_HPP
#define ORTIN_MENU_DEBUG_HPP

#include "ortin.hpp"
#include "arm946e/arm946edisasm.hpp"
#include "arm7tdmi/arm7tdmidisasm.hpp"

class DebugMenu : public Submenu {
public:
	DebugMenu();
	~DebugMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
	bool showLogs;
	bool showArm9Debug;
	bool showArm7Debug;
	bool showMemEditor9;
	bool showMemEditor7;

	void logsWindow();
	void arm9DebugWindow();
	void arm7DebugWindow();
	void memEditor9Window();
	void memEditor7Window();

	ARM946EDisassembler arm9disasm;
	ARM7TDMIDisassembler arm7disasm;
};

#endif //ORTIN_MENU_DEBUG_HPP
