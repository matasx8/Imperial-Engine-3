#pragma once
#include "VulkanFunctionTable.h"
#include "PrimitivePool.h"

#include <deque>

namespace imp
{
    class SafeResourceDestroyer;

    struct FenceFactory
    {
        struct Args
        {
            VkDevice device;
            VkFenceCreateFlags fcflags;
        };

        static VkFence Create(const Args& args);
        static void Destroy(VkFence fence, const Args& args);
    };

    struct SemaphoreFactory
    {
        struct Args
        {
            VkDevice device;
        };

        static VkSemaphore Create(const Args& args);
        static void Destroy(VkSemaphore semaphore, const Args& args);
    };

    typedef PrimitivePool<VkFence, FenceFactory, FenceFactory::Args> FencePool;
    typedef PrimitivePool<VkSemaphore, SemaphoreFactory, SemaphoreFactory::Args> SemaphorePool;

    struct SubmitSync
    {
        uint64_t submit;
        VkSemaphore semaphore;
        VkFence fence;
    };

    class SubmitSyncManager
    {
    public:

        SubmitSyncManager() = default;

        VkResult Initialize(VkDevice device, SafeResourceDestroyer* destroyer);
        VkResult Shutdown(VkDevice device);

        // Increment the SubmitSync on the Timeline
        // SubmitSync0 < SubmitSync1
        SubmitSync GetSubmitSync(VkDevice device);
        SubmitSync GetSubmitSync(VkDevice device, VkFenceCreateFlags fcflags);
        const SubmitSync* GetLastSubmitSync() const { return m_Syncs.size() ? &m_Syncs.back() : nullptr; }
        uint64_t GetLastSyncedPoint() const { return m_LastPoint; }

        void InsertIntoTimeline(const SubmitSync& sync);

        VkResult WaitForSubmitSync(VkDevice device, const SubmitSync& sync, uint64_t timeout);


    private:

        uint64_t m_LastPoint = 0;
        uint64_t m_ActualPoint = 0;

        std::deque<SubmitSync> m_Syncs;

        FencePool m_FencePool {};
        SemaphorePool m_SemaphorePool {};

        SafeResourceDestroyer* m_SafeResourceDestroyer = nullptr;
    };
}