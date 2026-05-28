/**
 * VR Scene 3D - Vulkan + OpenXR
 *
 * Środowisko:
 *   - Podłoga jako siatka (grid 20x20, co 1m)
 *   - 5 kolorowych sześcianów rozstawionych w scenie
 *   - Head tracking z headsetu (macierze z xrLocateViews)
 *   - Oświetlenie: ambient + directional (Lambert)
 *
 * Wymagania:
 *   - Vulkan SDK (glslangValidator.exe w PATH lub w C:/VulkanSDK/...)
 *   - OpenXR runtime (SteamVR / Oculus)
 *   - Kompilator C++17
 *
 * Kompilacja:
 *   cmake -B build && cmake --build build --config Release
 */

#define XR_USE_GRAPHICS_API_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VC_EXTRA_LEAN

#include <windows.h>
#include <unknwn.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <array>
#include <chrono>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────
//  Makra
// ─────────────────────────────────────────────────────────────────────────────
#define XR_CHECK(expr) do {                                                    \
    XrResult _r = (expr);                                                      \
    if (XR_FAILED(_r)) {                                                       \
        char _b[XR_MAX_RESULT_STRING_SIZE];                                    \
        xrResultToString(m_instance, _r, _b);                                  \
        throw std::runtime_error(std::string("OpenXR [") + #expr + "]: " + _b);\
    } } while(0)

#define VK_CHECK(expr) do {                                                    \
    VkResult _r = (expr);                                                      \
    if (_r != VK_SUCCESS)                                                      \
        throw std::runtime_error(std::string("Vulkan [") + #expr + "]: "      \
            + std::to_string((int)_r));                                        \
    } while(0)

// ─────────────────────────────────────────────────────────────────────────────
//  Prosta matematyka 3D (bez zewnętrznych zależności)
// ─────────────────────────────────────────────────────────────────────────────
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

// Kolumna-major 4x4 (zgodnie z konwencją Vulkan/GLSL)
struct Mat4 {
    float m[4][4] = {};  // m[col][row]

    static Mat4 identity() {
        Mat4 r; r.m[0][0]=r.m[1][1]=r.m[2][2]=r.m[3][3]=1.f; return r;
    }

    static Mat4 translation(float x, float y, float z) {
        Mat4 r = identity();
        r.m[3][0]=x; r.m[3][1]=y; r.m[3][2]=z; return r;
    }

    static Mat4 scale(float s) {
        Mat4 r = identity();
        r.m[0][0]=r.m[1][1]=r.m[2][2]=s; return r;
    }

    static Mat4 rotY(float a) {
        Mat4 r = identity();
        r.m[0][0]= cosf(a); r.m[2][0]= sinf(a);
        r.m[0][2]=-sinf(a); r.m[2][2]= cosf(a);
        return r;
    }

    // Kolumnowo-majorowe mnożenie
    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c=0;c<4;c++)
            for (int row=0;row<4;row++) {
                float s=0;
                for (int k=0;k<4;k++) s += m[k][row]*b.m[c][k];
                r.m[c][row]=s;
            }
        return r;
    }

    // Odwrotność dla macierzy widoku (rigid body: tylko rotacja+translacja)
    Mat4 rigidInverse() const {
        // transpozycja rotacji + nowa translacja
        Mat4 r = identity();
        for(int i=0;i<3;i++) for(int j=0;j<3;j++) r.m[i][j]=m[j][i];
        // -R^T * t
        for(int i=0;i<3;i++) {
            r.m[3][i] = -(m[3][0]*m[i][0] + m[3][1]*m[i][1] + m[3][2]*m[i][2]);
        }
        return r;
    }
};

// Konwersja quaternion (XrQuaternionf) → Mat4
static Mat4 quatToMat(float qx, float qy, float qz, float qw) {
    Mat4 r = Mat4::identity();
    float x=qx,y=qy,z=qz,w=qw;
    r.m[0][0]=1-2*(y*y+z*z); r.m[0][1]=2*(x*y+z*w); r.m[0][2]=2*(x*z-y*w);
    r.m[1][0]=2*(x*y-z*w);   r.m[1][1]=1-2*(x*x+z*z); r.m[1][2]=2*(y*z+x*w);
    r.m[2][0]=2*(x*z+y*w);   r.m[2][1]=2*(y*z-x*w); r.m[2][2]=1-2*(x*x+y*y);
    return r;
}

// Macierz projekcji z OpenXR FovF (Vulkan: Y odwrócone, głębokość 0..1)
static Mat4 xrFovToProjection(const XrFovf& fov, float nearZ=0.05f, float farZ=100.f) {
    float l = tanf(fov.angleLeft);
    float r = tanf(fov.angleRight);
    float d = tanf(fov.angleDown);
    float u = tanf(fov.angleUp);
    float w = r - l;
    float h = d - u;  // uwaga: d > u dla Vulkan (Y w dół)
    Mat4 m{};
    m.m[0][0] =  2.f / w;
    m.m[1][1] =  2.f / h;
    m.m[2][0] = -(r + l) / w;
    m.m[2][1] = -(u + d) / h;
    m.m[2][2] =  farZ / (nearZ - farZ);
    m.m[2][3] = -1.f;
    m.m[3][2] =  (farZ * nearZ) / (nearZ - farZ);
    return m;
}

