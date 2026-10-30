cmake_minimum_required(VERSION 3.20)

project(TesseractVR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
if(NOT ANDROID)
    find_package(glfw3 REQUIRED)
endif()
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenXR CONFIG REQUIRED)

find_program(GLSLANG_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
if(ANDROID)
    set(SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/assets/shaders)
else()
    set(SHADERS_DIR ${OUTPUT_DIR}/shaders)
endif()

if(ANDROID)
    add_library(${PROJECT_NAME} SHARED
        android/main.cpp
        src/Application.cpp
        src/VulkanRenderer.cpp
        src/OpenXRManager.cpp
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
    )
else()
    add_executable(${PROJECT_NAME} src/main.cpp src/Application.cpp src/VulkanRenderer.cpp src/OpenXRManager.cpp)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE XR_USE_GRAPHICS_API_VULKAN)

if(ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TESSERACT_ANDROID XR_USE_PLATFORM_ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        OpenXR::openxr_loader
        android
        log
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw GPUOpen::VulkanMemoryAllocator glm::glm OpenXR::openxr_loader)
endif()

add_custom_command(
    OUTPUT ${SHADERS_DIR}/vert.spv ${SHADERS_DIR}/frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    COMMAND ${GLSLANG_VALIDATOR} --target-env vulkan1.0 -o ${SHADERS_DIR}/vert.spv ${CMAKE_CURRENT_SOURCE_DIR}/shaders/09_shader_base.vert
    COMMAND ${GLSLANG_VALIDATOR} --target-env vulkan1.0 -o ${SHADERS_DIR}/frag.spv ${CMAKE_CURRENT_SOURCE_DIR}/shaders/09_shader_base.frag
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/09_shader_base.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/09_shader_base.frag
    COMMENT "Compiling shaders"
    VERBATIM
)

add_custom_target(shaders DEPENDS ${SHADERS_DIR}/vert.spv ${SHADERS_DIR}/frag.spv)
add_dependencies(${PROJECT_NAME} shaders)


# Automatyczne kopiowanie świeżych shaderów obok pliku .exe po kompilacji
if(NOT ANDROID)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADERS_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    )
endif()