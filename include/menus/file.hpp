#pragma once

#include "ortin.hpp"
#include "nfd.hpp"

class FileMenu : public Submenu {
public:
	NFD::Guard nfdGuard;
	std::filesystem::path romFilePath;
	std::filesystem::path bios9FilePath;
	std::filesystem::path bios7FilePath;
	std::filesystem::path firmwareFilePath;

	FileMenu();
	~FileMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
	bool showRomInfo;

	void romFileDialog();
	void bios9FileDialog();
	void bios7FileDialog();
	void firmwareFileDialog();
	void romInfoWindow();
};
