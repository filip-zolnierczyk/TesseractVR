#include "Application.h"
#include <stdexcept>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

void Application::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "TesseractVR", nullptr, nullptr);
}

void Application::processInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    float speed = 0.005f;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        currentWOffset += speed;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        currentWOffset -= speed;
    }
}

void Application::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        processInput();
        
        float time = static_cast<float>(glfwGetTime());
        // Zlecamy rendererowi narysowanie klatki i narzucamy mu nasz stan!
        renderer.drawFrame(time, currentWOffset);
    }
    
    renderer.waitForIdle();
}

void Application::cleanup() {
    renderer.cleanup(); // Najpierw niszczymy Vulkana
    glfwDestroyWindow(window); // Potem ubijamy okno
    glfwTerminate();
}

void Application::run() {
    initWindow();
    renderer.init(window);
    mainLoop();
    cleanup();
}