#pragma once

#include "ortin.hpp"

class DevMenu : public Submenu {
public:
	DevMenu();
	~DevMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
	bool showDemoWindow;
};
