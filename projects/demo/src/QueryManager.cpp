#include "QueryManager.h"

#include <cstdio>

bool QueryManager::Initialize(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t framesInFlight)
{
    m_FramesInFlight = framesInFlight;

    // Check that the graphics queue supports timestamps.
    VkPhysicalDeviceProperties props {};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    m_TimestampsSupported = props.limits.timestampComputeAndGraphics;
    // Convert nanoseconds per tick -> milliseconds per tick.
    m_TimestampPeriodMs = props.limits.timestampPeriod / 1e6f;

    m_TimestampPools.resize(framesInFlight, VK_NULL_HANDLE);
    m_StatsPools.resize(framesInFlight, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < framesInFlight; i++)
    {
        if (m_TimestampsSupported)
        {
            VkQueryPoolCreateInfo tsci {};
            tsci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            tsci.queryType  = VK_QUERY_TYPE_TIMESTAMP;
            tsci.queryCount = kTimestampCount;
            if (vkCreateQueryPool(device, &tsci, nullptr, &m_TimestampPools[i]) != VK_SUCCESS)
                return false;
        }

        VkQueryPoolCreateInfo stci {};
        stci.sType                  = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        stci.queryType              = VK_QUERY_TYPE_PIPELINE_STATISTICS;
        stci.queryCount             = kStatCount;
        stci.pipelineStatistics     = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
        if (vkCreateQueryPool(device, &stci, nullptr, &m_StatsPools[i]) != VK_SUCCESS)
            return false;
    }

    return true;
}

void QueryManager::Destroy(VkDevice device)
{
    for (uint32_t i = 0; i < m_FramesInFlight; i++)
    {
        if (m_TimestampPools[i] != VK_NULL_HANDLE)
            vkDestroyQueryPool(device, m_TimestampPools[i], nullptr);
        if (m_StatsPools[i] != VK_NULL_HANDLE)
            vkDestroyQueryPool(device, m_StatsPools[i], nullptr);
    }
}

void QueryManager::BeginFrame(VkCommandBuffer cb, uint32_t frameIndex)
{
    // Reset all query slots at the top of the command buffer (required before first use).
    if (m_TimestampsSupported)
        vkCmdResetQueryPool(cb, m_TimestampPools[frameIndex], 0, kTimestampCount);
    vkCmdResetQueryPool(cb, m_StatsPools[frameIndex], 0, kStatCount);
}

void QueryManager::BeginCullingPass(VkCommandBuffer cb, uint32_t frameIndex)
{
    if (m_TimestampsSupported)
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            m_TimestampPools[frameIndex], kTsCullingBegin);
}

void QueryManager::EndCullingPass(VkCommandBuffer cb, uint32_t frameIndex)
{
    if (m_TimestampsSupported)
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_TimestampPools[frameIndex], kTsCullingEnd);
}

void QueryManager::BeginGBufferPass(VkCommandBuffer cb, uint32_t frameIndex)
{
    if (m_TimestampsSupported)
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            m_TimestampPools[frameIndex], kTsGBufferBegin);
    vkCmdBeginQuery(cb, m_StatsPools[frameIndex], kStatGBufferPass, 0);
}

void QueryManager::EndGBufferPass(VkCommandBuffer cb, uint32_t frameIndex)
{
    vkCmdEndQuery(cb, m_StatsPools[frameIndex], kStatGBufferPass);
    if (m_TimestampsSupported)
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_TimestampPools[frameIndex], kTsGBufferEnd);
}

void QueryManager::BeginLightingPass(VkCommandBuffer cb, uint32_t frameIndex)
{
    if (m_TimestampsSupported)
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            m_TimestampPools[frameIndex], kTsLightingBegin);
}

void QueryManager::EndLightingPass(VkCommandBuffer cb, uint32_t frameIndex)
{
    if (m_TimestampsSupported)
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_TimestampPools[frameIndex], kTsLightingEnd);
}

void QueryManager::ReadbackAndPrint(VkDevice device, uint32_t frameIndex)
{
    // Pipeline statistics
    uint64_t stats[kStatCount] = {};
    vkGetQueryPoolResults(device, m_StatsPools[frameIndex], 0, kStatCount,
        sizeof(stats), stats, sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT);  // non-blocking: returns whatever is available

    // Timestamps
    double cullingMs  = 0.0;
    double gbufferMs  = 0.0;
    double lightingMs = 0.0;
    if (m_TimestampsSupported)
    {
        uint64_t ts[kTimestampCount] = {};
        vkGetQueryPoolResults(device, m_TimestampPools[frameIndex], 0, kTimestampCount,
            sizeof(ts), ts, sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT);
        cullingMs  = static_cast<double>(ts[kTsCullingEnd]  - ts[kTsCullingBegin])  * m_TimestampPeriodMs;
        gbufferMs  = static_cast<double>(ts[kTsGBufferEnd]  - ts[kTsGBufferBegin])  * m_TimestampPeriodMs;
        lightingMs = static_cast<double>(ts[kTsLightingEnd] - ts[kTsLightingBegin]) * m_TimestampPeriodMs;
    }

    printf("[GPU] Culling: %.3f ms  GBuffer: %.3f ms  Lighting: %.3f ms  |  Triangles (GBuffer): %llu\n",
        cullingMs, gbufferMs, lightingMs,
        static_cast<unsigned long long>(stats[kStatGBufferPass]));
}
