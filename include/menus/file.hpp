
#ifndef ORTIN_MENU_FILE_HPP
#define ORTIN_MENU_FILE_HPP

#include "ortin.hpp"
#include "nfd.hpp"

class FileMenu : public Submenu {
public:
	NFD::Guard nfdGuard;
	std::filesystem::path romFilePath;
	std::filesystem::path bios9FilePath;
	std::filesystem::path bios7FilePath;

	FileMenu();
	~FileMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
	bool showRomInfo;

	void romFileDialog();
	void bios9FileDialog();
	void bios7FileDialog();
	void romInfoWindow();
};

#endif //ORTIN_MENU_FILE_HPP
