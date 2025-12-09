#include "Swapchain.h"
#include "Log.h"

namespace imp
{
    VkResult Swapchain::Initialize(VkDevice device, const SwapchainInitParams& params)
    {
        VkSwapchainCreateInfoKHR sci {};
        sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sci.surface = params.surface;
        sci.imageFormat = params.format;
        sci.imageColorSpace = params.colorSpace;
        sci.presentMode = params.presentMode;
        sci.imageExtent = params.imageExtent;
        sci.minImageCount = params.imageCount;
        sci.imageArrayLayers = 1;
        sci.imageUsage = params.imageUsage;
        sci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sci.clipped = VK_TRUE;

        VkResult result = vkCreateSwapchainKHR(device, &sci, nullptr, &m_Swapchain);
        if (result != VK_SUCCESS)
        {
            g_Log("Failed to create a Swapchain with result %d.\n", result);
            return result;
        }

        uint32_t count = 0;
        result = vkGetSwapchainImagesKHR(device, m_Swapchain, &count, nullptr);
        m_Images.resize(count);
        result = vkGetSwapchainImagesKHR(device, m_Swapchain, &count, m_Images.data());

        if (result != VK_SUCCESS && result != VK_INCOMPLETE)
        {
            g_Log("Failed to get %d number of Swapchain Images with result %d. \n", params.imageCount, result);
            return result;
        }

        if (count == 0)
        {
            g_Log("Failed to get any number of Swapchain Images.\n");
            return VK_INCOMPLETE;
        }

        m_Images.resize(count);
        m_ImageViews.resize(count);

        m_SurfaceFormat = params.format;

        for (uint32_t i = 0; i < count; i++)
        {
            VkImageViewCreateInfo ivci {};
            ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivci.components.a = VK_COMPONENT_SWIZZLE_A;
            ivci.components.b = VK_COMPONENT_SWIZZLE_B;
            ivci.components.r = VK_COMPONENT_SWIZZLE_R;
            ivci.components.g = VK_COMPONENT_SWIZZLE_G;
            ivci.format = m_SurfaceFormat;
            ivci.image = m_Images[i];
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ivci.subresourceRange.layerCount = 1;
            ivci.subresourceRange.levelCount = 1;

            vkCreateImageView(device, &ivci, nullptr, &m_ImageViews[i]);
        }

        return result;
    }

    void Swapchain::Destroy(VkDevice device)
    {
        for (const auto& iv : m_ImageViews)
            vkDestroyImageView(device, iv, nullptr);
        vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
    }
}