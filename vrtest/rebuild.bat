@echo off
title VR Clean + Rebuild

echo ================================
echo   VRTEST CLEAN REBUILD SCRIPT
echo ================================
echo.

REM Przejdz do folderu skryptu
cd /d "%~dp0"

echo [1/4] Zatrzymywanie vrtest.exe...
taskkill /F /IM vrtest.exe >nul 2>nul

echo [2/4] Usuwanie folderu build...
if exist build (
    rmdir /S /Q build
    echo Build usuniety.
) else (
    echo Build nie istnieje.
)

echo.
echo [3/4] Usuwanie starego exe...

if exist "vrtest\bin\Release\vrtest.exe" (
    del /F /Q "vrtest\bin\Release\vrtest.exe"
    echo vrtest.exe usuniety.
) else (
    echo vrtest.exe nie istnieje.
)

echo.
echo [4/4] Uruchamianie build.bat...
call build.bat

echo.
echo ================================
echo   GOTOWE
echo ================================
pause