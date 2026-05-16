# TesseractVR

Minimalne repo pod inżynierkę: jedna aplikacja Vulkan, full-screen triangle i shaderowy 4D hipersześcian.

## Zawartość

- `src/main.cpp` - minimalna aplikacja Vulkan/GLFW.
- `shaders/09_shader_base.vert` - vertex shader dla full-screen triangle.
- `shaders/09_shader_base.frag` - fragment shader z 4D hipersześcianem i rotacją w płaszczyźnie XW.
- `CMakeLists.txt` - lokalny build przez CMake.

## Wymagania

- Vulkan SDK z `glslangValidator`.
- `glfw3` zainstalowane przez vcpkg albo w systemie.
- Generator CMake kompatybilny z Visual Studio lub Ninja.

## Układ katalogów

```text
TesseractVR/
	CMakeLists.txt
	README.md
	vcpkg.json
	src/
		main.cpp
	shaders/
		09_shader_base.vert
		09_shader_base.frag
```

## Build lokalny

Przykład dla Visual Studio 2022 i vcpkg:

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build-vs --config Release --target TesseractVR
```

## Uruchamianie

Dla generatora Visual Studio:

```powershell
cd build-vs/TesseractVR
.\Release\TesseractVR.exe
```

Jeśli używasz Ninja, uruchom plik wykonywalny z katalogu wyjściowego, w którym znajdują się też `shaders/vert.spv` i `shaders/frag.spv`.