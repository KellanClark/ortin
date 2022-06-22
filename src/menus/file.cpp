
#include "menus/file.hpp"
#include "nds.hpp"

FileMenu::FileMenu() {
	showRomInfo = false;
}

FileMenu::~FileMenu() {
	//
}

void FileMenu::drawMenu() {
	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Load ROM")) { romFileDialog(); }
		ImGui::Separator();

		ImGui::MenuItem("ROM Info", nullptr, &showRomInfo);

		ImGui::EndMenu();
	}
}

void FileMenu::drawWindows() {
	if (showRomInfo) romInfoWindow();
}

void FileMenu::romFileDialog() {
	NFD::UniquePath outPath;
	nfdfilteritem_t filterItem[1] = {{"Source code", "nds,bin"}};

	nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1);
	if (result == NFD_OKAY) {
		romFilePath = outPath.get();
		std::cout << "Success!" << std::endl << outPath.get() << std::endl;

		ortin.nds.addThreadEvent(NDS::STOP);
		ortin.nds.addThreadEvent(NDS::LOAD_ROM, &romFilePath);
		ortin.nds.addThreadEvent(NDS::RESET);
		ortin.nds.addThreadEvent(NDS::START);
	} else if (result != NFD_CANCEL) {
		std::cout << "Error: " << NFD::GetError() << std::endl;
	}
}

void FileMenu::romInfoWindow() {
	auto romInfo = ortin.nds.romInfo;

	ImGui::Begin("ROM Info", &showRomInfo);

	ImGui::Text("Path:  %s", romInfo.filePath.string().c_str());
	ImGui::Separator();

	ImGui::Text("Name:  %s", romInfo.name.c_str());
	ImGui::Text("Version:  0x%02X(%d)", romInfo.version, romInfo.version);
	bool autostart = romInfo.autostart;
	ImGui::Checkbox("Autostart", &autostart);
	ImGui::Separator();

	ImGui::Text("ARM9 ROM Offset:  0x%08X", romInfo.arm9RomOffset);
	ImGui::Text("ARM9 Entry Point:  0x%08X", romInfo.arm9EntryPoint);
	ImGui::Text("ARM9 Copy Destination:  0x%08X", romInfo.arm9CopyDestination);
	ImGui::Text("ARM9 Copy Size:  0x%08X", romInfo.arm9CopySize);
	ImGui::Spacing();

	ImGui::Text("ARM7 ROM Offset:  0x%08X", romInfo.arm7RomOffset);
	ImGui::Text("ARM7 Entry Point:  0x%08X", romInfo.arm7EntryPoint);
	ImGui::Text("ARM7 Copy Destination:  0x%08X", romInfo.arm7CopyDestination);
	ImGui::Text("ARM7 Copy Size:  0x%08X", romInfo.arm7CopySize);

	ImGui::End();
}