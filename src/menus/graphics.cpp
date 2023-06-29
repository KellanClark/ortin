#include "menus/graphics.hpp"

GraphicsMenu::GraphicsMenu() {
	showPalette = false;
	showBgViewer = false;

	glGenTextures(1, &bgViewerTexture);
	glBindTexture(GL_TEXTURE_2D, bgViewerTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	bgViewerBuffer = new u16[1024 * 1024];
}

GraphicsMenu::~GraphicsMenu() {
	delete[] bgViewerBuffer;
}

void GraphicsMenu::drawMenu() {
	if (ImGui::BeginMenu("Graphics")) {
		ImGui::MenuItem("Palette", nullptr, &showPalette);
		ImGui::MenuItem("Background Viewer", nullptr, &showBgViewer);

		ImGui::EndMenu();
	}
}

void GraphicsMenu::drawWindows() {
	if (showPalette) paletteWindow();
	if (showBgViewer) bgViewerWindow();
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

void drawTileToBuffer(void *buffer, int screenWidth, u32 vramStartAddress, bool useEngineA, int layer, bool eightBitColor, int paletteBank = 0) {
	auto& ppu = *ortin.nds.ppu;
	PPU::Pixel *palette = useEngineA ? ppu.engineABgPalette : ppu.engineBBgPalette;

	if (!useEngineA)
		vramStartAddress += 0x200000;

	for (int y = 0; y < 8; y++) {
		PPU::Pixel *row = (PPU::Pixel *)buffer + (y * screenWidth);
		for (int x = 0; x < 8; x++) {
			if (eightBitColor) {
				u8 tileData = ppu.readVram<u8, true, false>(vramStartAddress + (y * 8) + x);

				row[x] = palette[tileData];;
			} else {
				u8 tileData = ppu.readVram<u8, true, false>(vramStartAddress + (y * 4) + (x / 2));

				if (x & 1) {
					tileData >>= 4;
				} else {
					tileData &= 0xF;
				}

				row[x] = palette[(paletteBank << 4) | tileData];
			}
		}
	}
}

void GraphicsMenu::bgViewerWindow() {
	static int selectedEngineA = true;
	static int selectedBg = 0;

	ImGui::Begin("Backgrounds", &showBgViewer);

	// Create way too many references to pretend I'm in the PPU
	auto& ppu = *ortin.nds.ppu;
	PPU::GraphicsEngine& engine = selectedEngineA ? ppu.engineA : ppu.engineB;
	auto& bg = engine.bg[selectedBg];

	// Engine and layer selection
	if (selectedEngineA == ppu.displaySwap)
		ImGui::Text("Top Screen");
	else
		ImGui::Text("Bottom Screen");
	ImGui::SameLine(ImGui::GetWindowWidth() - 190);
	ImGui::RadioButton("Engine A", &selectedEngineA, true);
	ImGui::SameLine();
	ImGui::RadioButton("Engine B", &selectedEngineA, false);
	
	if (ImGui::BeginTabBar("bgtabbar")) {
		for (int i = 0; i < 4; i++) {
			if (ImGui::BeginTabItem(fmt::format("BG{}", i).c_str())) {
				selectedBg = i;
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	if ((engine.bgMode <= 5) || (selectedEngineA && (engine.bgMode == 6) && (selectedBg == 2))) {
		bool isAffine = false;
		if ((selectedBg == 2) && ((engine.bgMode == 2) || (engine.bgMode == 4) || (engine.bgMode == 5))) isAffine = true;
		if ((selectedBg == 3) && (engine.bgMode >= 1)) isAffine = true;
		bool extended = false;
		int xSize = isAffine ? 0 : (bg.screenSize & 1 ? 512 : 256);
		int ySize = isAffine ? 0 : (bg.screenSize & 2 ? 512 : 256);

		{
			ImGui::BeginChild("bginfopane", ImVec2(200, 400), true);
			u16 tileDataBuffer[8 * 8];

			ImGui::Text("Priority:  %d", bg.priority);
			ImGui::Text("Affine:   ");
			ImGui::SameLine();
			bool affine = isAffine;
			ImGui::SmallCheckbox("##affine", &affine);
			ImGui::Text("Mosaic:   ");
			ImGui::SameLine();
			bool mosaic = bg.mosaic;
			ImGui::SmallCheckbox("##mosaic", &mosaic);
			ImGui::Text("Size:      %d x %d", xSize, ySize);
			ImGui::Text("Tile Base: 0x%07X", 0x6000000 + (selectedEngineA ? 0 : 0x200000) + bg.charBlockBaseAddress);
			ImGui::Text("Map Base:  0x%07X", 0x6000000 + (selectedEngineA ? 0 : 0x200000) + bg.screenBlockBaseAddress);
			if (isAffine) {
				ImGui::Text("Reference Point: (%f, %f)", (float)((i32)(bg.BGX << 4) >> 4) / 256, (float)((i32)(bg.BGY << 4) >> 4) / 256);
				ImGui::Text("Affine Matrix:");
			} else {
				ImGui::Text("X Scroll:  %d", bg.BGHOFS);
				ImGui::Text("Y Scroll:  %d", bg.BGVOFS);
			}

			ImGui::EndChild();
		}

		{
			ImGui::SameLine();
			ImGui::BeginChild("bgdisplaypane", ImVec2(400, 400), true);

			for (int tileY = 0; tileY < ySize / 8; tileY++) {
				for (int tileX = 0; tileX < xSize / 8; tileX++) {
					u32 tileAddress;
					switch (bg.screenSize) {
					case 0: // 256x256
						tileAddress = bg.screenBlockBaseAddress + (tileY * 64) + (tileX * 2);
						break;
					case 1: // 512x256
						tileAddress = bg.screenBlockBaseAddress + ((tileX / 32) * 2048) + (tileY * 64) + (tileX * 2);
						break;
					case 2: // 256x512
						tileAddress = bg.screenBlockBaseAddress + (tileY * 64) + (tileX * 2);
						break;
					case 3: // 512x512
						tileAddress = bg.screenBlockBaseAddress + ((tileX / 32) * 2048) + ((tileY / 32) * 2048 * 2) + (tileY * 64) + (tileX * 2);
						break;
					}

					PPU::TileInfo tile;
					tile.raw = ppu.readVram<u16, true, false>(tileAddress + (selectedEngineA ? 0 : 0x10000));

					drawTileToBuffer(bgViewerBuffer + (tileY * 8 * xSize) + (tileX * 8), xSize * 2, bg.charBlockBaseAddress + (tile.tileIndex * 64), selectedEngineA, selectedBg, false, tile.paletteBank);
				}
			}

			// Update and draw the texture
			glBindTexture(GL_TEXTURE_2D, bgViewerTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, xSize, ySize, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, bgViewerBuffer);
			ImGui::Image((void*)(intptr_t)bgViewerTexture, ImVec2(xSize, ySize));

			ImGui::EndChild();
		}
	} else {
		ImGui::Text("Invalid layer selected.");
	}

	ImGui::End();
}