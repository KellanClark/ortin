#pragma once

#include "ortin.hpp"

class EmulationMenu : public Submenu {
public:
	EmulationMenu();
	~EmulationMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
};
