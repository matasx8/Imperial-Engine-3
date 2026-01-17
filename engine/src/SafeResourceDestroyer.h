#pragma once
#include "VulkanFunctionTable.h"

#include <deque>

namespace imp
{
    enum class VulkanResourceType
    {
        Buffer,
        Image,
        Semaphore
    };

    struct VulkanResource
    {
        VulkanResourceType type;
        union {
            VkBuffer buffer;
            VkImage image;
            VkSemaphore semaphore;
        };
        VkDeviceMemory memory;

    };

    class SafeResourceDestroyer
    {
    public:

        SafeResourceDestroyer() = default;
        ~SafeResourceDestroyer() = default;

        void EnqueueResourceForDestruction(VulkanResource& resource, uint64_t submitPoint);

        void ProcessQueue(VkDevice device, uint64_t completedPoint);

    private:

        std::deque<std::pair<uint64_t, VulkanResource>> m_Queue;
    };
}