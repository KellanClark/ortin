
#ifndef ORTIN_ORTIN_HPP
#define ORTIN_ORTIN_HPP

#include <stdio.h>
#include <thread>

#include <imgui.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_opengl3.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include "emulator/nds.hpp"

class Submenu {
public:
	Submenu() = default;
	~Submenu() = default;
	virtual void drawMenu() = 0;
	virtual void drawWindows() = 0;
};

#include "menus/file.hpp"
#include "menus/debug.hpp"
#include "menus/graphics.hpp"
#include "menus/dev.hpp"

class Ortin {
public:
	NDS nds;
	std::thread emuThread;

	Ortin();
	~Ortin();
	void drawFrame();
	void drawScreens();

	bool error;
	bool penDown;

	SDL_Window* window;
	SDL_GLContext gl_context;
	ImVec4 clear_color;
	GLuint engineATexture;
	GLuint engineBTexture;

	std::vector<Submenu*> submenus;
};

extern Ortin ortin;

#endif //ORTIN_ORTIN_HPP
