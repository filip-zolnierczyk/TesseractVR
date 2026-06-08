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

    void initWindow();
    void processInput();
    void mainLoop();
    void cleanup();
};