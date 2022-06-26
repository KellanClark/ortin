
#ifndef ORTIN_MENU_FILE_HPP
#define ORTIN_MENU_FILE_HPP

#include "ortin.hpp"

class FileMenu : public Submenu {
public:
	FileMenu();
	~FileMenu();
	void drawMenu() override;
	void drawWindows() override;
};

#endif //ORTIN_MENU_FILE_HPP
