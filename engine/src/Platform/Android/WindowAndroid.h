#pragma once
#include "Window.h"

#include <array>

namespace imp
{
    class WindowAndroid : public Window
    {
    public:
        WindowAndroid() = default;
        ~WindowAndroid() = default;

        virtual bool Initialize(const WindowInitParams& params) override;
        virtual bool Shutdown(VkInstance instance, VkDevice device) override;

        virtual VkResult CreateWindowSurface(VkInstance instance) override;
        
        virtual void UpdateInfo() override;

        virtual bool ShouldClose() const override;

        virtual uint32_t GetWidth() const override;
        virtual uint32_t GetHeight() const override;

        virtual const char* const* GetRequiredInstanceExtensions(uint32_t* count) const override;
        virtual const char* const* GetRequiredDeviceExtensions(uint32_t* count) const override;

        virtual VkSurfaceKHR GetWindowSurface() const override { return m_Surface; };

    private:
        static constexpr std::array<const char*, 1> kRequiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        static constexpr std::array<const char*, 2> kRequiredInstanceExtensions = { VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME };

        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        ANativeWindow* m_WindowHandle = nullptr;
    };
}