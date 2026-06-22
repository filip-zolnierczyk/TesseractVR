#pragma once

#include "VulkanRenderer.h"
#include <GLFW/glfw3.h>

class Application {
public:
    void run();

private:
    GLFWwindow* window;
    VulkanRenderer renderer;
    
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
    void processInput();
    void mainLoop();
    void cleanup();
};