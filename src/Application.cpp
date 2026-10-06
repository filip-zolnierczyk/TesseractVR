#include "Application.h"
#include <stdexcept>
#include <iostream>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

void Application::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "TesseractVR", nullptr, nullptr);
}

void Application::initVR() {
    if (!OpenXRManager::isRuntimeAvailable()) { vrActive = false; return; }
    try {
        vr.initSystem();
        xrInstanceExtensions = vr.getRequiredVulkanInstanceExtensions();
        xrDeviceExtensions = vr.getRequiredVulkanDeviceExtensions();
        // OpenXR narzuca konkretne GPU - renderer musi go uzyc zamiast wybierac wlasne
        renderer.setPhysicalDeviceSelector([this](VkInstance instance) {
            return vr.pickVulkanPhysicalDevice(instance);
        });
        vrActive = true;
    } catch (const std::exception& e) {
        std::cerr << "[VR] Headset/runtime niedostepny, startuje tryb desktop: " << e.what() << std::endl;
        vrActive = false;
    }
}

void Application::processInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    float speed = 0.01f;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        currentWOffset += speed;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        currentWOffset -= speed;
    }
    bool isShiftPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || 
                           glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
                           
    float rotSpeed = isShiftPressed ? -speed : speed;

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) angleXY += rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) angleXZ += rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) angleXW += rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) angleYZ += rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) angleYW += rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) angleZW += rotSpeed;

    // Pad (Xbox-style): A/B/X/Y powielaja klawisze 3/4/5/6
    GLFWgamepadstate padState;
    if (glfwGetGamepadState(GLFW_JOYSTICK_1, &padState)) {
        if (padState.buttons[GLFW_GAMEPAD_BUTTON_A]) angleXW += rotSpeed;
        if (padState.buttons[GLFW_GAMEPAD_BUTTON_B]) angleYZ += rotSpeed;
        if (padState.buttons[GLFW_GAMEPAD_BUTTON_X]) angleYW += rotSpeed;
        if (padState.buttons[GLFW_GAMEPAD_BUTTON_Y]) angleZW += rotSpeed;
    }

    bool spaceIsPressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    if (spaceIsPressed && !spaceWasPressed) {
        isPaused = !isPaused;
    }
    spaceWasPressed = spaceIsPressed;
}

void Application::mainLoop() {
    while (!glfwWindowShouldClose(window) && !(vrActive && vr.shouldQuit())) {
        glfwPollEvents();
        processInput();
        
        float currentFrameTime = static_cast<float>(glfwGetTime());
        currentFrameTime = 0.0f;
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        bool isPaused = true;
        if (!isPaused) {
            shaderTime += deltaTime;
        }

        if (vrActive) {
            // TRYB OPENXR: macierze widoku/projekcji pochodza z head-trackingu headsetu (jedna klatka na oko)
            vr.pollEvents();
            vr.renderFrame([&](uint32_t eyeIndex, uint32_t imageIndex, VkExtent2D extent,
                                const glm::mat4& view, const glm::mat4& proj) {
                renderer.renderXrEye(eyeIndex, imageIndex, extent, shaderTime, currentWOffset, view, proj,
                                      angleXY, angleXZ, angleXW, angleYZ, angleYW, angleZW);
            });
        } else {
            // KLASYCZNY TRYB DESKTOP (Fallback)
            // Stacjonarna kamera w przestrzeni 3D
            glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 10.0f);
            proj[1][1] *= -1; // Specyfika Vulkana: odwrócona oś Y!

            renderer.drawFrame(shaderTime, currentWOffset, view, proj, angleXY, angleXZ, angleXW, angleYZ, angleYW, angleZW);
        }
    }
    
    renderer.waitForIdle();
}

void Application::cleanup() {
    // Kolejnosc jest istotna: nasze VkImageView/VkFramebuffer wskazuja na obrazy
    // nalezace do XrSwapchain, wiec musza zostac zniszczone zanim runtime VR je zwolni.
    renderer.cleanup();
    if (vrActive) {
        vr.cleanup();
    }
    glfwDestroyWindow(window); // Potem ubijamy okno
    glfwTerminate();
}

void Application::run() {
    initWindow();
    initVR();

    renderer.init(window, xrInstanceExtensions, xrDeviceExtensions);

    if (vrActive) {
        try {
            vr.createSession(renderer.getInstance(), renderer.getPhysicalDevice(),
                              renderer.getDevice(), renderer.getGraphicsQueueFamily());

            std::vector<std::vector<VkImage>> imagesPerEye(vr.getEyeCount());
            for (uint32_t eye = 0; eye < vr.getEyeCount(); eye++) {
                imagesPerEye[eye] = vr.getSwapchainImages(eye);
            }
            renderer.initXrRenderTargets(vr.getSwapchainFormat(), imagesPerEye);
        } catch (const std::exception& e) {
            std::cerr << "[VR] Nie udalo sie uruchomic sesji VR, przelaczam na tryb desktop: " << e.what() << std::endl;
            vrActive = false;
        }
    }

    mainLoop();
    cleanup();
}