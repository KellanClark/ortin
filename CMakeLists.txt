cmake_minimum_required(VERSION 3.14)
project(Ortin)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(SDL2 CONFIG REQUIRED)

add_subdirectory(modules/nativefiledialog-extended)
add_subdirectory(modules/fmt)
add_subdirectory(modules/mio)

include_directories(
	modules/imgui/
	modules/imgui_club/imgui_memory_editor/
	modules/mINI/src/mini

	include/
	include/emulator-common/
)

add_executable(Ortin
	modules/imgui/imgui_draw.cpp
	modules/imgui/imgui_demo.cpp
	modules/imgui/imgui_tables.cpp
	modules/imgui/imgui_widgets.cpp
	modules/imgui/imgui.cpp
	modules/imgui/backends/imgui_impl_sdl2.cpp
	modules/imgui/backends/imgui_impl_opengl3.cpp

	src/ortin.cpp
	src/main.cpp
	src/menus/file.cpp
	src/menus/emulation.cpp
	src/menus/debug.cpp
	src/menus/graphics.cpp
	src/menus/dev.cpp

	src/emulator/nds.cpp
	src/emulator/busshared.cpp
	src/emulator/ipc.cpp
	src/emulator/ppu.cpp
	src/emulator/cartridge/key1.cpp
	src/emulator/cartridge/gamecard.cpp
	src/emulator/dma.cpp
	src/emulator/timer.cpp
	src/emulator/nds9/busarm9.cpp
	src/emulator/nds9/dsmath.cpp
	src/emulator/nds7/busarm7.cpp
	src/emulator/nds7/rtc.cpp
	src/emulator/nds7/spi.cpp
	src/emulator/nds7/apu.cpp
)

target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)

target_compile_options(Ortin PRIVATE
	-ftemplate-depth=20000
	-fconstexpr-depth=20000
	-MMD
	-MP
	-mbmi2
	#-Wall
	#-fsanitize=address
	#-fsanitize=leak
	#-fsanitize=pointer-compare
	#-fsanitize=pointer-subtract
	#-fstack-protector-all
	#-fsanitize=undefined
	#-fsanitize=thread
)

if (WIN32)
	target_link_libraries(Ortin PRIVATE
		SDL2::SDL2
		SDL2::SDL2main
		nfd
		fmt
		mio::mio
		opengl32
		#-fsanitize=address
		#-fsanitize=leak
		#-fsanitize=pointer-compare
		#-fsanitize=pointer-subtract
		#-fstack-protector-all
		#-fsanitize=undefined
		#-fsanitize=thread
	)
else()
	target_link_libraries(Ortin PRIVATE
		SDL2::SDL2
		SDL2::SDL2main
		nfd
		fmt
		mio::mio
		-lstdc++
		-ldl
		-lGL
		-lm
		-lpthread
		#-fsanitize=address
		#-fsanitize=leak
		#-fsanitize=pointer-compare
		#-fsanitize=pointer-subtract
		#-fstack-protector-all
		#-fsanitize=undefined
		#-fsanitize=thread
	)
endif()
