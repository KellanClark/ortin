#include "menus/graphics.hpp"

GraphicsMenu::GraphicsMenu() {
	showPalette = false;
}

GraphicsMenu::~GraphicsMenu() {
	//
}

void GraphicsMenu::drawMenu() {
	if (ImGui::BeginMenu("Graphics")) {
		ImGui::MenuItem("Palette", nullptr, &showPalette);

		ImGui::EndMenu();
	}
}

void GraphicsMenu::drawWindows() {
	if (showPalette) paletteWindow();
}

#define color555to8888(x) \
	(((int)(((((x) & 0x7C00) >> 10) / (float)31) * 255) << 8) | \
	((int)(((((x) & 0x03E0) >> 5) / (float)31) * 255) << 16) | \
	((int)((((x) & 0x001F) / (float)31) * 255) << 24) | 0xFF)

void GraphicsMenu::paletteWindow() {
	static u16 *selectedRegion;
	static u32 memoryOffset;
	static int selectedIndex;

	ImGui::Begin("Palettes", &showPalette);

	{
		const char *items[] = {"Engine A Standard BG",
							   "Engine A Standard OBJ",
							   "Engine B Standard BG",
							   "Engine B Standard OBJ"};
		static int item_current = 0;
		ImGui::Combo("##palette region", &item_current, items, IM_ARRAYSIZE(items));

		switch (item_current) {
		case 0: selectedRegion = (u16 *)ortin.nds.ppu->engineABgPalette; memoryOffset = 0x000; break;
		case 1: selectedRegion = (u16 *)ortin.nds.ppu->engineAObjPalette; memoryOffset = 0x200; break;
		case 2: selectedRegion = (u16 *)ortin.nds.ppu->engineBBgPalette; memoryOffset = 0x400; break;
		case 3: selectedRegion = (u16 *)ortin.nds.ppu->engineBObjPalette; memoryOffset = 0x600; break;
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
	for (int y = 0; y < 16; y++) {
		for (int x = 0; x < 16; x++) {
			int index = (y * 16) + x;
			std::string id = "Color " + std::to_string(index);
			u32 color = color555to8888(selectedRegion[index]);
			ImVec4 colorVec = ImVec4((color >> 24) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, (color & 0xFF) / 255.0f);

			if (ImGui::ColorButton(id.c_str(), colorVec, (selectedIndex == index) ? 0 : ImGuiColorEditFlags_NoBorder, ImVec2(10, 10)))
				selectedIndex = index;

			if (x != 15)
				ImGui::SameLine();
		}
	}
	ImGui::PopStyleVar();

	u16 color = selectedRegion[selectedIndex];
	ImGui::Spacing();
	ImGui::Text("Color Index:  %d", selectedIndex);
	ImGui::Text("Memory Location:  0x%07X", 0x5000000 + memoryOffset + (selectedIndex * 2));
	ImGui::Text("Color Data:  0x%04X", color);
	ImGui::Text("(r, g, b):  (%d, %d, %d)", color & 0x1F, (color >> 5) & 0x1F, (color >> 10) & 0x1F);

	ImGui::End();
}
