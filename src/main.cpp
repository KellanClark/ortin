
#include "ortin.hpp"

Ortin ortin;

SDL_Scancode keymap[12] = {
	SDL_SCANCODE_X, // Button A
	SDL_SCANCODE_Z, // Button B
	SDL_SCANCODE_BACKSPACE, // Select
	SDL_SCANCODE_RETURN, // Start
	SDL_SCANCODE_RIGHT, // Right
	SDL_SCANCODE_LEFT, // Left
	SDL_SCANCODE_UP, // Up
	SDL_SCANCODE_DOWN, // Down
	SDL_SCANCODE_W, // Button R
	SDL_SCANCODE_Q, // Button L
	SDL_SCANCODE_S, // Button X
	SDL_SCANCODE_A // Button Y
};
u16 lastJoypad;

int main(int argc, char *argv[]) {
	if (ortin.error)
		return -1;

	int emuThreadFps = 0;
	u32 lastFpsPoll = 0;
	SDL_Event event;

	volatile bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(ortin.window))
				done = true;
		}
		// Joypad inputs
		const u8 *currentKeyStates = SDL_GetKeyboardState(nullptr);
		u16 currentJoypad = 0;
		for (int i = 0; i < 12; i++) {
			if (currentKeyStates[keymap[i]])
				currentJoypad |= 1 << i;
		}
		if (currentJoypad != lastJoypad) {
			ortin.nds.addThreadEvent(NDS::UPDATE_KEYS, currentJoypad);
			lastJoypad = currentJoypad;
		}

		if ((SDL_GetTicks() - lastFpsPoll) >= 1000) {
			lastFpsPoll = SDL_GetTicks();
			emuThreadFps = ortin.nds.ppu.frameCounter;
			ortin.nds.ppu.frameCounter = 0;
		}
		std::string windowName = "Ortin - " + ortin.nds.romInfo.filePath.string() + " - " + std::to_string(emuThreadFps) + "FPS";
		SDL_SetWindowTitle(ortin.window, windowName.c_str());

		//auto ppu = ortin.nds.ppu;
		if (ortin.nds.ppu.updateScreen) {
			glBindTexture(GL_TEXTURE_2D, ortin.engineATexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, ortin.nds.ppu.framebufferA);
			glBindTexture(GL_TEXTURE_2D, ortin.engineBTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, ortin.nds.ppu.framebufferB);
			ortin.nds.ppu.updateScreen = false;
		}

		ortin.drawFrame();
	}

	return 0;
}