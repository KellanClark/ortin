
#ifndef ORTIN_MENU_GRAPHICS_HPP
#define ORTIN_MENU_GRAPHICS_HPP

#include "ortin.hpp"

class GraphicsMenu : public Submenu {
public:
	GraphicsMenu();
	~GraphicsMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
	bool showPalette;

	void paletteWindow();
};

#endif //ORTIN_MENU_GRAPHICS_HPP
