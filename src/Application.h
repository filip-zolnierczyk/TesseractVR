#pragma once

#include "VulkanRenderer.h"
#include "OpenXRManager.h"
#include <GLFW/glfw3.h>

class Application {
public:
    void run();

private:
    GLFWwindow* window;
    VulkanRenderer renderer;
    OpenXRManager vr;
    bool vrActive = false; // true, gdy sesja OpenXR zostala pomyslnie utworzona
    std::vector<const char*> xrInstanceExtensions;
    std::vector<const char*> xrDeviceExtensions;
    
    float currentWOffset = 0.0f; // Nasza zmienna dla 4 wymiaru
    int currentShape = 0;
    bool leftTriggerWasPressed = false;
    bool rightTriggerWasPressed = false;
    bool menuWasPressed = false;
    bool settingsOpen = false;
    bool tabWasPressed = false;

    // --- Panel ustawien ---
    static constexpr int kSettingCount = 2;     // 0 = tryb koloru, 1 = skala globalna
    static constexpr int kColorModeCount = 4;   // 0 = normalne, 1 = sciany 4D, 2 = sciany stabilne, 3 = jednolity
    static constexpr float kScales[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    static constexpr int kScaleCount = sizeof(kScales) / sizeof(kScales[0]);
    int selectedSetting = 0;
    int colorMode = 1;
    int scaleIndex = 2; // 1.0x
    bool xWasPressed = false;
    bool yWasPressed = false;
    bool menuKeyWasDown[4] = {};
    
    float angleXY = 0.0f;
    float angleXZ = 0.0f;
    float angleXW = 0.0f;
    float angleYZ = 0.0f;
    float angleYW = 0.0f;
    float angleZW = 0.0f;

    float shaderTime = 0.0f;
    float lastFrameTime = 0.0f;
    bool isPaused = false;
    bool spaceWasPressed = false;

    void initWindow();
    void initVR();
    void processInput();
    void mainLoop();
    void cleanup();
};