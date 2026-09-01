#include "OpenXRManager.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace {
template <typename T>
T getXrFunction(XrInstance instance, const char* name) {
    PFN_xrVoidFunction fn = nullptr;
    xrGetInstanceProcAddr(instance, name, &fn);
    return reinterpret_cast<T>(fn);
}
}

OpenXRManager::~OpenXRManager() {
    cleanup();
}

void OpenXRManager::initSystem() {
    const char* exts[] = {"XR_KHR_vulkan_enable"};
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    ci.enabledExtensionCount = 1;
    ci.enabledExtensionNames = exts;
    strncpy(ci.applicationInfo.applicationName, "TesseractVR", XR_MAX_APPLICATION_NAME_SIZE);
    ci.applicationInfo.applicationVersion = 1;
    strncpy(ci.applicationInfo.engineName, "TesseractEngine", XR_MAX_ENGINE_NAME_SIZE);
    ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    if (XR_FAILED(xrCreateInstance(&ci, &instance))) {
        throw std::runtime_error("Nie mozna utworzyc instancji OpenXR. Uruchom SteamVR lub Oculus runtime.");
    }

    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(instance, &sgi, &systemId))) {
        throw std::runtime_error("Nie znaleziono headsetu VR!");
    }

    queryGraphicsRequirements();

    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    viewConfigs.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, viewConfigs.data());
    views.resize(viewCount, {XR_TYPE_VIEW});
}

void OpenXRManager::queryGraphicsRequirements() {
    auto pfnReqs = getXrFunction<PFN_xrGetVulkanGraphicsRequirementsKHR>(instance, "xrGetVulkanGraphicsRequirementsKHR");
    if (!pfnReqs) {
        throw std::runtime_error("Runtime OpenXR nie wspiera Vulkana (brak xrGetVulkanGraphicsRequirementsKHR).");
    }
    XrGraphicsRequirementsVulkanKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    if (XR_FAILED(pfnReqs(instance, systemId, &req))) {
        throw std::runtime_error("xrGetVulkanGraphicsRequirementsKHR nie powiodlo sie.");
    }
}

std::vector<const char*> OpenXRManager::getRequiredVulkanInstanceExtensions() {
    auto fn = getXrFunction<PFN_xrGetVulkanInstanceExtensionsKHR>(instance, "xrGetVulkanInstanceExtensionsKHR");
    if (!fn) {
        throw std::runtime_error("Brak xrGetVulkanInstanceExtensionsKHR.");
    }
    uint32_t size = 0;
    fn(instance, systemId, 0, &size, nullptr);
    std::string buffer(size, '\0');
    fn(instance, systemId, size, &size, buffer.data());

    instanceExtStorage.clear();
    std::istringstream ss(buffer);
    std::string token;
    while (ss >> token) instanceExtStorage.push_back(token);

    std::vector<const char*> result;
    result.reserve(instanceExtStorage.size());
    for (auto& s : instanceExtStorage) result.push_back(s.c_str());
    return result;
}

std::vector<const char*> OpenXRManager::getRequiredVulkanDeviceExtensions() {
    auto fn = getXrFunction<PFN_xrGetVulkanDeviceExtensionsKHR>(instance, "xrGetVulkanDeviceExtensionsKHR");
    if (!fn) {
        throw std::runtime_error("Brak xrGetVulkanDeviceExtensionsKHR.");
    }
    uint32_t size = 0;
    fn(instance, systemId, 0, &size, nullptr);
    std::string buffer(size, '\0');
    fn(instance, systemId, size, &size, buffer.data());

    deviceExtStorage.clear();
    std::istringstream ss(buffer);
    std::string token;
    while (ss >> token) deviceExtStorage.push_back(token);

    std::vector<const char*> result;
    result.reserve(deviceExtStorage.size());
    for (auto& s : deviceExtStorage) result.push_back(s.c_str());
    return result;
}

VkPhysicalDevice OpenXRManager::pickVulkanPhysicalDevice(VkInstance vkInstance) {
    auto fn = getXrFunction<PFN_xrGetVulkanGraphicsDeviceKHR>(instance, "xrGetVulkanGraphicsDeviceKHR");
    if (!fn) {
        throw std::runtime_error("Brak xrGetVulkanGraphicsDeviceKHR.");
    }
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    if (XR_FAILED(fn(instance, systemId, vkInstance, &physicalDevice))) {
        throw std::runtime_error("xrGetVulkanGraphicsDeviceKHR nie powiodlo sie.");
    }
    return physicalDevice;
}

