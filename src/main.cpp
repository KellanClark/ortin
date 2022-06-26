
#include "ortin.hpp"

Ortin ortin;

int main(int argc, char *argv[]) {
	if (ortin.error)
		return -1;

	bool done = false;
	while (!done) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(ortin.window))
				done = true;
		}

		ortin.drawFrame();
	}

	return 0;
}