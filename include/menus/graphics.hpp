#pragma once

#include "ortin.hpp"

class GraphicsMenu : public Submenu {
public:
	GraphicsMenu();
	~GraphicsMenu();
	void drawMenu() override;
	void drawWindows() override;

private:
	bool showPalette;
	bool showBgViewer;

	void paletteWindow();
	void bgViewerWindow();

	GLuint bgViewerTexture;
	u16 *bgViewerBuffer;
};
