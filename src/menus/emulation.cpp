
#include "menus/emulation.hpp"

EmulationMenu::EmulationMenu() {
	//
}

EmulationMenu::~EmulationMenu() {
	//
}

void EmulationMenu::drawMenu() {
	if (ImGui::BeginMenu("Emulation")) {
		if (ortin.nds.running) {
			if (ImGui::MenuItem("Pause"))
				ortin.nds.addThreadEvent(NDS::STOP);
		} else {
			if (ImGui::MenuItem("Unpause"))
				ortin.nds.addThreadEvent(NDS::START);
		}
		if (ImGui::MenuItem("Reset")) {
			ortin.nds.addThreadEvent(NDS::RESET);
			ortin.nds.addThreadEvent(NDS::START);
		}
		if (ImGui::MenuItem("Sync Time")) { ortin.nds.addThreadEvent(NDS::SET_TIME); }

		ImGui::EndMenu();
	}
}

void EmulationMenu::drawWindows() {
	//
}
