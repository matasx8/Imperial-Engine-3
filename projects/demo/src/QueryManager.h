#pragma once
#include "volk.h"

#include <vector>
#include <cstdint>

// ---------------------------------------------------------
//  QueryManager
//
//  Per frame-in-flight:
//    - TIMESTAMP pool       : 4 slots  (begin/end x 2 passes)
//    - PIPELINE_STATISTICS  : 2 slots  (one per pass,
//                             CLIPPING_INVOCATIONS only)
//
//  Usage per frame:
//    1. BeginFrame(cb, frameIndex)
//    2. Call the Begin*/End* helpers around each render pass.
//    3. EndFrame(cb, frameIndex)  — writes the results.
//    4. After queue submit + GPU work is done for (frameIndex - 1),
//       call ReadbackAndPrint(frameIndex).
// ---------------------------------------------------------
class QueryManager
{
public:
    // Timestamp slots
    static constexpr uint32_t kTsCullingBegin  = 0;
    static constexpr uint32_t kTsCullingEnd    = 1;
    static constexpr uint32_t kTsGBufferBegin  = 2;
    static constexpr uint32_t kTsGBufferEnd    = 3;
    static constexpr uint32_t kTsLightingBegin = 4;
    static constexpr uint32_t kTsLightingEnd   = 5;
    static constexpr uint32_t kTimestampCount  = 6;

    // Pipeline-statistics slots
    static constexpr uint32_t kStatGBufferPass = 0;
    static constexpr uint32_t kStatCount       = 1;  // lighting pass not measured

    QueryManager() = default;
    ~QueryManager() = default;

    bool Initialize(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t framesInFlight);
    void Destroy(VkDevice device);

    // Call once at the start of command buffer recording for this frame.
    void BeginFrame(VkCommandBuffer cb, uint32_t frameIndex);

    // Wrap around the culling compute dispatch and render passes.
    void BeginCullingPass  (VkCommandBuffer cb, uint32_t frameIndex);
    void EndCullingPass    (VkCommandBuffer cb, uint32_t frameIndex);
    void BeginGBufferPass  (VkCommandBuffer cb, uint32_t frameIndex);
    void EndGBufferPass    (VkCommandBuffer cb, uint32_t frameIndex);
    void BeginLightingPass (VkCommandBuffer cb, uint32_t frameIndex);
    void EndLightingPass   (VkCommandBuffer cb, uint32_t frameIndex);

    // Read back results for the given frame slot (safe to call once that
    // frame's GPU work has retired) and print to stdout.
    void ReadbackAndPrint(VkDevice device, uint32_t frameIndex);

private:
    std::vector<VkQueryPool> m_TimestampPools;   // [framesInFlight]
    std::vector<VkQueryPool> m_StatsPools;       // [framesInFlight]

    float    m_TimestampPeriodMs = 0.0f;  // nanoseconds-per-tick -> ms conversion
    uint32_t m_FramesInFlight    = 0;
    bool     m_TimestampsSupported = true;
};
