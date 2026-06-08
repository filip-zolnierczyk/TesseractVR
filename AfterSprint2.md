🛠️ TesseractVR – Dokumentacja Wewnętrzna Backend Graficznego (Vulkan Core)
Cześć! Przebudowałem całkowicie architekturę naszego silnika. Monolityczny kod z main.cpp został rozbity na czyste, modularne klasy, a potok graficzny został dostosowany do standardów produkcyjnych.

🚀 Główne zmiany (Co zostało zrobione):
Rozbicie na klasy: Logika okna i pętli głównej znajduje się teraz w Application, a całe niskopoziomowe zarządzanie grafiką w VulkanRenderer.

Przejście na UBO (Uniform Buffer Objects): Usunąłem ograniczone pamięciowo Push Constants. Dane do shaderów lecą teraz przez pełnoprawny bufor UniformBufferObject z obsługą układów deskryptorów (Descriptor Sets).

Wdrożenie VMA (Vulkan Memory Allocator): Alokacja bufora UBO i zarządzanie pamięcią GPU są teraz w pełni kontrolowane przez bibliotekę AMD VMA (koniec z ręcznym vkAllocateMemory). Zależność dodaje się automatycznie przez vcpkg.json.

📐 (Matematyka, Raymarching i Shadery)
Wszystkie dane sterujące potokiem matematycznym i animacją docierają teraz do shaderów za pośrednictwem bloku UBO (Binding = 0). Zmodyfikowałem już plik 09_shader_base.frag – nie musisz nic zmieniać w konfiguracji deskryptorów.

Jak z tego korzystać w shaderze (09_shader_base.frag):
Push constants zostały zastąpione globalną strukturą, z której zmienne wyciągasz przez przedrostek ubo.:
```c++
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;         // Macierz widoku kamery 3D
    mat4 proj;         // Macierz rzutowania (Vulkan clip-space)
    vec2 resolution;   // Aktualna rozdzielczość okna (szerokość, wysokość)
    float time;        // Ciągły czas z CPU do animacji rotacji
    float w_offset;    // Suwak czwartego wymiaru (oś W) kontrolowany klawiaturą
} ubo;
```
Sterowanie z klawiatury pod testy Raymarchingu:
W pliku Application.cpp zaimplementowałem bazową obsługę klawiatury za pomocą GLFW. Wartość ubo.w_offset zmienia się dynamicznie w pętli:

Strzałka w GÓRĘ -> Zwiększa w_offset (ruch w osi W w przód)

Strzałka w DÓŁ -> Zmniejsza w_offset (ruch w osi W w tył)

Jeśli potrzebujesz podpiąć inne klawisze do testowania swoich rotacji (np. obrót w płaszczyznach XW/YW), dopisz zmienne do Application.h i zaktualizuj metodę Application::processInput() w następujący sposób:

```c++
if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
    // Twoja zmienna kąta obrotu += speed;
}
```
🥽 (Integracja VR i OpenXR)
Silnik jest teraz w pełni agnostyczny i przygotowany na wstrzyknięcie kontekstu VR. Przygotowałem dla Ciebie dedykowane interfejsy i gettery, dzięki którym OpenXR może bezproblemowo przejąć kontrolę nad Vulkanem.

1. Wstrzykiwanie rozszerzeń OpenXR do instancji i urządzenia Vulkana:
Podczas inicjalizacji OpenXR runtime poinformuje Cię, jakich rozszerzeń Vulkana wymaga (zarówno na poziomie Instancji, jak i Urządzenia Logicznego). Możesz je teraz przekazać bezpośrednio jako wektory do metody init:

```c++
// W miejscu inicjalizacji (np. w Twoim module VR)
std::vector<const char*> oxrInstanceExts = { /* rozszerzenia instancji z OpenXR */ };
std::vector<const char*> oxrDeviceExts    = { /* rozszerzenia urządzenia z OpenXR */ };
// Przekazanie do silnika
renderer.init(window, oxrInstanceExts, oxrDeviceExts);
```
2. Pobieranie uchwytów Vulkana dla struktury Graphics Binding:
OpenXR potrzebuje wskaźników do zainicjalizowanych obiektów Vulkana, aby utworzyć strukturę XrGraphicsBindingVulkanKHR. Wystawiłem dla Ciebie publiczne gettery w klasie VulkanRenderer:

```C++
XrGraphicsBindingVulkanKHR graphicsBinding{};
graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
graphicsBinding.instance = renderer.getInstance();
graphicsBinding.physicalDevice = renderer.getPhysicalDevice();
graphicsBinding.device = renderer.getDevice();
graphicsBinding.queueFamilyIndex = renderer.getGraphicsQueueFamily();
graphicsBinding.queueIndex = 0; // Pierwsza kolejka
```
3. Wpięcie macierzy oczu w pętli głównej (Application.cpp):
W metodzie Application::mainLoop() przygotowałem flagę bool enableVR = false; oraz blok warunkowy if-else. Gdy przełączysz ją na true, Twoim zadaniem będzie pobranie pozycji głowy i oczu z OpenXR i nadpisanie macierzy:

```C++
if (enableVR) {
    // 1. Pobierz transformacje z sesji OpenXR (dla danego oka)
    // 2. Wylicz view i proj na podstawie parametrów gogli
    view = mojelogo_OXR.getEyeViewMatrix(eyeIndex);
    proj = mojelogo_OXR.getEyeProjectionMatrix(eyeIndex);
}
```
// Renderer automatycznie zmapuje te macierze do UBO i wyśle do shadera
```
renderer.drawFrame(time, currentWOffset, view, proj);
```
Builduje się u nas na ustandaryzowanych presetach z CMakePresets.json. W razie pytań o potok 
pamięci deskryptorów lub synchronizację klatek, podbijajcie śmiało! Zapnijmy to razem.