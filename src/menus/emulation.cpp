
#include "menus/emulation.hpp"

EmulationMenu::EmulationMenu() {
	//
}

EmulationMenu::~EmulationMenu() {
	//
}

void EmulationMenu::drawMenu() {
	if (ImGui::BeginMenu("Emulation")) {
		if (ImGui::MenuItem("Sync Time")) { ortin.nds.addThreadEvent(NDS::SET_TIME); }

		ImGui::EndMenu();
	}
}

void EmulationMenu::drawWindows() {
	//
}
