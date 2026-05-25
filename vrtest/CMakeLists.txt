cmake_minimum_required(VERSION 3.16)
project(vrtest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Vulkan ──────────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)

# ── OpenXR - szukaj w C:/openxr (Twoja instalacja) ─────────────────────────
find_package(OpenXR QUIET
    PATHS "C:/openxr"
    NO_DEFAULT_PATH
)

if(NOT OpenXR_FOUND)
    find_path(OPENXR_INCLUDE_DIR openxr/openxr.h
        PATHS
            "C:/openxr/include"
            "$ENV{OPENXR_SDK_ROOT}/include"
            "C:/OpenXR-SDK/include"
            "/usr/include"
    )
    find_library(OPENXR_LIBRARY
        NAMES openxr_loader
        PATHS
            "C:/openxr/x64/lib"
            "C:/openxr/x64/lib/cmake"
            "C:/openxr/Win32/lib"
            "$ENV{OPENXR_SDK_ROOT}/x64/lib"
            "C:/OpenXR-SDK/lib"
            "/usr/lib"
            "/usr/lib/x86_64-linux-gnu"
        NO_DEFAULT_PATH
    )
    if(OPENXR_INCLUDE_DIR AND OPENXR_LIBRARY)
        set(OpenXR_FOUND TRUE)
        add_library(OpenXR::openxr_loader UNKNOWN IMPORTED)
        set_target_properties(OpenXR::openxr_loader PROPERTIES
            IMPORTED_LOCATION "${OPENXR_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENXR_INCLUDE_DIR}"
        )
        message(STATUS "OpenXR include: ${OPENXR_INCLUDE_DIR}")
        message(STATUS "OpenXR library: ${OPENXR_LIBRARY}")
    else()
        message(FATAL_ERROR
            "Nie znaleziono OpenXR SDK!\n"
            "  Include dir: ${OPENXR_INCLUDE_DIR}\n"
            "  Library:     ${OPENXR_LIBRARY}\n"
            "  Sprawdz czy C:/openxr/include/openxr/openxr.h istnieje\n"
            "  i czy C:/openxr/x64/lib/openxr_loader.lib istnieje"
        )
    endif()
endif()

# ── Executable ──────────────────────────────────────────────────────────────
add_executable(vrtest main.cpp)

target_compile_definitions(vrtest PRIVATE
    XR_USE_GRAPHICS_API_VULKAN
    VK_USE_PLATFORM_WIN32_KHR
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(vrtest PRIVATE
    Vulkan::Vulkan
    OpenXR::openxr_loader
)

set_target_properties(vrtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)