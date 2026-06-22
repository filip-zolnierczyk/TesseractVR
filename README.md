# TesseractVR

Minimalne repo pod inżynierkę: jedna aplikacja Vulkan, full-screen triangle i shaderowy 4D hipersześcian z interaktywną kontrolą rotacji w 6 płaszczyznach 4D.

## Zawartość

- `src/main.cpp` - minimalna aplikacja Vulkan/GLFW (wykorzystująca VMA oraz Uniform Buffer Objects).
- `src/Application.h/.cpp` - główna klasa aplikacji z obsługą wejścia (klawiszy 1-6).
- `src/VulkanRenderer.h/.cpp` - warstwa renderowania Vulkan.
- `shaders/09_shader_base.vert` - vertex shader dla full-screen triangle.
- `shaders/09_shader_base.frag` - fragment shader z 4D hipersześcianem i rotacją w 6 płaszczyznach (XY, XZ, XW, YZ, YW, ZW).
- `CMakeLists.txt` - konfiguracja builda.
- `CMakePresets.json` - ustandaryzowane presety kompilacji dla całego zespołu.
- `vcpkg.json` - manifest zależności.

## Wymagania

### Narzędzia
- **Visual Studio 2022** (lub kompatybilny kompilator MSVC)
- **CMake 3.21+**
- **Vulkan SDK** (z `glslangValidator`)
- **vcpkg** (menadżer zależności C++)

### Biblioteki (instalowane automatycznie przez vcpkg)
- `glfw3` - obsługa okna i wejścia
- `glm` - biblioteka matematyczna
- `vulkan-memory-allocator` - zarządzanie pamięcią GPU

### Konfiguracja środowiska

1. **Zainstaluj vcpkg** (jeśli jeszcze go nie masz):
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```

2. **Ustaw zmienną środowiskową `VCPKG_ROOT`**:
   - **Windows (PowerShell jako Administrator)**:
     ```powershell
     [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\path\to\vcpkg", "Machine")
     ```
   - Lub ustawić w `Zmienne środowiskowe` → `Zmienne systemowe` → `Nowa`

3. **Sprawdź instalację**:
   ```powershell
   $env:VCPKG_ROOT  # Powinno wyświetlić ścieżkę do vcpkg
   ```

## Build lokalny (CMake Presets)

Dzięki `CMakePresets.json` proces budowania jest ustandaryzowany niezależnie od maszyny. W terminalu w głównym folderze projektu wykonaj:

```powershell
# 1. Konfiguracja i pobranie zależności (vcpkg)
cmake --preset vulkan-windows-x64

# 2. Kompilacja projektu i shaderów
cmake --build --preset release
```

Executable będzie dostępny w: `build-vs/TesseractVR/Release/TesseractVR.exe`

## Uruchomienie

```powershell
# Z głównego folderu projektu:
cd build-vs/TesseractVR/Release
.\TesseractVR.exe
```

**Ważne**: Aplikacja musi być uruchomiona z katalogu `build-vs/TesseractVR/Release/`, ponieważ tam znajdują się skompilowane shadery (`.spv`).

## Sterowanie

| Klawisz | Akcja |
|---------|-------|
| **1** | Rotacja w płaszczyźnie **XY** |
| **2** | Rotacja w płaszczyźnie **XZ** |
| **3** | Rotacja w płaszczyźnie **XW** |
| **4** | Rotacja w płaszczyźnie **YZ** |
| **5** | Rotacja w płaszczyźnie **YW** |
| **6** | Rotacja w płaszczyźnie **ZW** |
| **Shift** + (1-6) | Rotacja w **przeciwnym kierunku** |
| **Spacja** | **Pauza/Wznowienie** animacji |
| **Esc** | **Zamknięcie** okna |

### Przykład
```
Przytrzymaj "3" aby obrócić hipersześcian wokół osi XW
Przytrzymaj "Shift + 5" aby obrócić w odwrotną stronę wokół osi YW
```

## Rozwiązywanie problemów

### CMake nie może znaleźć vcpkg.cmake
- Sprawdź czy `VCPKG_ROOT` jest poprawnie ustawiona: `echo $env:VCPKG_ROOT`
- Może wymagać ponownego otwarcia PowerShell/VS Code po ustawieniu zmiennej

### Błąd "glslangValidator not found"
- Upewnij się że Vulkan SDK jest zainstalowany
- Vulkan SDK powinien dodać `glslangValidator` do PATH

### Aplikacja nie uruchamia się
- Sprawdź czy jesteś w katalogu `build-vs/TesseractVR/Release/`
- Upewnij się że shaders `frag.spv` i `vert.spv` znajdują się w tym katalogu

## Architektura

- **Frontend**: GLFW (GLFW3) + Vulkan 1.0
- **Renderowanie**: Full-screen triangle → Ray marching w 4D
- **Shader**: Fragment shader implementuje ray marching z SDF (Signed Distance Field) dla 4D hipersześcianu
- **Kontrola**: Interaktywna rotacja w 6 niezależnych płaszczyznach 4D