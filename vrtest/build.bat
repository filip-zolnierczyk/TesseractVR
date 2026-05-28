@echo off
REM ============================================================
REM  build.bat - buduje projekt vrtest (Windows)
REM  Wymagania:
REM    - Visual Studio 2019+ (lub Build Tools)
REM    - Vulkan SDK  (https://vulkan.lunarg.com)
REM    - OpenXR SDK  (https://github.com/KhronosGroup/OpenXR-SDK/releases)
REM    - CMake 3.16+
REM ============================================================

echo === Budowanie VR Test App ===

REM Skompiluj shadery (wymaga glslc z Vulkan SDK)
echo [1/3] Kompilacja shaderów...
if not exist "shaders\compiled" mkdir shaders\compiled

where glslc >nul 2>&1
if %ERRORLEVEL% == 0 (
    glslc shaders\fullscreen.vert -o shaders\compiled\vert.spv
    glslc shaders\fullscreen.frag -o shaders\compiled\frag.spv
    echo     Shadery skompilowane.
) else (
    echo     UWAGA: glslc nie znaleziony. Shadery musza byc juz wbudowane w kod.
    echo     Zainstaluj Vulkan SDK i dodaj do PATH.
)

REM Konfiguracja CMake
echo [2/3] Konfiguracja CMake...
if not exist "build" mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo BLAD: Konfiguracja CMake nie powiodla sie!
    echo Sprawdz czy Vulkan SDK i OpenXR SDK sa zainstalowane.
    cd ..
    pause
    exit /b 1
)

REM Build
echo [3/3] Kompilacja...
cmake --build . --config Release
cd ..

if exist "bin\Relese\vrtest.exe" (
    echo.
    echo === Sukces! ===
    echo Uruchom: bin\vrtest.exe
    echo.
    echo PRZED URUCHOMIENIEM:
    echo   1. Uruchom SteamVR lub Oculus runtime
    echo   2. Podlacz headset VR
    echo.
) else (
    echo BLAD: Kompilacja nie powiodla sie!
)
pause