void OpenXRManager::createSession(VkInstance vkInstance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex) {
    XrGraphicsBindingVulkanKHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    binding.instance = vkInstance;
    binding.physicalDevice = physicalDevice;
    binding.device = device;
    binding.queueFamilyIndex = queueFamilyIndex;
    binding.queueIndex = 0;

    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = systemId;
    if (XR_FAILED(xrCreateSession(instance, &sci, &session))) {
        throw std::runtime_error("Nie udalo sie utworzyc sesji OpenXR.");
    }

    // STAGE (roomscale, poczatek na podlodze wyznaczonej przez chaperone/guardian) pozwala
    // fizycznie chodzic dookola struktury. Jesli runtime tego nie wspiera, wracamy do LOCAL
    // (przestrzen zakotwiczona w miejscu startu, bez podlogi - tylko obrot w miejscu).
    XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    rsci.poseInReferenceSpace = {{0, 0, 0, 1}, {0.0f, 1.3f, 0.0f}}; // podnosi teserakt na wysokosc oczu
    if (XR_FAILED(xrCreateReferenceSpace(session, &rsci, &appSpace))) {
        rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        rsci.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
        if (XR_FAILED(xrCreateReferenceSpace(session, &rsci, &appSpace))) {
            throw std::runtime_error("Nie udalo sie utworzyc przestrzeni referencyjnej OpenXR.");
        }
    }

    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());

    const int64_t preferred[] = {
        VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM
    };
    int64_t chosen = formats.empty() ? static_cast<int64_t>(VK_FORMAT_B8G8R8A8_SRGB) : formats[0];
    for (int64_t pref : preferred) {
        if (std::find(formats.begin(), formats.end(), pref) != formats.end()) {
            chosen = pref;
            break;
        }
    }
    swapchainFormat = static_cast<VkFormat>(chosen);

    swapchains.resize(viewConfigs.size());
    for (size_t i = 0; i < viewConfigs.size(); i++) {
        auto& vc = viewConfigs[i];
        auto& sc = swapchains[i];
        sc.width = static_cast<int32_t>(vc.recommendedImageRectWidth);
        sc.height = static_cast<int32_t>(vc.recommendedImageRectHeight);

        XrSwapchainCreateInfo swCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swCI.arraySize = 1;
        swCI.format = chosen;
        swCI.width = sc.width;
        swCI.height = sc.height;
        swCI.mipCount = 1;
        swCI.faceCount = 1;
        swCI.sampleCount = 1; // renderer nie obsluguje MSAA
        swCI.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        if (XR_FAILED(xrCreateSwapchain(session, &swCI, &sc.handle))) {
            throw std::runtime_error("Nie udalo sie utworzyc XrSwapchain dla oka " + std::to_string(i));
        }

        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(sc.handle, 0, &imageCount, nullptr);
        sc.images.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        xrEnumerateSwapchainImages(sc.handle, imageCount, &imageCount,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data()));
    }
}

VkExtent2D OpenXRManager::getEyeExtent(uint32_t eyeIndex) const {
    return { static_cast<uint32_t>(swapchains[eyeIndex].width), static_cast<uint32_t>(swapchains[eyeIndex].height) };
}

std::vector<VkImage> OpenXRManager::getSwapchainImages(uint32_t eyeIndex) const {
    std::vector<VkImage> result;
    result.reserve(swapchains[eyeIndex].images.size());
    for (auto& img : swapchains[eyeIndex].images) result.push_back(img.image);
    return result;
}

