#pragma once
#include "volk.h"

namespace imp
{
	class CommandBufferPool
	{
	public:

		CommandBufferPool() = default;
		~CommandBufferPool() = default;

		VkResult Initialize(VkDevice device);
		VkResult Shutdown(VkDevice device);

		VkCommandBuffer GetCommandBuffer();

	private:

		VkCommandPool m_CommandPool = VK_NULL_HANDLE;
	};
}