#include "SafeResourceDestroyer.h"

namespace imp
{
    void SafeResourceDestroyer::EnqueueResourceForDestruction(VulkanResource& resource, uint64_t submitPoint)
    {
        m_Queue.push_back(std::make_pair(submitPoint, resource));
    }

    void SafeResourceDestroyer::ProcessQueue(VkDevice device, uint64_t completedPoint)
    {
        while (!m_Queue.empty())
        {
            auto& front = m_Queue.front();
            if (front.first > completedPoint)
                break;

            VulkanResource& resource = front.second;
            switch (resource.type)
            {
            case VulkanResourceType::Buffer:
                vkt.vkDestroyBuffer(device, resource.buffer, nullptr);
                break;
            case VulkanResourceType::Image:
                vkt.vkDestroyImage(device, resource.image, nullptr);
                break;
            default:
                break;
            }
            vkt.vkFreeMemory(device, resource.memory, nullptr);
            m_Queue.pop_front();
        }
    }
}