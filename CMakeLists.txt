cmake_minimum_required(VERSION 3.20)

project(TesseractVR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

find_program(GLSLANG_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
set(SHADERS_DIR ${OUTPUT_DIR}/shaders)

add_executable(${PROJECT_NAME} src/main.cpp src/Application.cpp src/VulkanRenderer.cpp)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw GPUOpen::VulkanMemoryAllocator glm::glm)

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
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SHADERS_DIR}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)