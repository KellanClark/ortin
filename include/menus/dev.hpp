
#ifndef ORTIN_MENU_DEV_HPP
#define ORTIN_MENU_DEV_HPP

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

#endif //ORTIN_MENU_DEV_HPP
