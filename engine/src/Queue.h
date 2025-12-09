#pragma once
#include "volk.h"

#include <vector>

namespace imp
{
	struct QueueFamilyIndices
	{
		int graphicsFamily;
		int computeFamily;
		int numUniqueQueues;
	};

	class Queue
	{
	public:
		Queue() = default;

		VkResult Initialize(VkPhysicalDevice physicalDevice, const std::vector<const char*>& requiredExtensions, const VkPhysicalDeviceFeatures2* requiredFeatures);
		VkResult ShutDown();

		inline VkDevice GetDevice() const { return m_Device; }
		inline const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }

		inline VkQueue GetGraphicsQueue() const { return m_GraphicsQ; }
		inline VkQueue GetComputeQueue() const { return m_ComputeQ; }

	private:

		VkResult FindQueueFamilies(VkPhysicalDevice physicalDevice);
		VkResult AquireDeviceQueues();

		VkQueue m_GraphicsQ = VK_NULL_HANDLE;
		VkQueue m_ComputeQ = VK_NULL_HANDLE;

		QueueFamilyIndices m_QueueFamilyIndices = {};

		VkDevice m_Device = VK_NULL_HANDLE;
	};
}