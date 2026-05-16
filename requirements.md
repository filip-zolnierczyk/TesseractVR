# TesseractVR - Instrukcja Instalacji

Przewodnik krok po kroku do zainstalowania wszystkich narzędzi potrzebnych do budowania i uruchamiania projektu TesseractVR na Windows.

## 📋 Wymagania systemowe

- **OS**: Windows 10 / Windows 11
- **Architektura**: x64
- **GPU**: Karta graficzna kompatybilna z Vulkanem 1.0
- **Wolne miejsce**: ~5-10 GB (SDK + build artifacts)

---

## ✅ Krok 1: Zainstaluj narzędzia do kompilacji C++

### Opcja A: Build Tools (Lekka - Rekomendowana)

Pobierz **Visual Studio Build Tools 2022** z: https://visualstudio.microsoft.com/downloads/
- Wyszukaj "Build Tools for Visual Studio 2022"

Zainstaluj:
1. Uruchom instalator
2. W **Workloads** zaznacz:
   - ✅ **Desktop development with C++**
3. Kliknij **Install** i czekaj (~2-3 minuty)

### Opcja B: Visual Studio Community (Pełne IDE)

Pobierz z: https://visualstudio.microsoft.com/vs/

Zainstaluj:
1. Uruchom instalator
2. W **Workloads** zaznacz:
   - ✅ **Desktop development with C++**
3. Kliknij **Install** i czekaj (~5-10 minut)

### Weryfikacja
Otwórz PowerShell i wykonaj:
```powershell
cmake --version
```
Powinno wyświetlić wersję CMake (minimum 3.20).

---

## ✅ Krok 2: Zainstaluj Vulkan SDK

### Pobierz
1. Przejdź na: https://vulkan.lunarg.com/sdk/home
2. Pobierz **Vulkan SDK** dla Windows (najnowszą wersję)

### Zainstaluj
1. Uruchom instalator `.exe`
2. Zaakceptuj warunki licencji
3. W **Optional components** zaznacz:
   - ✅ **Vulkan Tools** (zawiera `glslangValidator`)
   - ✅ **Glslang validation layer**
4. Kliknij **Install** i czekaj (~2-3 minuty)
5. **Restart systemu** nie wymagany, ale zalecany

### Weryfikacja
Otwórz PowerShell i wykonaj:
```powershell
glslangValidator --version
```
Powinno wyświetlić wersję narzędzia.

### Zmienne środowiskowe
Vulkan SDK powinno automatycznie ustawić:
- `VULKAN_SDK` = ścieżka do instalacji SDK (np. `C:\VulkanSDK\1.3.290`)

Weryfikacja:
```powershell
echo $env:VULKAN_SDK
```

---

## ✅ Krok 3: Zainstaluj vcpkg

### Klonuj repozytorium
Otwórz PowerShell w wybranym katalogu (np. `C:\vcpkg`) i wykonaj:
```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
```

### Zbuduj vcpkg
```powershell
.\bootstrap-vcpkg.bat
```
Czekaj ~1-2 minuty aż bootstrap się skończy.

### Zainstaluj zależności z vcpkg.json
Przejdź do katalogu projektu TesseractVR:
```powershell
cd C:\Users\Marcel\Documents\studia\tesseract\TesseractVR
```

Zainstaluj pakiety:
```powershell
& "C:\vcpkg\vcpkg" install --triplet x64-windows
```

Czekaj ~3-5 minut. Vcpkg automatycznie zainstaluje:
- `glfw3` (GLFW library)
- `vcpkg-cmake` (CMake toolchain support)
- `vcpkg-cmake-config` (CMake configuration)

### Weryfikacja
Po zakończeniu, w terminalu powinno pojawić się:
```
Total elapsed time: ... ms

The following packages are already installed:
  ...
```

---

## ✅ Krok 4: Edytor kodu (Opcjonalny)

### Zalecane opcje:

**VS Code (Lekkie, rekomendowane)**
1. Pobierz z: https://code.visualstudio.com/
2. Zainstaluj rozszerzenia:
   - C/C++ (Microsoft)
   - CMake (Microsoft)
   - CMake Tools (Microsoft)

**Visual Studio Community**
- Jeśli wybrałeś opcję B w Kroku 1, możesz używać pełnego IDE

**Inne edytory**
- Vim, Neovim, Sublime Text, etc. (wszystkie działają z CMake)

---6

## ✅ Krok 5: Klonuj/Przygotuj repozytorium

Jeśli jeszcze nie masz lokalnej kopii:
```powershell
git clone https://github.com/filip-zolnierczyk/TesseractVR.git
cd TesseractVR
```

Lub aktualizuj istniejące repozytorium:
```powershell
cd C:\Users\Marcel\Documents\studia\tesseract\TesseractVR
git fetch origin
git checkout main  # lub MadaszoTestBranch
```

---

## ✅ Krok 5: Skonfiguruj i zbuduj projekt

### 1. Konfiguruj CMake

Wykonaj z głównego katalogu projektu:
```powershell
cmake -S . -B build-vs `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

