#pragma once
#include "volk.h"

#include <vector>
#include <string>
#include <array>

namespace imp
{
	inline static constexpr std::initializer_list<const char*> g_PreferredInstanceExtensions { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
	inline static constexpr std::initializer_list<const char*> g_PreferredInstanceLayers { "VK_LAYER_KHRONOS_validation" };
	inline static constexpr std::array<const char*, 1> g_RequiredDeviceExtensions =
	{
		// TODO: use Windowing system to request swapchain extension
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		//VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
	};

	VkResult GetInstanceLayers(std::vector<std::string>& layers);
	VkResult GetInstanceExtensions(const std::vector<std::string>& enabledLayers, const std::vector<const char*>& preferred, std::vector<std::string>& actual);

	VkResult CheckAllRequiredExtensionsSupported(VkPhysicalDevice device, const std::vector<const char*>& requiredExtensions);
}