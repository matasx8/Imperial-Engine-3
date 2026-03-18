#pragma once
#include "Engine.h"

#include <glm/glm.hpp>
#include <vector>

namespace SceneLoader
{
    struct Scene;
}

namespace VU
{
    inline void SetDebugName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name)
    {
        if (!name || !device) return;
        
        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = objectType;
        nameInfo.objectHandle = objectHandle;
        nameInfo.pObjectName = name;
        
        vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
    }

    inline void CmdBeginLabel(VkCommandBuffer cb, const char* name, float r = 0.5f, float g = 0.5f, float b = 0.5f)
    {
        VkDebugUtilsLabelEXT label {};
        label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        label.color[0]   = r;
        label.color[1]   = g;
        label.color[2]   = b;
        label.color[3]   = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(cb, &label);
    }

    inline void CmdEndLabel(VkCommandBuffer cb)
    {
        vkCmdEndDebugUtilsLabelEXT(cb);
    }

    struct Image
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
    };

    struct Buffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normals;
        glm::vec2 uvs;
        glm::vec4 tangent;  // w = handedness sign
    };
    static_assert(sizeof(Vertex) == 48);

    struct GlobalUniformsData
    {
        glm::mat4 viewProj;
        glm::mat4 invViewProj;   // used in lighting pass to reconstruct world pos from depth
        glm::vec3 cameraPos;
        float padding1;
        glm::vec3 lightDir;  // normalized, points from surface toward light
        float padding2;
        // Frustum planes extracted on the CPU each frame (Gribb-Hartmann, Vulkan [0,1] depth).
        // Stored here so the culling compute shader reads them from the globals UBO instead
        // of re-extracting them per thread.
        glm::vec4 frustumPlanes[6];
    };

    struct GlobalUniforms
    {
        // One UBO and one descriptor set per frame-in-flight so the CPU can write the next
        // frame's data while the GPU is still consuming the previous frame's UBO.
        std::vector<Buffer>          ubos;
        VkDescriptorSetLayout        descriptorSetLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> descriptorSets;
        GlobalUniformsData           data;  // CPU-side mirror; written to ubos[frameIndex] each frame
    };

    struct DrawData
    {
        glm::mat4 transform;
        uint32_t materialIdx;
        uint32_t _pad[3];  // std430: struct stride rounds up to mat4's 16-byte alignment → 80 bytes
    };
    static_assert(sizeof(DrawData) == 80);

    // Per-mesh bounding sphere in local (object) space.
    // The culling shader transforms the center by DrawData.transform and scales
    // the radius by the maximum column length to get a world-space sphere.
    // GPU layout: vec4 (xyz = center, w = radius) — 16 bytes, vec4-aligned.
    struct BoundingSphere
    {
        glm::vec3 center;
        float radius;
    };
    static_assert(sizeof(BoundingSphere) == 16);

    struct SceneData
    {
        glm::mat4 cameraTransform;
        glm::mat4 projection;
        glm::vec3 lightDir;  // normalized direction toward light

        std::vector<DrawData> drawDatas;
    };

    struct RenderingDescriptors
    {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        Buffer drawDataBuffer;
        Buffer materialBuffer;
        Buffer boundingSphereBuffer;  // BoundingSphere[] — binding 4, compute stage
        Buffer drawIndexBuffer;       // uint[] — binding 5, maps gl_DrawIDARB → original draw data index
    };

    // Pre-generated indirect draw buffer for GPU-driven rendering.
    // One VkDrawIndexedIndirectCommand per mesh, written once at startup.
    struct IndirectDrawBuffer
    {
        Buffer commands;        // VkDrawIndexedIndirectCommand[]
        Buffer countBuffer;     // single uint32_t = number of draws
        uint32_t maxDrawCount = 0;
    };

    VkResult BuildIndirectDrawBuffer(imp::Engine& engine, const SceneLoader::Scene& scenel, IndirectDrawBuffer& out);

    // Per-frame push constants for the culling compute shader.
    struct CullingPushConstants
    {
        uint32_t totalDraws;  // 4 bytes
    };

    // Culling compute pipeline.
    //
    // Each frame:
    //   1. DispatchCulling fills culledBuffer with only the visible draws
    //      and drawIndexBuffer with the original mesh index for each visible draw.
    //   2. The geometry pass reads from culledBuffer instead of the master buffer.
    //      The vertex shader reads drawIndexBuffer[gl_DrawIDARB] to index into DrawData[].
    struct CullingPipeline
    {
        VkPipeline            pipeline            = VK_NULL_HANDLE;
        VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;
        IndirectDrawBuffer    culledBuffer;        // output: compacted draws written by compute
        GlobalUniforms*       pGlobalUniforms     = nullptr;
    };

    VkResult CreateCullingPipeline(imp::Engine& engine, VkShaderModule compModule,
        const IndirectDrawBuffer& srcDrawBuffer,
        const RenderingDescriptors& renderingDescriptors,
        GlobalUniforms& globals,
        CullingPipeline& out);

    // Records: zero count → dispatch → barrier into cb.
    // Must be called before the geometry pass each frame.
    void DispatchCulling(VkCommandBuffer cb, const CullingPipeline& pipeline, uint32_t totalDraws, uint32_t frameIndex);

    struct PhongPipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkRenderPass renderPass;
        Image depthImage;
        VkImageView depthImageView;

        GlobalUniforms* pGlobalUniforms;
        RenderingDescriptors* pRenderingDescriptors;
    };

    // Deferred G-Buffer
    //
    //  Attachment 0 - albedoMetallic   R8G8B8A8_UNORM    (RGB = albedo, A = metallic)
    //  Attachment 1 - normalRoughness  R16G16B16A16_SFLOAT (XYZ = world normal, W = roughness)
    //  Attachment 2 - depth            D32_SFLOAT          (world pos reconstructed via invViewProj)
    //  Lighting output - swapchain image, no separate allocation
    // -------------------------------------------------------------------------
    struct GBuffer
    {
        static constexpr VkFormat kAlbedoMetallicFormat  = VK_FORMAT_R8G8B8A8_UNORM;
        static constexpr VkFormat kNormalRoughnessFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        static constexpr VkFormat kDepthFormat           = VK_FORMAT_D32_SFLOAT;

        Image albedoMetallic;   // binding 0 in the lighting pass
        Image normalRoughness;  // binding 1 in the lighting pass
        Image depth;            // binding 2 in the lighting pass
    };

    // Geometry pass: writes all meshes into the G-Buffer.
    // One render pass, one framebuffer (swapchain-independent).
    struct GBufferPipeline
    {
        VkPipeline       pipeline       = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkRenderPass     renderPass     = VK_NULL_HANDLE;
        VkFramebuffer    framebuffer    = VK_NULL_HANDLE;

        GBuffer gbuffer;

        GlobalUniforms*      pGlobalUniforms       = nullptr;
        RenderingDescriptors* pRenderingDescriptors = nullptr;
    };

    // Lighting pass: full-screen triangle, reads G-Buffer, writes to swapchain.
    // One render pass, one framebuffer per swapchain image.
    struct LightingPipeline
    {
        VkPipeline       pipeline       = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkRenderPass     renderPass     = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> framebuffers;  // [swapchain image index]

        // Descriptor set: G-Buffer attachments as combined image samplers.
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;
        VkSampler             gbufferSampler      = VK_NULL_HANDLE; // nearest, clamp — GBuffer is 1:1 pixels

        GlobalUniforms* pGlobalUniforms = nullptr;
    };

    VkResult CreateShaderModule(VkDevice device, const uint32_t* source, size_t codeSize, VkShaderModule& shader, const char* debugName = nullptr);

    VkResult CreateImage(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, Image& image, const char* debugName = nullptr, uint32_t mipLevels = 1);
    VkResult CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& imageView, const char* debugName = nullptr, uint32_t mipLevels = 1);

    VkResult CreateFramebuffer(VkDevice device, VkRenderPass rp, uint32_t attachmentCount, const VkImageView* pAttachments, uint32_t width, uint32_t height, VkFramebuffer& framebuffer, const char* debugName = nullptr);

    VkResult CreateBuffer(VkPhysicalDevice pDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer, const char* debugName = nullptr);

    VkResult SetupGlobalUniforms(imp::Engine& engine, GlobalUniforms& globals, uint32_t framesInFlight);
    void InitializeSceneData(imp::Engine& engine, SceneData& scene, SceneLoader::Scene& scenel);
    void UpdateCamera(imp::Window& window, SceneData& scene, GlobalUniformsData& globalsData, double delta);
    // Uploads globals.data to the UBO slot for this frame.  Safe with multiple frames in flight
    // because each slot is an independent HOST_VISIBLE buffer.
    void UpdateGlobalDataDescriptorSetByCopy(imp::Engine& engine, const GlobalUniforms& globals, uint32_t frameIndex);
    // drawDatas must be the CPU-side mirror built by InitializeSceneData; uploaded once to DEVICE_LOCAL memory.
    VkResult SetupRenderingDescriptorSet(imp::Engine& engine, RenderingDescriptors& data,
        SceneLoader::Scene& scenel, const std::vector<DrawData>& drawDatas);

    // Stage 1: G-Buffer image allocation
    // Allocates the three G-Buffer images at the given resolution. Call once at startup; call again on resize.
    VkResult CreateGBuffer(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, GBuffer& gbuffer);

    // Stage 3: pipeline creation (also allocates G-Buffer images and all framebuffers)
    VkResult CreateGBufferPipeline(imp::Engine& engine, VkShaderModule vertModule, VkShaderModule fragModule, GBufferPipeline& pipeline, uint32_t width, uint32_t height);
    VkResult CreateLightingPipeline(imp::Engine& engine, VkShaderModule vertModule, VkShaderModule fragModule, LightingPipeline& pipeline, const GBuffer& gbuffer, uint32_t width, uint32_t height);

    // Legacy (replaced in Stage 3)
    VkResult CreatePhongPipeline(VkDevice device, VkShaderModule vertModule, VkShaderModule fragModule, PhongPipeline& pipeline);

    // Partial re-upload for dirty draw data entries (e.g. animated meshes).
    // Pass the indices of the changed DrawData slots; a device-local staging copy is submitted.
    // (Not yet implemented — placeholder for future dirty-tracking support.)

    void PaceFrame(VkDevice device, std::vector<imp::SubmitSync>& framePacingData, const imp::SubmitSync& currentFrameSync, uint32_t& frameIndex, uint32_t maxFramesInFlight, imp::SubmitSyncManager& submitSyncManager);

    void InsertPipelineBarrier(VkCommandBuffer cb, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
        , VkAccessFlags srcAccess, VkAccessFlags dstAccess);

    void InsertPipelineBarrier2(VkCommandBuffer cb, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage
        , VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess);
}