**Uwaga**: Zastąp `C:/vcpkg` ścieżką do Twojej instalacji vcpkg (użyj forward slashów `/`).

Czekaj ~1-2 minuty. Powinna się pojawić wiadomość:
```
-- Build files have been written to: ...
```

### 2. Zbuduj projekt

Z wiersza poleceń (universalnie na wszystkich IDE):
```powershell
cmake --build build-vs --config Release --target TesseractVR
```

Czekaj ~3-5 minut na kompilację.

Alternatywnie, jeśli masz VS Code z CMake Tools:
1. Otwórz folder projektu w VS Code
2. Wybierz **Kit**: Visual Studio 17 2022 (x64)
3. Kliknij "Build" w dolnym pasku lub naciśnij `F7`

### 3. Weryfikacja builda

Sprawdź czy plik wykonywalny istnieje:
```powershell
Test-Path build-vs/TesseractVR/Release/TesseractVR.exe
```

Powinno wyświetlić `True`.

Sprawdź czy shadery zostały skompilowane:
```powershell
Get-ChildItem build-vs/TesseractVR/shaders/
```

Powinny być pliki:
- `vert.spv`
- `frag.spv`

---

## ✅ Krok 6: Uruchom aplikację
7
Przejdź do katalogu z plikiem wykonywalnym:
```powershell
cd build-vs/TesseractVR/Release
```

Uruchom aplikację:
```powershell
.\TesseractVR.exe
```

**Powinno się otworzyć okno 800x600 z animowanym 4D hipersześcianem!**

Aby zamknąć aplikację, zamknij okno.

---

## 🔧 Troubleshooting

### Problem: `glslangValidator` nie znaleziony
**Rozwiązanie**: 
- Uruchom Vulkan SDK installer ponownie, zaznacz `Vulkan Tools`
- Lub dodaj ręcznie `%VULKAN_SDK%\bin` do zmiennej środowiskowej `PATH`

### Problem: CMake configuration fails - `Vulkan not found`
**Rozwiązanie**:
- Uruchom PowerShell z uprawnieniami administratora
- Sprawdź czy `VULKAN_SDK` jest ustawiona: `echo $env:VULKAN_SDK`

### Problem: vcpkg package installation fails
**Rozwiązanie**:
- Usuń folder `vcpkg_installed` z projektu: `Remove-Item -Recurse vcpkg_installed`
- Uruchom instalację vcpkg ponownie (Krok 3)

### Problem: "failed to find a suitable GPU"
**Rozwiązanie**:
- Sprawdź czy Twoja GPU obsługuje Vulkan: https://www.khronos.org/vulkan/
- Zainstaluj najnowsze sterowniki GPU

### Problem: Build trwa bardzo długo
- To normalne dla pierwszego builda (~5-10 minut)
- Kolejne buildy będą szybsze (~1-2 minuty)

---

## 📝 Zmienne środowiskowe (podsumowanie)

Powinny być automatycznie ustawione:

```powershell
# Sprawdź:
$env:VULKAN_SDK      # Vulkan SDK path
$env:PATH            # Powinno zawierać %VULKAN_SDK%\bin
```

Jeśli nie są ustawione, dodaj ręcznie w **System Properties** → **Environment Variables**:
- `VULKAN_SDK` = `C:\VulkanSDK\1.3.290` (lub wersja którą zainstalowałeś)

---

## 🎯 Podsumowanie kroków

| Lp. | Krok | Narzędzie | Czas |
|-----|------|-----------|------|
| 1 | Visual Studio 2022 | Installer | 5-10 min |
| 2 | Build Tools / VS | Installer | 2-10 min |
| 2 | Vulkan SDK | Installer | 2-3 min |
| 3 | vcpkg setup | Bootstrap + install | 3-5 min |
| 4 | Edytor (opcjonalny) | Installer | 1-5 min |
| 5 | Git repository | Terminal | <1 min |
| 6 | CMake configure | Terminal | 1-2 min |
| 6 | Build projekt | Terminal/IDE | 3-5 min |
| 7 | Run aplikacja | Terminal | <1 min |

**Całkowity czas: ~15-35 minut** (zależy od wybranych opcji
---

## ✨ Gotowe!

Jeśli wszystko powiodło się, powinieneś zobaczyć okno z animowanym 4D hipersześcianem. Gratulacje! 🎉

---

## 📚 Przydatne linki

- [Vulkan SDK](https://vulkan.lunarg.com/)
- [CMake Documentation](https://cmake.org/documentation/)
- [vcpkg GitHub](https://github.com/microsoft/vcpkg)
- [Visual Studio](https://visualstudio.microsoft.com/)
- [GLFW Documentation](https://www.glfw.org/)

---

## 💡 Wskazówki dla przyszłych rebuild'ów

Po zmianach w kodzie:

```powershell
# Rebuild project (szybko)
cmake --build build-vs --config Release --target TesseractVR

# Jeśli zmienisz CMakeLists.txt
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build-vs --config Release --target TesseractVR

# Clean rebuild (jeśli coś się zepsuje)
Remove-Item -Recurse build-vs
# potem ponownie: cmake configure + build
```

---

**Ostatnia aktualizacja**: May 2026
