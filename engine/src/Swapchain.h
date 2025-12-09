#pragma once
#include "VulkanFunctionTable.h"

#include <vector>

namespace imp
{
    typedef std::vector<VkImage> SwapchainImages;
    typedef std::vector<VkImageView> SwapchainImageViews;

    struct SwapchainInitParams
    {
        void* pNext;
        VkFormat format;
        VkSurfaceKHR surface;
        VkColorSpaceKHR colorSpace;
        VkExtent2D imageExtent;
        uint32_t imageCount;
        VkImageUsageFlags imageUsage;
        VkPresentModeKHR presentMode;
    };

    class Swapchain
    {
        public:

        Swapchain() = default;

        VkResult Initialize(VkDevice device, const SwapchainInitParams& params);
        void Destroy(VkDevice device);

        VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
        VkImage GetSwapchainImage(uint32_t index) const { return m_Images[index]; }
        VkImageView GetSwapchainImageView(uint32_t index) const { return m_ImageViews[index]; }
        uint32_t GetSwapchainImageCount() const { return m_Images.size(); }
        VkFormat GetSurfaceFormat() const { return m_SurfaceFormat; }

        private:

        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        SwapchainImages m_Images {};
        SwapchainImageViews m_ImageViews {};

        VkFormat m_SurfaceFormat = VK_FORMAT_UNDEFINED;
    };
}