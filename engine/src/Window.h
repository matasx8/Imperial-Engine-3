#pragma once
#include "Swapchain.h"

#include <cstdint>
#include <glm/mat4x4.hpp>

namespace imp
{
    enum class WindowType
    {
        kPlatformWindow,
        kHeadlessWindow
    };

    struct WindowInitParams
    {
        void* pNext = nullptr;
        uint32_t width = 1600;
        uint32_t height = 800;
        uint64_t windowHandle = 0;
        WindowType type = WindowType::kPlatformWindow;
    };

    class Window
    {
        public:
        Window() = default;
        virtual ~Window() = default;
        
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        virtual bool Initialize(const WindowInitParams& params) = 0;
        virtual bool Shutdown(VkInstance instance, VkDevice device);

        virtual VkResult InitializeSwapchain(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
        virtual VkResult CreateWindowSurface(VkInstance instance) = 0;

        virtual void UpdateInfo(double frameTimeMs) = 0;

        virtual void MoveCamera(glm::mat4& transform, float delta) = 0;

        virtual bool ShouldClose() const = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual Swapchain& GetSwapchain() { return m_Swapchain; };

        virtual const char* const* GetRequiredInstanceExtensions(uint32_t* count) const = 0;
        virtual const char* const* GetRequiredDeviceExtensions(uint32_t* count) const = 0;

        virtual VkSurfaceKHR GetWindowSurface() const = 0;

        VkSurfaceCapabilitiesKHR GetSurfaceCapabilities(VkPhysicalDevice device) const;

    protected:
        Swapchain m_Swapchain {};

        double m_LastTime = 0.0;
        double m_DeltaTime = 0.0;
    };
}