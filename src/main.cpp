
#include "ortin.hpp"
#include "emulator/nds7/apu.hpp"
#include "wavfile/wavfile.hpp"

#include "ini.h"

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
u32 lastJoypad;

WavFile<i16> wavFile;
SDL_AudioSpec desiredAudioSpec, audioSpec;
SDL_AudioDeviceID audioDevice;
void audioCallback(void *userdata, uint8_t *stream, int len) {
	if (!ortin.nds.running)
		return;

	ortin.nds.nds7->apu->soundRunning = true; // What's thread safety?

	SDL_memcpy(stream, ortin.nds.nds7->apu->outputSamples, len); // Copy samples to SDL's buffer
	wavFile.write((u8 *)ortin.nds.nds7->apu->outputSamples, len); // Write samples to file
}

int main(int argc, char *argv[]) {
	if (ortin.error)
		return -1;

	// Load settings
	mINI::INIFile file("settings.ini");
	mINI::INIStructure ini;
	file.read(ini);
	if (ini["files"]["autoloadbios9"] == "true") {
		static std::filesystem::path path = ini["files"]["bios9path"];
		ortin.nds.addThreadEvent(NDS::LOAD_BIOS9, &path);
	}
	if (ini["files"]["autoloadbios7"] == "true") {
		static std::filesystem::path path = ini["files"]["bios7path"];
		ortin.nds.addThreadEvent(NDS::LOAD_BIOS7, &path);
	}
	if (ini["files"]["autoloadfirmware"] == "true") {
		static std::filesystem::path path = ini["files"]["firmwarepath"];
		ortin.nds.addThreadEvent(NDS::LOAD_FIRMWARE, &path);
	}

	// Setup Audio
	desiredAudioSpec = {
		.freq = 32768,
		.format = AUDIO_U16,
		.channels = 2,
		.samples = SAMPLE_BUFFER_SIZE,
		.callback = audioCallback,
		.userdata = nullptr
	};
	audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredAudioSpec, &audioSpec, 0);
	SDL_PauseAudioDevice(audioDevice, 0);
	wavFile.open("output.wav", audioSpec.freq, audioSpec.channels);

	int emuThreadFps = 0;
	u32 lastFpsPoll = 0;
	SDL_Event event;

	volatile bool done = false;
	while (!done) {
		// Input
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(ortin.window))
				done = true;
		}
		// Joypad inputs
		const u8 *currentKeyStates = SDL_GetKeyboardState(nullptr);
		u32 currentJoypad = 0;
		for (int i = 0; i < 12; i++) {
			if (currentKeyStates[keymap[i]])
				currentJoypad |= 1 << i;
		}
		currentJoypad |= ortin.penDown << 16;
		if (currentJoypad != lastJoypad) {
			ortin.nds.addThreadEvent(NDS::UPDATE_KEYS, currentJoypad);
			lastJoypad = currentJoypad;
		}

		// Count FPS
		if ((SDL_GetTicks() - lastFpsPoll) >= 1000) {
			lastFpsPoll = SDL_GetTicks();
			emuThreadFps = ortin.nds.ppu->frameCounter;
			ortin.nds.ppu->frameCounter = 0;
		}

		// Set window name
		std::string windowName = "Ortin - " + ortin.nds.romInfo.filePath.string() + " - " + std::to_string(emuThreadFps) + "FPS";
		SDL_SetWindowTitle(ortin.window, windowName.c_str());

		// Update display texture
		//auto ppu = ortin.nds.ppu;
		if (ortin.nds.ppu->updateScreen) {
			glBindTexture(GL_TEXTURE_2D, ortin.engineATexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, ortin.nds.ppu->framebufferA);
			glBindTexture(GL_TEXTURE_2D, ortin.engineBTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, ortin.nds.ppu->framebufferB);
			ortin.nds.ppu->updateScreen = false;
		}

		ortin.drawFrame();
	}

	SDL_CloseAudioDevice(audioDevice);
	wavFile.close();

	return 0;
}