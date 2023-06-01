![Ortin](logo.png)

A *very* work in progress emulator for the Nindendo DS handheld game console.

## Building
Precompiled binaries are provided for Windows and Linux under Releases.

### Windows Requirements
I'm most likely going to change some aspects of the Windows build process very soon, so this is a placeholder.

### Linux Requirements
- At least GCC 12 or Clang 13
- `libsdl2-dev` >= 2.16
- `libgtk-3-dev`

### Instrucions
Clone the repository with `git clone https://github.com/KellanClark/ortin.git --recurse-submodule`
Then use the standard CMake build process
```
cd ortin
mkdir build
cd build
cmake ..
make
```