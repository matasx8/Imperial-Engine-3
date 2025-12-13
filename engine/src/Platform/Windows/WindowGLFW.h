#pragma once
#include "Window.h"

#include <array>

struct GLFWwindow;

namespace imp
{
    class WindowGLFW : public Window
    {
    public:
        WindowGLFW() = default;
        ~WindowGLFW() = default;

        virtual bool Initialize(const WindowInitParams& params) override;
        virtual bool Shutdown(VkInstance instance, VkDevice device) override;

        virtual VkResult CreateWindowSurface(VkInstance instance) override;
        
        virtual void UpdateInfo(double frameTimeMs) override;

        virtual bool ShouldClose() const override;

        virtual uint32_t GetWidth() const override;
        virtual uint32_t GetHeight() const override;

        virtual const char* const* GetRequiredInstanceExtensions(uint32_t* count) const override;
        virtual const char* const* GetRequiredDeviceExtensions(uint32_t* count) const override;

        virtual VkSurfaceKHR GetWindowSurface() const override { return m_Surface; };

    private:
        static constexpr std::array<const char*, 1> kRequiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        GLFWwindow* m_Window = nullptr;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    };
}