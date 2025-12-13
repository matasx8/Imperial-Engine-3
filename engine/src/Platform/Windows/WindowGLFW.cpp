#include "Platform/Windows/WindowGLFW.h"
#include "Volk/volk.h"
#include <GLFW/glfw3.h>


namespace imp
{
    bool WindowGLFW::Initialize(const WindowInitParams& params)
    {
        if (glfwInit() != GLFW_TRUE)
            return false;

        glfwInitVulkanLoader(vkGetInstanceProcAddr);

        if (glfwVulkanSupported() != GLFW_TRUE)
            return false;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_Window = glfwCreateWindow(params.width, params.height, "Imperial Engine 3", NULL, NULL);

        return true;
    }
    bool WindowGLFW::Shutdown(VkInstance instance, VkDevice device)
    {
        return true;
    }

    VkResult WindowGLFW::CreateWindowSurface(VkInstance instance)
    {
        return glfwCreateWindowSurface(instance, m_Window, NULL, &m_Surface);;
    }
    
    void WindowGLFW::UpdateInfo(double frameTimeMs)
    {
        char titleBuffer[256];
        snprintf(titleBuffer, sizeof(titleBuffer), "Imperial Engine 3 - %.2f ms (%.0f FPS)", 
                 frameTimeMs, 1000.0 / frameTimeMs);
        glfwSetWindowTitle(m_Window, titleBuffer);
    }

    bool WindowGLFW::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }

    uint32_t WindowGLFW::GetWidth() const
    {
        int width = 0;
        glfwGetWindowSize(m_Window, &width, nullptr);
        return static_cast<uint32_t>(width);
    }

    uint32_t WindowGLFW::GetHeight() const
    {
        int height = 0;
        glfwGetWindowSize(m_Window, nullptr, &height);
        return static_cast<uint32_t>(height);
    }

    const char* const* WindowGLFW::GetRequiredInstanceExtensions(uint32_t* count) const
    {
        return glfwGetRequiredInstanceExtensions(count);
    }
    const char* const* WindowGLFW::GetRequiredDeviceExtensions(uint32_t* count) const
    {
        *count = kRequiredDeviceExtensions.size();
        return kRequiredDeviceExtensions.data();
    }
}