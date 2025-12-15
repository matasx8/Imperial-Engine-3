#include "Window.h"

namespace imp
{
    uint32_t AdjustSwapchainImageCount(uint32_t count, const VkSurfaceCapabilitiesKHR& caps)
    {
        if (count > caps.maxImageCount && caps.maxImageCount != 0)
            return caps.maxImageCount;
        if (count < caps.minImageCount)
            return caps.minImageCount;
        return count;
    }

    VkResult Window::InitializeSwapchain(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
    {
        VkResult result = CreateWindowSurface(instance);
        if (result != VK_SUCCESS)
            return result;

        SwapchainInitParams params {};
        params.surface = GetWindowSurface();

        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, params.surface, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats { count };
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, params.surface, &count, formats.data());

        params.format = formats.front().format;
        params.colorSpace = formats.front().colorSpace;

        VkSurfaceCapabilitiesKHR surfaceCaps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, params.surface, &surfaceCaps);

        params.imageCount = AdjustSwapchainImageCount(2, surfaceCaps);
        params.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        params.imageExtent = { GetWidth(), GetHeight() };
        params.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR ;

        result = m_Swapchain.Initialize(device, params);
        return result;
    }

    bool Window::Shutdown(VkInstance instance, VkDevice device)
    {
        m_Swapchain.Destroy(device);
        return true;
    }

    VkSurfaceCapabilitiesKHR Window::GetSurfaceCapabilities(VkPhysicalDevice device) const
    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, GetWindowSurface(), &surfaceCaps);
        return surfaceCaps;
    }
}