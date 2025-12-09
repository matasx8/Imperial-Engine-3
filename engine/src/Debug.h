#pragma once
#include "volk.h"

#include <vector>
#include <string>

namespace imp
{
	bool ShouldInitDebugger(const std::vector<std::string>& enabledLayers, const std::vector<std::string>& enabledExtensions);
	VkResult InitializeDebugger(VkInstance instance);
	void DestroyDebugger(VkInstance instance);
}