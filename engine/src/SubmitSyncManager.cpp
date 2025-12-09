#include "SubmitSyncManager.h"
#include "Log.h"

#include <cassert>

namespace imp
{
    VkFence FenceFactory::Create(const FenceFactory::Args& args)
    {
        static constexpr VkFenceCreateInfo fci {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};

        VkFence fence = VK_NULL_HANDLE;
        VkResult res = vkt.vkCreateFence(args.device, &fci, nullptr, &fence);
        if (res != VK_SUCCESS)
            g_Log("Failed to create VkFence with result %d\n", res);
        return fence;
    }

    void FenceFactory::Destroy(VkFence fence, const FenceFactory::Args& args)
    {
        vkt.vkDestroyFence(args.device, fence, nullptr);
    }

    VkSemaphore SemaphoreFactory::Create(const SemaphoreFactory::Args& args)
    {
        static constexpr VkSemaphoreCreateInfo fci {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};

        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkResult res = vkt.vkCreateSemaphore(args.device, &fci, nullptr, &semaphore);
        if (res != VK_SUCCESS)
            g_Log("Failed to create VkSemaphore with result &d\n", res);
        return semaphore;
    }

    void SemaphoreFactory::Destroy(VkSemaphore semaphore, const SemaphoreFactory::Args& args)
    {
        vkt.vkDestroySemaphore(args.device, semaphore, nullptr);
    }

    VkResult SubmitSyncManager::Initialize(VkDevice device)
    {
        return VK_SUCCESS;
    }

    VkResult SubmitSyncManager::Shutdown(VkDevice device)
    {
        FenceFactory::Args fArgs {device};
        m_FencePool.Destroy(fArgs);

        SemaphoreFactory::Args sArgs {device};
        m_SemaphorePool.Destroy(sArgs);

        return VK_SUCCESS;
    }

    SubmitSync SubmitSyncManager::GetSubmitSync(VkDevice device)
    {
        FenceFactory::Args fArgs {device};
        SemaphoreFactory::Args sArgs {device};

        SubmitSync sync;
        sync.submit = ++m_ActualPoint;
        sync.fence = m_FencePool.Acquire(fArgs);
        sync.semaphore = m_SemaphorePool.Acquire(sArgs);

        return sync;
    }

    void SubmitSyncManager::InsertIntoTimeline(const SubmitSync& sync)
    {
        // Inserting out of order Syncs into the submission timeline is UB
        if (m_Syncs.size())
            assert(m_Syncs.back().submit < sync.submit);

        m_Syncs.push_back(sync);
    }

    VkResult SubmitSyncManager::WaitForSubmitSync(VkDevice device, const SubmitSync& sync, uint64_t timeout)
    {
        VkResult res = VK_SUCCESS;
        int numSyncsToRecycle = 0;
        for (const auto& s : m_Syncs)
        {
            if (s.submit < sync.submit)
            {
                numSyncsToRecycle++;
                continue;
            }

            res = vkt.vkWaitForFences(device, 1, &sync.fence, VK_TRUE, timeout);
            if (res == VK_TIMEOUT)
                return res;

            if (res != VK_SUCCESS)
            {
                g_Log("Failed to wait for fence wtih result %d\n", res);
                return res;
            }

            numSyncsToRecycle++;
            m_LastPoint = sync.submit;
            break;
        }

        const bool destroyLastSemaphore = numSyncsToRecycle == m_Syncs.size();
        for (int i = 0; i < numSyncsToRecycle; i++)
        {
            const auto& s = m_Syncs.front();
            vkt.vkWaitForFences(device, 1, &s.fence, VK_TRUE, 0); // al previous fences must be signalled already
            res = vkt.vkResetFences(device, 1, &s.fence);
            if (res == VK_SUCCESS)
                m_FencePool.Release(s.fence);

            if (i != numSyncsToRecycle - 1)
                m_SemaphorePool.Release(s.semaphore);
            else // We can't recycle the very last semaphore since it'd be in the pending state
                vkt.vkDestroySemaphore(device, s.semaphore, nullptr);

            m_Syncs.pop_front();
        }

        return res;
    }
}