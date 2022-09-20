
#ifndef ORTIN_MENU_EMULATION_HPP
#define ORTIN_MENU_EMULATION_HPP

#include "ortin.hpp"

class EmulationMenu : public Submenu {
public:
	EmulationMenu();
	~EmulationMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
};

#endif //ORTIN_EMULATION_FILE_HPP
