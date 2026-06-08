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
    bool enableVR = false; 

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        processInput();
        
        float time = static_cast<float>(glfwGetTime());
        
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::mat4(1.0f);

        if (!enableVR) {
            // KLASYCZNY TRYB DESKTOP (Fallback)
            // Stacjonarna kamera w przestrzeni 3D
            view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 10.0f);
            proj[1][1] *= -1; // Specyfika Vulkana: odwrócona oś Y!
        } else {
            // TRYB OPENXR
            // widokiem dla lewego/prawego oka
        }

        // Silnik staje się całkowicie agnostyczny - rysuje to, co mu podasz
        renderer.drawFrame(time, currentWOffset, view, proj);
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