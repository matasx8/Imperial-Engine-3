project(ImperialEngine3_Engine LANGUAGES CXX)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "src/Engine.cpp"
    "src/CommandBufferPool.cpp"
    "src/Debug.cpp"
    "src/Layers.cpp"
    "src/Log.cpp"
    "src/Platform.cpp"
    "src/PrimitivePool.cpp"
    "src/Queue.cpp"
    "src/SubmitSyncManager.cpp"
    "src/Swapchain.cpp"
    "src/Window.cpp"
    "src/VulkanFunctionTable.h"
    "src/Platform/Windows/PlatformImpl.cpp"
    "src/Platform/Windows/WindowGLFW.cpp"
)

add_library(ImperialEngine3_Engine STATIC ${ENGINE_SOURCES})

target_include_directories(ImperialEngine3_Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

## -- volk -- 
set(VOLK_PULL_IN_VULKAN ON CACHE BOOL "Make Volk include Vulkan headers" FORCE)
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR CACHE STRING "" FORCE)
add_subdirectory(external/Volk)
target_link_libraries(ImperialEngine3_Engine PUBLIC volk)

## -- GLFW --
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

add_subdirectory(external/GLFW)
target_link_libraries(ImperialEngine3_Engine PRIVATE glfw)
