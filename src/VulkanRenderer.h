#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <optional>
#include <string>
#include <functional>

// --- Struktury wyciągnięte z main.cpp ---
struct UniformBufferObject {
    alignas(16) glm::mat4 view;         
    alignas(16) glm::mat4 proj;         
    alignas(8)  glm::vec2 resolution;   
    alignas(4)  float time;             
    alignas(4)  float w_offset;    
    alignas(4)  float nearPlane;    // Dodane dla prawidłowej asymetrycznej projekcji
    alignas(4)  float aXY;
    alignas(4)  float aXZ;
    alignas(4)  float aXW;
    alignas(4)  float aYZ;
    alignas(4)  float aYW;
    alignas(4)  float aZW;     
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// --- Publiczny interfejs naszego silnika ---
class VulkanRenderer {
public:
    // API pozwala teraz na wstrzyknięcie rozszerzeń narzucanych przez OpenXR
    void init(GLFWwindow* window, 
              const std::vector<const char*>& instanceExtensions = {}, 
              const std::vector<const char*>& deviceExtensions = {});

    // Pozwala OpenXR narzucić konkretne GPU (musi być ustawione przed init())
    using PhysicalDeviceSelector = std::function<VkPhysicalDevice(VkInstance)>;
    void setPhysicalDeviceSelector(PhysicalDeviceSelector selector) { physicalDeviceSelector = std::move(selector); }
              
    // Silnik będzie od teraz rysował bazując na macierzach oczu z VR
    void drawFrame(float time, float wOffset, const glm::mat4& view, const glm::mat4& proj, float aXY, float aXZ, float aXW, float aYZ, float aYW, float aZW);
    void cleanup();
    void waitForIdle();

    // --- GETTERY DLA MODUŁU OPENXR ---
    VkInstance       getInstance() const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice         getDevice() const { return device; }
    VkQueue          getGraphicsQueue() const { return graphicsQueue; }
    uint32_t         getGraphicsQueueFamily() const { return graphicsQueueFamilyIndex; }

    // --- RENDEROWANIE OCZU DLA OPENXR (osobny render pass/pipeline w formacie narzuconym przez runtime) ---
    // imagesPerEye[eye] to lista obrazow VkImage nalezacych do XrSwapchain danego oka.
    void initXrRenderTargets(VkFormat colorFormat, const std::vector<std::vector<VkImage>>& imagesPerEye);
    void destroyXrRenderTargets();
    void renderXrEye(uint32_t eyeIndex, uint32_t imageIndex, VkExtent2D extent, float time, float wOffset,
                      const glm::mat4& view, const glm::mat4& proj,
                      float aXY, float aXZ, float aXW, float aYZ, float aYW, float aZW);

private:
    GLFWwindow* window;
    
    // Zapamiętane rozszerzenia narzucone przez zewnątrz
    std::vector<const char*> injectedInstanceExtensions;
    std::vector<const char*> injectedDeviceExtensions;
    uint32_t graphicsQueueFamilyIndex = 0;
    PhysicalDeviceSelector physicalDeviceSelector; // narzucone GPU (np. przez OpenXR)

    // --- UCHWYTY VULKANA ---
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VmaAllocator allocator;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // --- SWAPCHAIN I RENDER PASS ---
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;

    // --- PIPELINE I DESKRYPTORY ---
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    // --- PAMIĘĆ (UBO + VMA) ---
    VkBuffer uniformBuffer;
    VmaAllocation uniformBufferAllocation;
    void* uniformBufferMapped;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    // --- SYNCHRONIZACJA ---
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    // --- ZASOBY RENDEROWANIA OCZU DLA OPENXR ---
    VkRenderPass xrRenderPass = VK_NULL_HANDLE;
    VkPipeline xrGraphicsPipeline = VK_NULL_HANDLE;
    VkFormat xrColorFormat = VK_FORMAT_UNDEFINED;
    std::vector<std::vector<VkImageView>> xrImageViews;     // [oko][obraz]
    std::vector<std::vector<VkFramebuffer>> xrFramebuffers; // [oko][obraz]
    VkBuffer xrUniformBuffer = VK_NULL_HANDLE;
    VmaAllocation xrUniformBufferAllocation = VK_NULL_HANDLE;
    void* xrUniformBufferMapped = nullptr;
    VkDescriptorPool xrDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet xrDescriptorSet = VK_NULL_HANDLE;
    VkCommandBuffer xrCommandBuffer = VK_NULL_HANDLE;
    VkFence xrFence = VK_NULL_HANDLE;

    void createXrRenderPass();
    void createXrGraphicsPipeline();

    // --- DEKLARACJE METOD PRYWATNYCH ---
    void initVulkan();
    void initVMA();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void cleanupSwapChain();
    void recreateSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createUniformBuffer();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandPool();
    void createCommandBuffer();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, float time, float wOffset, const glm::mat4& view, const glm::mat4& proj, float aXY, float aXZ, float aXW, float aYZ, float aYW, float aZW);
    void createSyncObjects();

    // --- METODY POMOCNICZE ---
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    std::vector<const char*> getRequiredExtensions();
    bool checkValidationLayerSupport();
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
};