void OpenXRManager::pollEvents() {
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(instance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* e = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            sessionState = e->state;
            if (sessionState == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo sbi{XR_TYPE_SESSION_BEGIN_INFO};
                sbi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (xrBeginSession(session, &sbi) == XR_SUCCESS) {
                    sessionRunning = true;
                } else {
                    quit = true;
                }
            } else if (sessionState == XR_SESSION_STATE_STOPPING) {
                xrEndSession(session);
                sessionRunning = false;
            } else if (sessionState == XR_SESSION_STATE_EXITING || sessionState == XR_SESSION_STATE_LOSS_PENDING) {
                quit = true;
            }
        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            quit = true;
        }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

void OpenXRManager::renderFrame(const EyeRenderFn& renderEye) {
    if (!sessionRunning) return;

    XrFrameWaitInfo fwi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    if (XR_FAILED(xrWaitFrame(session, &fwi, &fs))) return;

    XrFrameBeginInfo fbi{XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(xrBeginFrame(session, &fbi))) return;

    std::vector<XrCompositionLayerProjectionView> projViews(swapchains.size());

    if (fs.shouldRender) {
        XrViewLocateInfo vli{XR_TYPE_VIEW_LOCATE_INFO};
        vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        vli.displayTime = fs.predictedDisplayTime;
        vli.space = appSpace;
        XrViewState vs{XR_TYPE_VIEW_STATE};
        uint32_t viewCount = static_cast<uint32_t>(views.size());
        xrLocateViews(session, &vli, &vs, viewCount, &viewCount, views.data());

        for (size_t i = 0; i < swapchains.size(); i++) {
            auto& sc = swapchains[i];

            uint32_t imageIndex = 0;
            XrSwapchainImageAcquireInfo acqInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrAcquireSwapchainImage(sc.handle, &acqInfo, &imageIndex);

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            xrWaitSwapchainImage(sc.handle, &waitInfo);

            glm::mat4 view = poseToViewMatrix(views[i].pose);
            glm::mat4 proj = fovToProjectionMatrix(views[i].fov, 0.1f, 100.0f);
            VkExtent2D extent = { static_cast<uint32_t>(sc.width), static_cast<uint32_t>(sc.height) };

            renderEye(static_cast<uint32_t>(i), imageIndex, extent, view, proj);

            auto& pv = projViews[i];
            pv = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            pv.pose = views[i].pose;
            pv.fov = views[i].fov;
            pv.subImage.swapchain = sc.handle;
            pv.subImage.imageRect = {{0, 0}, {sc.width, sc.height}};

            XrSwapchainImageReleaseInfo relInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(sc.handle, &relInfo);
        }
    }

    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = appSpace;
    layer.viewCount = static_cast<uint32_t>(projViews.size());
    layer.views = projViews.data();
    const XrCompositionLayerBaseHeader* layers[] = { reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer) };

    XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
    fei.displayTime = fs.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fei.layerCount = fs.shouldRender ? 1 : 0;
    fei.layers = fs.shouldRender ? layers : nullptr;
    xrEndFrame(session, &fei);
}

void OpenXRManager::cleanup() {
    for (auto& sc : swapchains) {
        if (sc.handle != XR_NULL_HANDLE) xrDestroySwapchain(sc.handle);
    }
    swapchains.clear();
    if (appSpace != XR_NULL_HANDLE) { xrDestroySpace(appSpace); appSpace = XR_NULL_HANDLE; }
    if (session != XR_NULL_HANDLE) { xrDestroySession(session); session = XR_NULL_HANDLE; }
    if (instance != XR_NULL_HANDLE) { xrDestroyInstance(instance); instance = XR_NULL_HANDLE; }
}

glm::mat4 OpenXRManager::poseToViewMatrix(const XrPosef& pose) {
    glm::quat q(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
    glm::mat4 rot = glm::mat4_cast(q);
    glm::mat4 cameraToWorld = glm::translate(glm::mat4(1.0f), glm::vec3(pose.position.x, pose.position.y, pose.position.z)) * rot;
    return glm::inverse(cameraToWorld);
}

// Standardowa asymetryczna projekcja OpenXR (Vulkan: os Y w dol, glebokosc 0..1)
glm::mat4 OpenXRManager::fovToProjectionMatrix(const XrFovf& fov, float nearZ, float farZ) {
    float l = tanf(fov.angleLeft);
    float r = tanf(fov.angleRight);
    float d = tanf(fov.angleDown);
    float u = tanf(fov.angleUp);
    float w = r - l;
    float h = d - u;

    glm::mat4 proj(0.0f);
    proj[0][0] = 2.0f / w;
    proj[1][1] = 2.0f / h;
    proj[2][0] = -(r + l) / w;
    proj[2][1] = -(u + d) / h;
    proj[2][2] = farZ / (nearZ - farZ);
    proj[2][3] = -1.0f;
    proj[3][2] = (farZ * nearZ) / (nearZ - farZ);
    return proj;
}
