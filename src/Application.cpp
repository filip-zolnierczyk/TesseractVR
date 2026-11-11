#include "Application.h"
#include <stdexcept>
#include <cmath>
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

    // Tab: odpowiednik przycisku ustawien z kontrolera (do testow w trybie desktop)
    bool tabIsPressed = (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS);
    if (tabIsPressed && !tabWasPressed) settingsOpen = !settingsOpen;
    tabWasPressed = tabIsPressed;

    // Zdarzenia jednorazowe (zbocza) wspolne dla sceny i panelu ustawien
    bool leftTriggerPressed = false;
    bool rightTriggerPressed = false;
    int scrollDir = 0;  // -1 = poprzednie ustawienie, +1 = nastepne
    int modifyDir = 0;  // -1 = poprzedni tryb, +1 = nastepny tryb
    const OpenXRManager::ControllerState* pad = nullptr;

    if (vrActive) {
        vr.syncControllers();
        pad = &vr.getControllerState();
        leftTriggerPressed = pad->leftTrigger && !leftTriggerWasPressed;
        rightTriggerPressed = pad->rightTrigger && !rightTriggerWasPressed;
        leftTriggerWasPressed = pad->leftTrigger;
        rightTriggerWasPressed = pad->rightTrigger;

        if (pad->menu && !menuWasPressed) settingsOpen = !settingsOpen;
        menuWasPressed = pad->menu;

        // Przewijanie listy ustawien przyciskami: Y (gorny) = w gore, X (dolny) = w dol
        if (pad->y && !yWasPressed) scrollDir = -1;
        if (pad->x && !xWasPressed) scrollDir = 1;
        yWasPressed = pad->y;
        xWasPressed = pad->x;
    }

    // Klawiatura w panelu ustawien: gora/dol = przewijanie, lewo/prawo = zmiana trybu
    const int menuKeys[4] = { GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT };
    bool menuKeyDown[4];
    for (int i = 0; i < 4; i++) {
        menuKeyDown[i] = glfwGetKey(window, menuKeys[i]) == GLFW_PRESS;
    }
    if (settingsOpen) {
        if (menuKeyDown[0] && !menuKeyWasDown[0]) scrollDir = -1;
        if (menuKeyDown[1] && !menuKeyWasDown[1]) scrollDir = 1;
        if (menuKeyDown[2] && !menuKeyWasDown[2]) modifyDir = -1;
        if (menuKeyDown[3] && !menuKeyWasDown[3]) modifyDir = 1;
    }
    for (int i = 0; i < 4; i++) menuKeyWasDown[i] = menuKeyDown[i];

    if (settingsOpen) {
        // Gdy panel jest otwarty, sterowanie nie rusza figury ani nie zmienia figur
        if (rightTriggerPressed) modifyDir = 1;
        if (leftTriggerPressed) modifyDir = -1;
        selectedSetting = (selectedSetting + scrollDir + kSettingCount) % kSettingCount;
        if (modifyDir != 0) {
            if (selectedSetting == 0) colorMode = (colorMode + modifyDir + kColorModeCount) % kColorModeCount;
            else scaleIndex = (scaleIndex + modifyDir + kScaleCount) % kScaleCount;
        }
    } else {
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
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) angleXZ -= rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) angleXZ += rotSpeed;

    // Oculus Touch: galki i przyciski pozostaja dostepne jako sterowanie pomocnicze.
    if (pad) {
        const auto& c = *pad;
        if (leftTriggerPressed || rightTriggerPressed) {
            currentShape = (currentShape + (rightTriggerPressed ? 1 : -1) + 7) % 7;
            currentWOffset = 0.0f;
            angleXY = 0.0f;
            angleXZ = 0.0f;
            angleXW = 0.0f;
            angleYZ = 0.0f;
            angleYW = 0.0f;
            angleZW = 0.0f;
        }

        if (c.a) angleXW += speed;
        if (c.b) angleYZ += speed;
        if (c.x) angleYW += speed;
        if (c.y) angleZW += speed;
        if (std::abs(c.leftStickX) > 0.6f) angleXY += c.leftStickX * speed;
        if (std::abs(c.leftStickY) > 0.6f) angleXZ -= c.leftStickY * speed;
        if (std::abs(c.rightStickX) > 0.6f) angleYZ += c.rightStickX * speed;
        if (std::abs(c.rightStickY) > 0.6f) currentWOffset += c.rightStickY * speed;

        constexpr float gripSensitivity = 1.5f;
        angleXY += c.leftGripDelta.z * gripSensitivity;
        angleXZ += c.leftGripDelta.y * gripSensitivity;
        angleYZ += c.leftGripDelta.x * gripSensitivity;
        angleXW += c.rightGripDelta.x * gripSensitivity;
        angleYW += c.rightGripDelta.y * gripSensitivity;
        angleZW += c.rightGripDelta.z * gripSensitivity;
    }
    }

    bool spaceIsPressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    if (spaceIsPressed && !spaceWasPressed) {
        isPaused = !isPaused;
    }
    spaceWasPressed = spaceIsPressed;

    renderer.setSettingsPanelVisible(settingsOpen);
    renderer.setRenderSettings(colorMode, kScales[scaleIndex], selectedSetting);
}

void Application::mainLoop() {
    static int frameCount = 0;
    static double fpsTimer = 0.0;
    double lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window) && !(vrActive && vr.shouldQuit())) {
        glfwPollEvents();
        processInput();

        double now = glfwGetTime();
        double delta = now - lastFrameTime;
        lastFrameTime = now;

        frameCount++;
        fpsTimer += delta;

        if (fpsTimer >= 1.0) {
            double fps = frameCount / fpsTimer;
            std::string title = "TesseractVR - FPS: " + std::to_string((int)fps);
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            fpsTimer = 0.0;
        }

        if (isPaused) {
            shaderTime += static_cast<float>(delta);
        }

        if (vrActive) {
            // TRYB OPENXR: macierze widoku/projekcji pochodza z head-trackingu headsetu (jedna klatka na oko)
            vr.pollEvents();
            vr.renderFrame([&](uint32_t eyeIndex, uint32_t imageIndex, VkExtent2D extent,
                                const glm::mat4& view, const glm::mat4& proj) {
                renderer.renderXrEye(eyeIndex, imageIndex, extent, shaderTime, currentWOffset, static_cast<float>(currentShape), view, proj,
                                      angleXY, angleXZ, angleXW, angleYZ, angleYW, angleZW);
            });
        } else {
            // KLASYCZNY TRYB DESKTOP (Fallback)
            // Stacjonarna kamera w przestrzeni 3D
            glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.5f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 10.0f);
            proj[1][1] *= -1; // Specyfika Vulkana: odwrócona oś Y!

            renderer.drawFrame(shaderTime, currentWOffset, static_cast<float>(currentShape), view, proj,
                               angleXY, angleXZ, angleXW, angleYZ, angleYW, angleZW);
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