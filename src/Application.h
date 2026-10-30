#pragma once

#include "VulkanRenderer.h"
#include "OpenXRManager.h"
#ifndef TESSERACT_ANDROID
#include <GLFW/glfw3.h>
#else
struct android_app;
#endif

class Application {
public:
    void run();
#ifdef TESSERACT_ANDROID
    void runAndroid(android_app* app);
#endif

private:
#ifndef TESSERACT_ANDROID
    GLFWwindow* window;
#endif
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
#ifndef TESSERACT_ANDROID
    void processInput();
#endif
    void mainLoop();
    void cleanup();
};