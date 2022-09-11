
#include "ortin.hpp"

Ortin::Ortin() : emuThread(&NDS::run, std::ref(nds)) {
	error = false;
	penDown = false;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("Error: %s\n", SDL_GetError());
		error = true;
		return;
	}

	const char* glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	window = SDL_CreateWindow("NDS", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
	gl_context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(0); // Disable vsync

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();
	clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	FileMenu *fileMenuPtr = new FileMenu();
	submenus.push_back(fileMenuPtr);
	DebugMenu *debugMenuPtr = new DebugMenu();
	submenus.push_back(debugMenuPtr);
	GraphicsMenu *graphicsMenuPtr = new GraphicsMenu();
	submenus.push_back(graphicsMenuPtr);
	DevMenu *devMenuPtr = new DevMenu();
	submenus.push_back(devMenuPtr);

	// Textures for main display
	glGenTextures(1, &engineATexture);
	glBindTexture(GL_TEXTURE_2D, engineATexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenTextures(1, &engineBTexture);
	glBindTexture(GL_TEXTURE_2D, engineBTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // GL_LINEAR
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

Ortin::~Ortin() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	emuThread.detach();
}

void Ortin::drawFrame() {
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	drawScreens();

	ImGui::BeginMainMenuBar();
	for (int i = 0; i < submenus.size(); i++)
		submenus[i]->drawMenu();
	ImGui::EndMainMenuBar();

	for (int i = 0; i < submenus.size(); i++)
		submenus[i]->drawWindows();

	// Rendering
	ImGui::Render();
	ImGuiIO& io = ImGui::GetIO();
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(window);
}

void Ortin::drawScreens() {
	ImGui::Begin("DS Screen");

	ImGui::Image((void*)(intptr_t)(nds.ppu.displaySwap ? ortin.engineATexture : ortin.engineBTexture), ImVec2(256 * 1, 192 * 1));
	ImGui::ImageButton((void*)(intptr_t)(nds.ppu.displaySwap ? ortin.engineBTexture : ortin.engineATexture), ImVec2(256 * 1, 192 * 1), ImVec2(0, 0), ImVec2(1, 1), 0);

	if (ImGui::IsItemActive() && ImGui::IsItemHovered()) { // Touchscreen
		auto mousePos = ImGui::GetMousePos();
		auto topLeft = ImGui::GetItemRectMin();
		auto size = ImGui::GetItemRectSize();

		nds.nds7.spi.touchscreen.xPosition = ((mousePos.x - topLeft.x) / size.x) * 0x0FF0;
		nds.nds7.spi.touchscreen.yPosition = ((mousePos.y - topLeft.y) / size.y) * 0x0BF0;
		penDown = true;
		//printf("%03X  %03X\n", nds.nds7.spi.touchscreen.xPosition, nds.nds7.spi.touchscreen.yPosition);
	} else {
		nds.nds7.spi.touchscreen.xPosition = 0;
		nds.nds7.spi.touchscreen.yPosition = 0;
		penDown = false;
	}

	ImGui::End();
}