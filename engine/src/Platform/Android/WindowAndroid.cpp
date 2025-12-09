#include "Platform/Android/WindowAndroid.h"


namespace imp
{
    bool WindowAndroid::Initialize(const WindowInitParams& params)
    {
        m_WindowHandle = reinterpret_cast<ANativeWindow*>(params.windowHandle);

        return true;
    }
    bool WindowAndroid::Shutdown(VkInstance instance, VkDevice device)
    {
        return true;
    }

    VkResult WindowAndroid::CreateWindowSurface(VkInstance instance)
    {
        VkAndroidSurfaceCreateInfoKHR ci {};
        ci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        ci.pNext = nullptr;
        ci.flags = 0;
        ci.window = m_WindowHandle;

        VkResult result = vkCreateAndroidSurfaceKHR(instance, &ci, nullptr, &m_Surface);
        return result;
    }
    
    void WindowAndroid::UpdateInfo()
    {

    }

    bool WindowAndroid::ShouldClose() const
    {
        return true;
    }

    uint32_t WindowAndroid::GetWidth() const
    {
        return 0;
    }
    uint32_t WindowAndroid::GetHeight() const
    {
        return 0;
    }

    const char* const* WindowAndroid::GetRequiredInstanceExtensions(uint32_t* count) const
    {
        *count = kRequiredInstanceExtensions.size();
        return kRequiredInstanceExtensions.data();
    }
    const char* const* WindowAndroid::GetRequiredDeviceExtensions(uint32_t* count) const
    {
        *count = kRequiredDeviceExtensions.size();
        return kRequiredDeviceExtensions.data();
    }
}