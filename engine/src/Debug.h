#pragma once
#include "volk.h"

#include <vector>
#include <string>

namespace imp
{
	bool ShouldInitDebugger(const std::vector<std::string>& enabledLayers, const std::vector<std::string>& enabledExtensions);
	VkResult InitializeDebugger(VkInstance instance);
	void DestroyDebugger(VkInstance instance);

	inline void SetDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name)
	{
		if (!name || device == VK_NULL_HANDLE) return;
		VkDebugUtilsObjectNameInfoEXT nameInfo {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = objectType;
		nameInfo.objectHandle = objectHandle;
		nameInfo.pObjectName = name;
		vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
	}
}