# TesseractVR

Minimalne repo pod inżynierkę: jedna aplikacja Vulkan, full-screen triangle i shaderowy 4D hipersześcian.

## Zawartość

- `src/main.cpp` - minimalna aplikacja Vulkan/GLFW (wykorzystująca VMA oraz Uniform Buffer Objects).
- `shaders/09_shader_base.vert` - vertex shader dla full-screen triangle.
- `shaders/09_shader_base.frag` - fragment shader z 4D hipersześcianem i rotacją w płaszczyźnie XW.
- `CMakeLists.txt` - konfiguracja builda.
- `CMakePresets.json` - ustandaryzowane presety kompilacji dla całego zespołu.
- `vcpkg.json` - manifest zależności.

## Wymagania

- Vulkan SDK z `glslangValidator`.
- `glfw3` oraz `vulkan-memory-allocator` (zarządzane automatycznie przez vcpkg).
- Ustawiona zmienna środowiskowa systemu: `VCPKG_ROOT` wskazująca na główny katalog instalacji vcpkg.

## Build lokalny (CMake Presets)

Dzięki `CMakePresets.json` proces budowania jest ustandaryzowany niezależnie od maszyny. W terminalu w głównym folderze projektu wykonaj:

```powershell
# 1. Konfiguracja i pobranie zależności (vcpkg)
cmake --preset vulkan-windows-x64

# 2. Kompilacja projektu i shaderów
cmake --build --preset release