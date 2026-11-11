# Sterowanie – TesseractVR

Stan na podstawie kodu: `Application::processInput()` ([src/Application.cpp](src/Application.cpp)) oraz akcji kontrolerów w [src/OpenXRManager.cpp](src/OpenXRManager.cpp).

Wszystkie obroty są ciągłe: kąt zmienia się o stałą wartość (`0.01` rad) w każdej klatce, dopóki klawisz/przycisk jest wciśnięty.

## Klawiatura (tryb desktop i VR)

| Klawisz | Akcja |
|---------|-------|
| **1** | Obrót w płaszczyźnie **XY** |
| **2** | Obrót w płaszczyźnie **XZ** |
| **3** | Obrót w płaszczyźnie **XW** |
| **4** | Obrót w płaszczyźnie **YZ** |
| **5** | Obrót w płaszczyźnie **YW** |
| **6** | Obrót w płaszczyźnie **ZW** |
| **Shift** (lewy/prawy) + **1–6** | Obrót w przeciwną stronę |
| **E** | Obrót w płaszczyźnie **XZ** (kierunek dodatni) |
| **Q** | Obrót w płaszczyźnie **XZ** (kierunek ujemny) |
| **↑ / ↓** | Przesunięcie przekroju 3D wzdłuż osi **W** (+ / −) |
| **Spacja** | Przełączenie flagi pauzy (patrz uwaga niżej) |
| **Esc** | Zamknięcie aplikacji |

## Kontrolery Oculus Touch (tylko tryb VR / OpenXR)

| Wejście | Akcja |
|---------|-------|
| **Lewa gałka, oś X** | Obrót **XY** (proporcjonalnie do wychylenia) |
| **Lewa gałka, oś Y** | Obrót **XZ** (wychylenie w górę obraca w kierunku ujemnym) |
| **Prawa gałka, oś X** | Obrót **YZ** |
| **Prawa gałka, oś Y** | Przesunięcie przekroju wzdłuż osi **W** |
| **A** (prawy kontroler) | Obrót **XW** |
| **B** (prawy kontroler) | Obrót **YZ** |
| **X** (lewy kontroler) | Obrót **YW** |
| **Y** (lewy kontroler) | Obrót **ZW** |
| **Lewy grip** (powyżej 50%) + ruch kontrolera | Obrót 3D: **XY / XZ / YZ** (zmiana orientacji kontrolera → obrót odpowiednio wokół osi Z / Y / X) |
| **Prawy grip** (powyżej 50%) + ruch kontrolera | Obrót 4D: **XW / YW / ZW** (osie X / Y / Z kontrolera) |
| **Prawy spust** (powyżej 50%) | Następna figura |
| **Lewy spust** (powyżej 50%) | Poprzednia figura |

- Gałki mają martwą strefę 0.15; poniżej tej wartości wychylenie jest ignorowane.
- Zmiana figury (spustem) działa jednorazowo na wciśnięcie, zapętla się (po ostatniej figurze wraca pierwsza) i **resetuje** wszystkie kąty obrotu oraz przesunięcie W.
- Przycisk **B** i prawa gałka X obracają w tej samej płaszczyźnie (YZ).
- Przycisk **Y** (ZW) jest przypisany do lewego kontrolera, **A** i **B** do prawego.
- Zmiana figur jest dostępna tylko w VR; w trybie desktop nie ma na to klawisza.

## Płaszczyzny obrotu 4D

| Symbol | Osie |
|--------|------|
| XY, XZ, YZ | Zwykłe obroty 3D |
| XW, YW, ZW | Obroty „4D”, mieszające oś W z jedną z osi przestrzennych |

## Figury

Kolejność (zmiana spustami): 0 = box, 1 = sphere, 2 = capsule, 3 = rounded box, 4 = ellipsoid, 5 = torus, 6 = cross-polytope.

- Aktualna figura to `currentShape` w [src/Application.h](src/Application.h). Trafia do shadera jako `shapeId` w UBO, a [shaders/09_shader_base.frag](shaders/09_shader_base.frag) wybiera w `mapScene()` odpowiednią funkcję SDF.
- Nazwy figur na pasku HUD są w tablicy `NAMES` w [shaders/hud.frag](shaders/hud.frag) (ta sama kolejność).

## Panel ustawień

Otwierany/zamykany przyciskiem menu (3 kreski) na lewym kontrolerze, a na desktopie klawiszem **Tab**. Lista pozycji, każda z nazwą i aktualnym trybem; wybrana pozycja jest odwrócona (czarne tło).

| Wejście | Akcja w otwartym panelu |
|---------|-------------------------|
| **Y / X** (lewy kontroler) | Przewijanie listy ustawień w górę / w dół (zapętlone) |
| **Prawy / lewy spust** | Następny / poprzedni tryb wybranego ustawienia (zapętlone) |
| Desktop: **↑ / ↓** i **← / →** | To samo co Y / X i spusty |

Dopóki panel jest otwarty, przyciski, gałki, gripy i spusty **nie obracają figury ani nie zmieniają figur**; klawiatura sceny też jest zablokowana.

Ustawienia:
1. **COLOR MODE:** NORMALS (kolory od normalnych + światło), FACES 4D, FACES STABLE, SOLID.
2. **SCALE:** globalna skala figury 50%, 75%, 100%, 125%, 150%, 200%.

Stan ustawień jest w `Application.h` (`colorMode`, `scaleIndex`, `selectedSetting`) i trafia do shaderów przez UBO.

## Widok i HUD

- **Głowa (VR):** ruch głową zmienia kamerę (head-tracking). Pasek HUD na dole jest nieruchomy względem ekranu.
- **HUD:** pasek z nazwą aktualnie wyświetlanej figury.
- **Tytuł okna:** pokazuje aktualne FPS.

## Uwagi

- **Spacja:** przełącza `isPaused`, a czas shadera rośnie tylko gdy flaga jest ustawiona na `true`. Czas (`ubo.time`) jest obecnie używany tylko w nieużywanej funkcji `getLocalPoint_rot`, więc Spacja nie ma na razie widocznego efektu na obrót figury. Nazwa „pauza” jest więc myląca (domyślnie `isPaused = false`).
- **Q i E:** zmieniają ten sam kąt co klawisz **2**, więc 2, E (dodatni) i Q (ujemny) działają na XZ.
