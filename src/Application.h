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