// Macierz widoku z XrPosef
static Mat4 xrPoseToView(const XrPosef& pose) {
    // Konwertuj pozę headsetu na macierz "camera → world"
    Mat4 rot = quatToMat(pose.orientation.x, pose.orientation.y,
                         pose.orientation.z, pose.orientation.w);
    rot.m[3][0] = pose.position.x;
    rot.m[3][1] = pose.position.y;
    rot.m[3][2] = pose.position.z;
    // Widok = odwrotność macierzy kamery
    return rot.rigidInverse();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Vertex (pozycja + normal + kolor)
// ─────────────────────────────────────────────────────────────────────────────
struct Vertex {
    float pos[3];
    float normal[3];
    float color[3];
};

// ─────────────────────────────────────────────────────────────────────────────
//  Push constant wysyłany do shadera (MVP + dane oświetlenia)
// ─────────────────────────────────────────────────────────────────────────────
struct PushConst {
    Mat4  mvp;          // 64 bajty
    Mat4  model;        // 64 bajty (do normalnych w przestrzeni świata)
    float lightDir[4];  // 16 bajtów (xyz=kierunek, w=padding)
    float ambientCol[4];// 16 bajtów
    float diffuseCol[4];// 16 bajtów
};
// Razem: 176 bajtów — mieści się w 256B limicie Vulkan

// ─────────────────────────────────────────────────────────────────────────────
//  Generowanie geometrii
// ─────────────────────────────────────────────────────────────────────────────

// Sześcian: 6 ścian × 2 trójkąty × 3 wierzchołki
static std::vector<Vertex> makeCube(float r, float g, float b) {
    // Każda ściana: normal + 2 trójkąty (6 wierzchołków)
    const float s = 0.5f;
    struct Face { float nx,ny,nz; float verts[4][3]; };
    static const Face faces[] = {
        // +X
        { 1,0,0, {{ s,-s,-s},{ s,-s, s},{ s, s, s},{ s, s,-s}} },
        // -X
        {-1,0,0, {{-s,-s, s},{-s,-s,-s},{-s, s,-s},{-s, s, s}} },
        // +Y
        { 0,1,0, {{-s, s, s},{ s, s, s},{ s, s,-s},{-s, s,-s}} },
        // -Y
        { 0,-1,0,{{-s,-s,-s},{ s,-s,-s},{ s,-s, s},{-s,-s, s}} },
        // +Z
        { 0,0,1, {{-s,-s, s},{ s,-s, s},{ s, s, s},{-s, s, s}} },
        // -Z
        { 0,0,-1,{{ s,-s,-s},{-s,-s,-s},{-s, s,-s},{ s, s,-s}} },
    };
    std::vector<Vertex> verts;
    for (auto& f : faces) {
        // Quad → 2 trójkąty (0,1,2) i (0,2,3)
        int idx[] = {0,1,2, 0,2,3};
        for (int i : idx)
            verts.push_back({{f.verts[i][0],f.verts[i][1],f.verts[i][2]},
                             {f.nx,f.ny,f.nz},
                             {r,g,b}});
    }
    return verts; // 36 wierzchołków
}

// Siatka podłogi: linie co 1m, wymiar gridSize × gridSize
// Linie renderujemy jako bardzo cienkie quady (dwa trójkąty na linię)
// Prostsze: użyjemy osobnego pipeline LINE_LIST
static std::vector<Vertex> makeGrid(int halfSize, float y) {
    // Zwraca wierzchołki do rysowania jako GL_LINES (VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
    std::vector<Vertex> v;
    float gray[] = {0.3f, 0.3f, 0.35f};
    float n[]    = {0.f, 1.f, 0.f};
    for (int i = -halfSize; i <= halfSize; i++) {
        float fi = (float)i;
        float fh = (float)halfSize;
        // Linia wzdłuż Z
        v.push_back({{ fi, y, -fh }, {n[0],n[1],n[2]}, {gray[0],gray[1],gray[2]}});
        v.push_back({{ fi, y,  fh }, {n[0],n[1],n[2]}, {gray[0],gray[1],gray[2]}});
        // Linia wzdłuż X
        v.push_back({{-fh, y,  fi }, {n[0],n[1],n[2]}, {gray[0],gray[1],gray[2]}});
        v.push_back({{ fh, y,  fi }, {n[0],n[1],n[2]}, {gray[0],gray[1],gray[2]}});
    }
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shadery GLSL (kompilowane w runtime przez glslangValidator)
// ─────────────────────────────────────────────────────────────────────────────
static const char* VERT_GLSL = R"GLSL(
#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PC {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
    vec4 ambientCol;
    vec4 diffuseCol;
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    // Normal w przestrzeni świata (uproszczenie: brak skalowania niejednorodnego)
    fragNormal = mat3(pc.model) * inNormal;
    fragColor  = inColor;
}
)GLSL";

static const char* FRAG_GLSL = R"GLSL(
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

