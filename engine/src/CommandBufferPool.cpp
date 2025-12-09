#include "CommandBufferPool.h"

namespace imp
{
	VkResult CommandBufferPool::Initialize(VkDevice device)
	{
		return VkResult();
	}

	VkResult CommandBufferPool::Shutdown(VkDevice device)
	{
		return VkResult();
	}

	VkCommandBuffer CommandBufferPool::GetCommandBuffer()
	{
		return VkCommandBuffer();
	}
}