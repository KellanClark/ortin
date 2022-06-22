
#include "menus/dev.hpp"

DevMenu::DevMenu() {
	showDemoWindow = false;
}

DevMenu::~DevMenu() {
	//
}

void DevMenu::drawMenu() {
	if (ImGui::BeginMenu("Dev")) {
		ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow);
		ImGui::Text("GUI FPS: %.0f", ImGui::GetIO().Framerate);

		ImGui::EndMenu();
	}
}

void DevMenu::drawWindows() {
	if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);
}