layout(push_constant) uniform PC {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;    // xyz = kierunek światła (znormalizowany, odwrócony)
    vec4 ambientCol;  // xyz = kolor ambient
    vec4 diffuseCol;  // xyz = kolor diffuse
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(pc.lightDir.xyz);

    float diff = max(dot(N, L), 0.0);

    vec3 ambient  = pc.ambientCol.xyz * fragColor;
    vec3 diffuse  = diff * pc.diffuseCol.xyz * fragColor;

    outColor = vec4(ambient + diffuse, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
//  Kompilacja shaderów
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<uint32_t> loadSpv(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t sz = (size_t)f.tellg(); f.seekg(0);
    std::vector<uint32_t> d(sz/4);
    f.read(reinterpret_cast<char*>(d.data()), sz);
    return d;
}

static std::vector<uint32_t> compileGlsl(const std::string& src,
                                          const std::string& stage,
                                          const std::string& tag)
{
    std::string srcPath = tag + "." + stage;
    std::string spvPath = tag + "_" + stage + ".spv";

    { std::ofstream f(srcPath); if (!f) return {}; f << src; }

    // Szukaj glslangValidator w Vulkan SDK
    const char* compilers[] = {
        "glslangValidator",
        "C:/VulkanSDK/1.3.280.0/Bin/glslangValidator.exe",
        "C:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe",
        "C:/VulkanSDK/1.3.250.0/Bin/glslangValidator.exe",
        "C:/VulkanSDK/1.2.189.0/Bin/glslangValidator.exe",
        nullptr
    };

    bool ok = false;
    for (int i = 0; compilers[i] && !ok; i++) {
        std::string cmd = std::string(compilers[i])
            + " -V --target-env vulkan1.0 -S " + stage
            + " -o " + spvPath + " " + srcPath + " 2>nul";
        ok = (system(cmd.c_str()) == 0);
    }
    if (!ok) {
        std::string cmd = "glslc -fshader-stage=" + stage
            + " -o " + spvPath + " " + srcPath + " 2>nul";
        ok = (system(cmd.c_str()) == 0);
    }

    auto data = loadSpv(spvPath);
    DeleteFileA(srcPath.c_str());
    DeleteFileA(spvPath.c_str());

    if (!ok || data.empty())
        throw std::runtime_error(
            "Nie mozna skompilowac shadera '" + stage + "'!\n"
            "Zainstaluj Vulkan SDK i dodaj glslangValidator.exe do PATH.\n"
            "  https://vulkan.lunarg.com/sdk/home");
    return data;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bufor Vulkan (vertex buffer, index buffer itd.)
// ─────────────────────────────────────────────────────────────────────────────
struct GpuBuffer {
    VkBuffer       buf    = VK_NULL_HANDLE;
    VkDeviceMemory mem    = VK_NULL_HANDLE;
    VkDeviceSize   size   = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Obiekt sceny (jeden draw call)
// ─────────────────────────────────────────────────────────────────────────────
struct SceneObject {
    GpuBuffer  vbo;
    uint32_t   vertCount  = 0;
    Mat4       modelMatrix;
    bool       isLines    = false; // grid rysujemy jako linie
};

// ─────────────────────────────────────────────────────────────────────────────
//  Główna klasa aplikacji
// ─────────────────────────────────────────────────────────────────────────────
class VRScene {
public:
    void run() {
        std::cout << "=== VR Scene 3D (Vulkan + OpenXR) ===\n";
        initOpenXR();
        callGraphicsReqs();
        initVulkan();
        createSession();
        createSwapchains();
        createRenderPass();
        createPipelines();    // dwa pipeline: solid + lines
        createFramebuffers();
        buildScene();
        mainLoop();
        cleanup();
    }

private:
    // ── OpenXR ──────────────────────────────────────────────────────────────
    XrInstance       m_instance     = XR_NULL_HANDLE;
    XrSystemId       m_systemId     = XR_NULL_SYSTEM_ID;
    XrSession        m_session      = XR_NULL_HANDLE;
    XrSpace          m_appSpace     = XR_NULL_HANDLE;
    XrSessionState   m_sessionState = XR_SESSION_STATE_UNKNOWN;
    bool             m_sessionRunning = false;
    bool             m_quit           = false;

    std::vector<XrViewConfigurationView> m_viewConfigs;
    std::vector<XrView>                  m_views;

    struct Swapchain {
        XrSwapchain                            handle = XR_NULL_HANDLE;
        int32_t                                width  = 0;
        int32_t                                height = 0;
        std::vector<XrSwapchainImageVulkanKHR> images;
    };
    std::vector<Swapchain> m_swapchains;

    // ── Vulkan ───────────────────────────────────────────────────────────────
    VkInstance       m_vkInstance  = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDevice  = VK_NULL_HANDLE;
    VkDevice         m_device      = VK_NULL_HANDLE;
    VkQueue          m_queue       = VK_NULL_HANDLE;
    uint32_t         m_queueFamily = 0;
    VkCommandPool    m_cmdPool     = VK_NULL_HANDLE;
    VkFormat         m_swapFmt     = VK_FORMAT_R8G8B8A8_SRGB;

    // Depth buffer (jeden, współdzielony – rozmiar max oka)
    VkImage        m_depthImage  = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMem    = VK_NULL_HANDLE;
    VkImageView    m_depthView   = VK_NULL_HANDLE;
    VkFormat       m_depthFmt    = VK_FORMAT_D32_SFLOAT;

    VkRenderPass     m_renderPass   = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeLayout   = VK_NULL_HANDLE;
    VkPipeline       m_pipeSolid    = VK_NULL_HANDLE; // sześciany
    VkPipeline       m_pipeLines    = VK_NULL_HANDLE; // siatka

    struct SwapFB { VkImageView view=VK_NULL_HANDLE; VkFramebuffer fb=VK_NULL_HANDLE; };
    std::vector<std::vector<SwapFB>> m_swapFBs;

    std::vector<std::string> m_vkInstanceExts;
    std::vector<std::string> m_vkDeviceExts;

    // ── Scena ────────────────────────────────────────────────────────────────
    std::vector<SceneObject> m_objects;

    // ─────────────────────────────────────────────────────────────────────────
    void initOpenXR() {
        std::cout << "[OpenXR] Inicjalizacja...\n";
        const char* exts[] = {"XR_KHR_vulkan_enable"};
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        ci.enabledExtensionCount = 1;
        ci.enabledExtensionNames = exts;
        strncpy(ci.applicationInfo.applicationName, "VRScene3D", XR_MAX_APPLICATION_NAME_SIZE);
        ci.applicationInfo.applicationVersion = 1;
        strncpy(ci.applicationInfo.engineName,  "CustomEngine", XR_MAX_ENGINE_NAME_SIZE);
        ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        if (XR_FAILED(xrCreateInstance(&ci, &m_instance)))
            throw std::runtime_error("Nie mozna utworzyc instancji OpenXR. "
                "Uruchom SteamVR lub Oculus runtime.");

        XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
        sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        if (XR_FAILED(xrGetSystem(m_instance, &sgi, &m_systemId)))
            throw std::runtime_error("Nie znaleziono headsetu VR!");

        XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};
        xrGetSystemProperties(m_instance, m_systemId, &props);
        std::cout << "[OpenXR] System: " << props.systemName << "\n";

        uint32_t vc=0;
        xrEnumerateViewConfigurationViews(m_instance, m_systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &vc, nullptr);
        m_viewConfigs.resize(vc, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        xrEnumerateViewConfigurationViews(m_instance, m_systemId,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, vc, &vc, m_viewConfigs.data());
        m_views.resize(vc, {XR_TYPE_VIEW});
        std::cout << "[OpenXR] Widoki: " << vc << "\n";
    }

    void callGraphicsReqs() {
        auto get = [&](const char* name) -> PFN_xrVoidFunction {
            PFN_xrVoidFunction fn=nullptr;
            xrGetInstanceProcAddr(m_instance, name, &fn);
            return fn;
        };

        auto pfnReqs = (PFN_xrGetVulkanGraphicsRequirementsKHR)get(
            "xrGetVulkanGraphicsRequirementsKHR");
        if (!pfnReqs) throw std::runtime_error("xrGetVulkanGraphicsRequirementsKHR brak!");
        XrGraphicsRequirementsVulkanKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        XR_CHECK(pfnReqs(m_instance, m_systemId, &req));

        auto loadExts = [&](const char* fname, std::vector<std::string>& out) {
            auto fn = (PFN_xrGetVulkanInstanceExtensionsKHR)get(fname);
            if (!fn) return;
            uint32_t sz=0; fn(m_instance,m_systemId,0,&sz,nullptr);
            std::string buf(sz,'\0'); fn(m_instance,m_systemId,sz,&sz,buf.data());
            std::istringstream ss(buf); std::string e;
            while(ss>>e) out.push_back(e);
        };
        loadExts("xrGetVulkanInstanceExtensionsKHR", m_vkInstanceExts);
        loadExts("xrGetVulkanDeviceExtensionsKHR",   m_vkDeviceExts);
        std::cout << "[OpenXR] Graphics req OK.\n";
    }

    void initVulkan() {
        std::cout << "[Vulkan] Inicjalizacja...\n";
        VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        ai.pApplicationName = "VRScene3D";
        ai.apiVersion       = VK_API_VERSION_1_0;

        std::vector<const char*> ie; for(auto& e:m_vkInstanceExts) ie.push_back(e.c_str());
        VkInstanceCreateInfo vkCI{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        vkCI.pApplicationInfo=&ai;
        vkCI.enabledExtensionCount=(uint32_t)ie.size();
        vkCI.ppEnabledExtensionNames=ie.data();
        VK_CHECK(vkCreateInstance(&vkCI,nullptr,&m_vkInstance));

        // Pobierz GPU wskazany przez OpenXR
        auto pfn=(PFN_xrGetVulkanGraphicsDeviceKHR)nullptr;
        xrGetInstanceProcAddr(m_instance,"xrGetVulkanGraphicsDeviceKHR",(PFN_xrVoidFunction*)&pfn);
        if (pfn) pfn(m_instance,m_systemId,m_vkInstance,&m_physDevice);
        if (m_physDevice==VK_NULL_HANDLE) {
            uint32_t n=0; vkEnumeratePhysicalDevices(m_vkInstance,&n,nullptr);
            if (!n) throw std::runtime_error("Brak GPU Vulkan!");
            std::vector<VkPhysicalDevice> devs(n);
            vkEnumeratePhysicalDevices(m_vkInstance,&n,devs.data());
            m_physDevice=devs[0];
        }

        VkPhysicalDeviceProperties pdp{};
        vkGetPhysicalDeviceProperties(m_physDevice,&pdp);
        std::cout << "[Vulkan] GPU: " << pdp.deviceName << "\n";

        // Queue family
        uint32_t qc=0; vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice,&qc,nullptr);
        std::vector<VkQueueFamilyProperties> qp(qc);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice,&qc,qp.data());
        m_queueFamily=UINT32_MAX;
        for(uint32_t i=0;i<qc;i++) if(qp[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){m_queueFamily=i;break;}
        if(m_queueFamily==UINT32_MAX) throw std::runtime_error("Brak graphics queue!");

        float qpri=1.f;
        VkDeviceQueueCreateInfo qCI{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qCI.queueFamilyIndex=m_queueFamily; qCI.queueCount=1; qCI.pQueuePriorities=&qpri;

        std::vector<const char*> de; for(auto& e:m_vkDeviceExts) de.push_back(e.c_str());
        VkDeviceCreateInfo devCI{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        devCI.queueCreateInfoCount=1; devCI.pQueueCreateInfos=&qCI;
        devCI.enabledExtensionCount=(uint32_t)de.size(); devCI.ppEnabledExtensionNames=de.data();

        // Włącz wide lines (dla siatki – jeśli GPU to wspiera)
        VkPhysicalDeviceFeatures feats{};
        VkPhysicalDeviceFeatures supported{};
        vkGetPhysicalDeviceFeatures(m_physDevice, &supported);
        if (supported.wideLines) feats.wideLines = VK_TRUE;
        if (supported.fillModeNonSolid) feats.fillModeNonSolid = VK_TRUE;
        devCI.pEnabledFeatures = &feats;

        VK_CHECK(vkCreateDevice(m_physDevice,&devCI,nullptr,&m_device));
        vkGetDeviceQueue(m_device,m_queueFamily,0,&m_queue);

        VkCommandPoolCreateInfo cpCI{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpCI.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpCI.queueFamilyIndex=m_queueFamily;
        VK_CHECK(vkCreateCommandPool(m_device,&cpCI,nullptr,&m_cmdPool));
        std::cout << "[Vulkan] Device OK.\n";
    }

    void createSession() {
        XrGraphicsBindingVulkanKHR b{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
        b.instance=m_vkInstance; b.physicalDevice=m_physDevice;
        b.device=m_device; b.queueFamilyIndex=m_queueFamily; b.queueIndex=0;
        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
        sci.next=&b; sci.systemId=m_systemId;
        XR_CHECK(xrCreateSession(m_instance,&sci,&m_session));

        XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        rsci.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
        rsci.poseInReferenceSpace={{0,0,0,1},{0,0,0}};
        XR_CHECK(xrCreateReferenceSpace(m_session,&rsci,&m_appSpace));
        std::cout << "[OpenXR] Sesja OK.\n";
    }

    void createSwapchains() {
        uint32_t fc=0; xrEnumerateSwapchainFormats(m_session,0,&fc,nullptr);
        std::vector<int64_t> fmts(fc);
        xrEnumerateSwapchainFormats(m_session,fc,&fc,fmts.data());

        const int64_t pref[]={
            VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM};
        int64_t chosen=fmts[0];
        for(auto p:pref) for(auto f:fmts) if(f==p){chosen=f;goto done;}
        done:
        m_swapFmt=(VkFormat)chosen;
        std::cout << "[OpenXR] Swapchain format: " << chosen << "\n";

        m_swapchains.resize(m_viewConfigs.size());
        m_swapFBs.resize(m_viewConfigs.size());

        for(size_t i=0;i<m_viewConfigs.size();i++){
            auto& vc=m_viewConfigs[i]; auto& sc=m_swapchains[i];
            sc.width=vc.recommendedImageRectWidth;
            sc.height=vc.recommendedImageRectHeight;

            XrSwapchainCreateInfo swCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            swCI.arraySize=1; swCI.format=chosen;
            swCI.width=sc.width; swCI.height=sc.height;
            swCI.mipCount=1; swCI.faceCount=1;
            swCI.sampleCount=vc.recommendedSwapchainSampleCount;
            swCI.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            XR_CHECK(xrCreateSwapchain(m_session,&swCI,&sc.handle));

            uint32_t ic=0; xrEnumerateSwapchainImages(sc.handle,0,&ic,nullptr);
            sc.images.resize(ic,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
            xrEnumerateSwapchainImages(sc.handle,ic,&ic,
                (XrSwapchainImageBaseHeader*)sc.images.data());
            std::cout << "  Oko " << i << ": " << sc.width << "x" << sc.height
                      << " × " << ic << " obrazy\n";
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Depth buffer (jeden dla obu oczu – max rozdzielczość)
    // ─────────────────────────────────────────────────────────────────────────
    uint32_t findMemType(uint32_t filter, VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(m_physDevice,&mp);
        for(uint32_t i=0;i<mp.memoryTypeCount;i++)
            if((filter&(1u<<i)) && (mp.memoryTypes[i].propertyFlags&props)==props)
                return i;
        throw std::runtime_error("Nie znaleziono odpowiedniego typu pamieci!");
    }

    void createDepthBuffer(uint32_t w, uint32_t h) {
        VkImageCreateInfo iCI{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        iCI.imageType   = VK_IMAGE_TYPE_2D;
        iCI.format      = m_depthFmt;
        iCI.extent      = {w, h, 1};
        iCI.mipLevels   = 1; iCI.arrayLayers = 1;
        iCI.samples     = VK_SAMPLE_COUNT_1_BIT;
        iCI.tiling      = VK_IMAGE_TILING_OPTIMAL;
        iCI.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        VK_CHECK(vkCreateImage(m_device,&iCI,nullptr,&m_depthImage));

        VkMemoryRequirements mr{};
        vkGetImageMemoryRequirements(m_device,m_depthImage,&mr);
        VkMemoryAllocateInfo maI{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        maI.allocationSize=mr.size;
        maI.memoryTypeIndex=findMemType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(m_device,&maI,nullptr,&m_depthMem));
        VK_CHECK(vkBindImageMemory(m_device,m_depthImage,m_depthMem,0));

        VkImageViewCreateInfo ivCI{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivCI.image    = m_depthImage;
        ivCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivCI.format   = m_depthFmt;
        ivCI.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
        VK_CHECK(vkCreateImageView(m_device,&ivCI,nullptr,&m_depthView));
    }

    // ─────────────────────────────────────────────────────────────────────────
    void createRenderPass() {
        // Kolor
        VkAttachmentDescription color{};
        color.format        = m_swapFmt;
        color.samples       = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Głębokość
        VkAttachmentDescription depth{};
        depth.format        = m_depthFmt;
        depth.samples       = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp= VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription sub{};
        sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount    = 1;
        sub.pColorAttachments       = &colorRef;
        sub.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass   = VK_SUBPASS_EXTERNAL; dep.dstSubpass   = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                         | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                         | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask= 0;
        dep.dstAccessMask= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                         | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkAttachmentDescription atts[] = {color, depth};
        VkRenderPassCreateInfo rpCI{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpCI.attachmentCount=2; rpCI.pAttachments=atts;
        rpCI.subpassCount=1;    rpCI.pSubpasses=&sub;
        rpCI.dependencyCount=1; rpCI.pDependencies=&dep;
        VK_CHECK(vkCreateRenderPass(m_device,&rpCI,nullptr,&m_renderPass));
        std::cout << "[Vulkan] Render pass OK.\n";
    }

    // ─────────────────────────────────────────────────────────────────────────
    VkShaderModule makeModule(const std::vector<uint32_t>& spv) {
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize=spv.size()*4; ci.pCode=spv.data();
        VkShaderModule m; VK_CHECK(vkCreateShaderModule(m_device,&ci,nullptr,&m));
        return m;
    }

    VkPipeline buildPipeline(VkShaderModule vert, VkShaderModule frag,
                              VkPrimitiveTopology topo, float lineWidth=1.f)
    {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module=vert; stages[0].pName="main";
        stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=frag; stages[1].pName="main";

        // Vertex input: pos(3f) + normal(3f) + color(3f) = 36 bajtów
        VkVertexInputBindingDescription binding{};
        binding.binding=0; binding.stride=sizeof(Vertex);
        binding.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrs[3]{};
        attrs[0]={0,0,VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex,pos)};
        attrs[1]={1,0,VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex,normal)};
        attrs[2]={2,0,VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex,color)};

        VkPipelineVertexInputStateCreateInfo viCI{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        viCI.vertexBindingDescriptionCount=1;   viCI.pVertexBindingDescriptions=&binding;
        viCI.vertexAttributeDescriptionCount=3; viCI.pVertexAttributeDescriptions=attrs;

        VkPipelineInputAssemblyStateCreateInfo iaCI{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        iaCI.topology=topo;

        VkViewport vp{0,0,(float)m_swapchains[0].width,(float)m_swapchains[0].height,0,1};
        VkRect2D sc{{0,0},{(uint32_t)m_swapchains[0].width,(uint32_t)m_swapchains[0].height}};
        VkPipelineViewportStateCreateInfo vpCI{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vpCI.viewportCount=1; vpCI.pViewports=&vp;
        vpCI.scissorCount=1;  vpCI.pScissors=&sc;

        VkPipelineRasterizationStateCreateInfo rsCI{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rsCI.polygonMode=VK_POLYGON_MODE_FILL;
        rsCI.cullMode = VK_CULL_MODE_NONE;
        rsCI.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsCI.lineWidth=lineWidth;

        VkPipelineMultisampleStateCreateInfo msCI{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        msCI.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo dsCI{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dsCI.depthTestEnable=VK_TRUE;
        dsCI.depthWriteEnable=VK_TRUE;
        dsCI.depthCompareOp=VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState cbAtt{};
        cbAtt.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
                             VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cbCI{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbCI.attachmentCount=1; cbCI.pAttachments=&cbAtt;

        VkGraphicsPipelineCreateInfo gpCI{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        gpCI.stageCount=2;          gpCI.pStages=stages;
        gpCI.pVertexInputState=&viCI;
        gpCI.pInputAssemblyState=&iaCI;
        gpCI.pViewportState=&vpCI;
        gpCI.pRasterizationState=&rsCI;
        gpCI.pMultisampleState=&msCI;
        gpCI.pDepthStencilState=&dsCI;
        gpCI.pColorBlendState=&cbCI;
        gpCI.layout=m_pipeLayout;
        gpCI.renderPass=m_renderPass;

        VkPipeline pipe;
        VK_CHECK(vkCreateGraphicsPipelines(m_device,VK_NULL_HANDLE,1,&gpCI,nullptr,&pipe));
        return pipe;
    }

    void createPipelines() {
        std::cout << "[Vulkan] Kompilacja shaderow GLSL...\n";
        auto vertSpv = compileGlsl(VERT_GLSL, "vert", "vr3d_tmp");
        auto fragSpv = compileGlsl(FRAG_GLSL, "frag", "vr3d_tmp");
        std::cout << "[Vulkan] Shadery skompilowane.\n";

        // Push constant layout
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcRange.offset     = 0;
        pcRange.size       = sizeof(PushConst);

        VkPipelineLayoutCreateInfo plCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plCI.pushConstantRangeCount=1; plCI.pPushConstantRanges=&pcRange;
        VK_CHECK(vkCreatePipelineLayout(m_device,&plCI,nullptr,&m_pipeLayout));

        VkShaderModule vm = makeModule(vertSpv);
        VkShaderModule fm = makeModule(fragSpv);

        m_pipeSolid = buildPipeline(vm, fm, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1.f);
        m_pipeLines = buildPipeline(vm, fm, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,     1.f);

        vkDestroyShaderModule(m_device, vm, nullptr);
        vkDestroyShaderModule(m_device, fm, nullptr);
        std::cout << "[Vulkan] Pipeline solid + lines OK.\n";
    }

    void createFramebuffers() {
        // Depth buffer na max rozdzielczość
        uint32_t maxW=0, maxH=0;
        for(auto& sc:m_swapchains){
            maxW=std::max(maxW,(uint32_t)sc.width);
            maxH=std::max(maxH,(uint32_t)sc.height);
        }
        createDepthBuffer(maxW, maxH);

        for(size_t i=0;i<m_swapchains.size();i++){
            auto& sc=m_swapchains[i]; auto& fbs=m_swapFBs[i];
            fbs.resize(sc.images.size());
            for(size_t j=0;j<sc.images.size();j++){
                VkImageViewCreateInfo ivCI{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                ivCI.image=sc.images[j].image; ivCI.viewType=VK_IMAGE_VIEW_TYPE_2D;
                ivCI.format=m_swapFmt;
                ivCI.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                VK_CHECK(vkCreateImageView(m_device,&ivCI,nullptr,&fbs[j].view));

                VkImageView atts[]={fbs[j].view, m_depthView};
                VkFramebufferCreateInfo fbCI{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                fbCI.renderPass=m_renderPass;
                fbCI.attachmentCount=2; fbCI.pAttachments=atts;
                fbCI.width=sc.width; fbCI.height=sc.height; fbCI.layers=1;
                VK_CHECK(vkCreateFramebuffer(m_device,&fbCI,nullptr,&fbs[j].fb));
            }
        }
        std::cout << "[Vulkan] Framebuffery OK.\n";
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  GPU buffer (vertex buffer)
    // ─────────────────────────────────────────────────────────────────────────
    GpuBuffer createVertexBuffer(const std::vector<Vertex>& verts) {
        GpuBuffer b;
        b.size = verts.size() * sizeof(Vertex);

        VkBufferCreateInfo bCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bCI.size=b.size; bCI.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bCI.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(m_device,&bCI,nullptr,&b.buf));

        VkMemoryRequirements mr{};
        vkGetBufferMemoryRequirements(m_device,b.buf,&mr);
        VkMemoryAllocateInfo maI{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        maI.allocationSize=mr.size;
        maI.memoryTypeIndex=findMemType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(m_device,&maI,nullptr,&b.mem));
        VK_CHECK(vkBindBufferMemory(m_device,b.buf,b.mem,0));

        void* ptr; VK_CHECK(vkMapMemory(m_device,b.mem,0,b.size,0,&ptr));
        memcpy(ptr,verts.data(),b.size);
        vkUnmapMemory(m_device,b.mem);
        return b;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Budowanie sceny
    // ─────────────────────────────────────────────────────────────────────────
    void buildScene() {
        std::cout << "[Scene] Budowanie sceny...\n";

        // ── Podłoga (siatka) ─────────────────────────────────────────────────
        {
            SceneObject obj;
            auto verts = makeGrid(10, -1.6f); // 10m w każdą stronę, y=0
            obj.vbo        = createVertexBuffer(verts);
            obj.vertCount  = (uint32_t)verts.size();
            obj.modelMatrix= Mat4::identity();
            obj.isLines    = true;
            m_objects.push_back(obj);
        }

        // ── Sześciany ────────────────────────────────────────────────────────
        // Definicja: { x, y, z, skala, R, G, B, obrot_Y }
        struct CubeDef { float x,y,z,s, r,g,b, ry; };
        const CubeDef cubes[] = {
            //  x      y     z     s     R     G     B    rotY
            {  0.0f, 0.5f, -3.0f, 1.0f, 0.9f, 0.2f, 0.2f, 0.0f  }, // czerwony, prosto przed
            { -2.0f, 0.5f, -2.5f, 0.7f, 0.2f, 0.7f, 0.9f, 0.4f  }, // niebieski, lewo
            {  2.0f, 0.5f, -2.5f, 0.8f, 0.2f, 0.9f, 0.3f, -0.3f }, // zielony, prawo
            { -1.0f, 1.2f, -4.5f, 0.5f, 0.9f, 0.8f, 0.1f, 0.8f  }, // żółty, dalej lewo + wyżej
            {  1.5f, 0.3f, -4.0f, 0.6f, 0.7f, 0.2f, 0.9f, -0.6f }, // fioletowy, dalej prawo
        };

        for (auto& cd : cubes) {
            SceneObject obj;
            auto verts = makeCube(cd.r, cd.g, cd.b);
            obj.vbo        = createVertexBuffer(verts);
            obj.vertCount  = (uint32_t)verts.size();
            // Model: translacja × rotacja × skala
            obj.modelMatrix = Mat4::translation(cd.x, cd.y, cd.z)
                            * Mat4::rotY(cd.ry)
                            * Mat4::scale(cd.s);
            obj.isLines     = false;
            m_objects.push_back(obj);
        }

        std::cout << "[Scene] Obiektow: " << m_objects.size() << "\n";
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Renderowanie jednego oka
    // ─────────────────────────────────────────────────────────────────────────
    void renderEye(uint32_t eyeIdx, uint32_t imgIdx, const XrView& view) {
        auto& sc = m_swapchains[eyeIdx];
        auto& fb = m_swapFBs[eyeIdx][imgIdx];

        // ── Macierze widoku/projekcji z headsetu ────────────────────────────
        Mat4 viewMat = xrPoseToView(view.pose);
        Mat4 projMat = xrFovToProjection(view.fov);

        VkCommandBufferAllocateInfo cbAI{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbAI.commandPool=m_cmdPool; cbAI.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAI.commandBufferCount=1;
        VkCommandBuffer cb;
        VK_CHECK(vkAllocateCommandBuffers(m_device,&cbAI,&cb));

        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cb,&bi));

        // Wyczyść kolor nieba (ciemny błękit) + głębokość
        VkClearValue clears[2];
        clears[0].color = {{0.05f, 0.08f, 0.15f, 1.f}}; // nocne niebo
        clears[1].depthStencil = {1.f, 0};

        VkRenderPassBeginInfo rpBI{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpBI.renderPass=m_renderPass; rpBI.framebuffer=fb.fb;
        rpBI.renderArea.extent={(uint32_t)sc.width,(uint32_t)sc.height};
        rpBI.clearValueCount=2; rpBI.pClearValues=clears;
        vkCmdBeginRenderPass(cb,&rpBI,VK_SUBPASS_CONTENTS_INLINE);

        // Kierunek światła (góra-prawo, normalizowany)
        const float lightDir[4] = {0.577f, 0.577f, 0.577f, 0.f};
        const float ambCol[4]   = {0.25f, 0.25f, 0.30f, 1.f};
        const float difCol[4]   = {0.85f, 0.85f, 0.80f, 1.f};

        for(auto& obj : m_objects) {
            // Wybierz pipeline
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                obj.isLines ? m_pipeLines : m_pipeSolid);

            // MVP = proj × view × model
            PushConst pc{};
            pc.mvp   = projMat * viewMat * obj.modelMatrix;
            pc.model = obj.modelMatrix;
            memcpy(pc.lightDir,   lightDir, 16);
            memcpy(pc.ambientCol, ambCol,   16);
            memcpy(pc.diffuseCol, difCol,   16);

            vkCmdPushConstants(cb, m_pipeLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(PushConst), &pc);

            VkBuffer bufs[] = {obj.vbo.buf};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cb, 0, 1, bufs, offsets);
            vkCmdDraw(cb, obj.vertCount, 1, 0, 0);
        }

        vkCmdEndRenderPass(cb);
        VK_CHECK(vkEndCommandBuffer(cb));

        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount=1; si.pCommandBuffers=&cb;
        VK_CHECK(vkQueueSubmit(m_queue,1,&si,VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(m_queue));
        vkFreeCommandBuffers(m_device,m_cmdPool,1,&cb);
    }

    // ─────────────────────────────────────────────────────────────────────────
    void pollEvents() {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        while(xrPollEvent(m_instance,&ev)==XR_SUCCESS){
            switch(ev.type){
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:{
                auto* e=(XrEventDataSessionStateChanged*)&ev;
                m_sessionState=e->state;
                std::cout << "[OpenXR] Stan sesji: " << m_sessionState << "\n";
                if(m_sessionState==XR_SESSION_STATE_READY){
                    XrSessionBeginInfo sbi{XR_TYPE_SESSION_BEGIN_INFO};
                    sbi.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if(xrBeginSession(m_session,&sbi)==XR_SUCCESS){
                        m_sessionRunning=true;
                        std::cout << "[OpenXR] Sesja rozpoczeta! Patrz w headset.\n";
                    } else m_quit=true;
                }
                else if(m_sessionState==XR_SESSION_STATE_STOPPING){
                    xrEndSession(m_session); m_sessionRunning=false;
                }
                else if(m_sessionState==XR_SESSION_STATE_EXITING||
                        m_sessionState==XR_SESSION_STATE_LOSS_PENDING)
                    m_quit=true;
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: m_quit=true; break;
            default: break;
            }
            ev={XR_TYPE_EVENT_DATA_BUFFER};
        }
    }

    void mainLoop() {
        std::cout << "[App] Petla glowna (Ctrl+C aby wyjsc)...\n";
        uint64_t frame=0;
        while(!m_quit){
            pollEvents();
            if(!m_sessionRunning){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            XrFrameWaitInfo fwi{XR_TYPE_FRAME_WAIT_INFO};
            XrFrameState    fs {XR_TYPE_FRAME_STATE};
            XR_CHECK(xrWaitFrame(m_session,&fwi,&fs));
            XrFrameBeginInfo fbi{XR_TYPE_FRAME_BEGIN_INFO};
            XR_CHECK(xrBeginFrame(m_session,&fbi));

            // Pobierz aktualne widoki z headsetu
            XrViewLocateInfo vli{XR_TYPE_VIEW_LOCATE_INFO};
            vli.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            vli.displayTime=fs.predictedDisplayTime;
            vli.space=m_appSpace;
            XrViewState vs{XR_TYPE_VIEW_STATE};
            uint32_t vc=(uint32_t)m_views.size();
            xrLocateViews(m_session,&vli,&vs,vc,&vc,m_views.data());

            std::vector<XrCompositionLayerProjectionView> projViews(m_swapchains.size());
            std::vector<uint32_t> imgIdx(m_swapchains.size());

            for(size_t i=0;i<m_swapchains.size();i++){
                XrSwapchainImageAcquireInfo acqI{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                XR_CHECK(xrAcquireSwapchainImage(m_swapchains[i].handle,&acqI,&imgIdx[i]));
                XrSwapchainImageWaitInfo waitI{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitI.timeout=XR_INFINITE_DURATION;
                XR_CHECK(xrWaitSwapchainImage(m_swapchains[i].handle,&waitI));

                // Renderuj oko z prawdziwymi macierzami z headsetu
                renderEye((uint32_t)i, imgIdx[i], m_views[i]);

                auto& pv=projViews[i];
                pv={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                pv.pose=m_views[i].pose;
                pv.fov =m_views[i].fov;
                pv.subImage.swapchain=m_swapchains[i].handle;
                pv.subImage.imageRect={{0,0},{m_swapchains[i].width,m_swapchains[i].height}};

                XrSwapchainImageReleaseInfo relI{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                XR_CHECK(xrReleaseSwapchainImage(m_swapchains[i].handle,&relI));
            }

            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            layer.space=m_appSpace;
            layer.viewCount=(uint32_t)projViews.size();
            layer.views=projViews.data();
            const XrCompositionLayerBaseHeader* layers[]={(XrCompositionLayerBaseHeader*)&layer};

            XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
            fei.displayTime=fs.predictedDisplayTime;
            fei.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            fei.layerCount=fs.shouldRender?1:0;
            fei.layers=fs.shouldRender?layers:nullptr;
            XR_CHECK(xrEndFrame(m_session,&fei));

            if(frame%100==0) std::cout << "[App] Klatka " << frame << "\n";
            frame++;
        }
    }

    void cleanup() {
        if(m_device) vkDeviceWaitIdle(m_device);

        for(auto& obj : m_objects){
            if(obj.vbo.buf) vkDestroyBuffer(m_device,obj.vbo.buf,nullptr);
            if(obj.vbo.mem) vkFreeMemory(m_device,obj.vbo.mem,nullptr);
        }

        for(auto& fbs:m_swapFBs) for(auto& f:fbs){
            if(f.fb)   vkDestroyFramebuffer(m_device,f.fb,nullptr);
            if(f.view) vkDestroyImageView(m_device,f.view,nullptr);
        }

        if(m_depthView)  vkDestroyImageView(m_device,m_depthView,nullptr);
        if(m_depthImage) vkDestroyImage(m_device,m_depthImage,nullptr);
        if(m_depthMem)   vkFreeMemory(m_device,m_depthMem,nullptr);

        if(m_pipeSolid)  vkDestroyPipeline(m_device,m_pipeSolid,nullptr);
        if(m_pipeLines)  vkDestroyPipeline(m_device,m_pipeLines,nullptr);
        if(m_pipeLayout) vkDestroyPipelineLayout(m_device,m_pipeLayout,nullptr);
        if(m_renderPass) vkDestroyRenderPass(m_device,m_renderPass,nullptr);
        if(m_cmdPool)    vkDestroyCommandPool(m_device,m_cmdPool,nullptr);
        if(m_device)     vkDestroyDevice(m_device,nullptr);
        if(m_vkInstance) vkDestroyInstance(m_vkInstance,nullptr);

        for(auto& sc:m_swapchains) if(sc.handle) xrDestroySwapchain(sc.handle);
        if(m_appSpace) xrDestroySpace(m_appSpace);
        if(m_session)  xrDestroySession(m_session);
        if(m_instance) xrDestroyInstance(m_instance);

        std::cout << "[App] Cleanup OK.\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
int main() {
    try {
        VRScene app;
        app.run();
    } catch(const std::exception& e){
        std::cerr << "\n[BLAD] " << e.what() << "\n\n"
                  << "Upewnij sie ze:\n"
                  << "  1. Oculus software / SteamVR jest uruchomiony\n"
                  << "  2. Headset jest podlaczony\n"
                  << "  3. glslangValidator.exe jest w PATH (Vulkan SDK)\n";
        return 1;
    }
}
