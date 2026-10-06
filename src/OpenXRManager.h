#pragma once

// XR_USE_GRAPHICS_API_VULKAN jest ustawiane globalnie w CMakeLists.txt
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <functional>

// Cienka warstwa integrujaca OpenXR z VulkanRenderer:
// head-tracking + stereo, jedno oko = jeden XrSwapchain renderowany przez VulkanRenderer.
class OpenXRManager {
public:
    ~OpenXRManager();
    static bool isRuntimeAvailable();
    bool isSessionRunning() const { return sessionRunning; }
    bool shouldQuit() const { return quit; }

    // Krok 1: instancja OpenXR + system (headset). Rzuca std::runtime_error jesli brak runtime/headsetu.
    void initSystem();

    // Krok 2: rozszerzenia Vulkana wymagane przez runtime (wywolac przed VulkanRenderer::init)
    std::vector<const char*> getRequiredVulkanInstanceExtensions();
    std::vector<const char*> getRequiredVulkanDeviceExtensions();

    // Krok 3: GPU wskazane przez runtime (wywolac po utworzeniu VkInstance)
    VkPhysicalDevice pickVulkanPhysicalDevice(VkInstance instance);

    // Krok 4: sesja + swapchainy per-oko (wywolac po VulkanRenderer::init)
    void createSession(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex);

    VkFormat getSwapchainFormat() const { return swapchainFormat; }
    uint32_t getEyeCount() const { return static_cast<uint32_t>(swapchains.size()); }
    VkExtent2D getEyeExtent(uint32_t eyeIndex) const;
    std::vector<VkImage> getSwapchainImages(uint32_t eyeIndex) const;

    void pollEvents();

    // Wywolywane raz na oko z gotowymi macierzami widoku/projekcji z headsetu.
    using EyeRenderFn = std::function<void(uint32_t eyeIndex, uint32_t imageIndex, VkExtent2D extent,
                                            const glm::mat4& view, const glm::mat4& proj)>;
    void renderFrame(const EyeRenderFn& renderEye);

    void cleanup();

private:
    struct SwapchainInfo {
        XrSwapchain handle = XR_NULL_HANDLE;
        int32_t width = 0;
        int32_t height = 0;
        std::vector<XrSwapchainImageVulkanKHR> images;
    };

    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace appSpace = XR_NULL_HANDLE;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning = false;
    bool quit = false;

    std::vector<XrViewConfigurationView> viewConfigs;
    std::vector<XrView> views;
    std::vector<SwapchainInfo> swapchains;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;

    std::vector<std::string> instanceExtStorage;
    std::vector<std::string> deviceExtStorage;

    void queryGraphicsRequirements();
    static glm::mat4 poseToViewMatrix(const XrPosef& pose);
    static glm::mat4 fovToProjectionMatrix(const XrFovf& fov, float nearZ, float farZ);
};
