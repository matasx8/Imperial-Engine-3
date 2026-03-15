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
        glm::vec3 cameraPos;
        float padding1;
        glm::vec3 lightDir;  // normalized, points from surface toward light
        float padding2;
    };

    struct GlobalUniforms
    {
        Buffer ubo;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        GlobalUniformsData data;
    };

    struct DrawData
    {
        glm::mat4 transform;
        uint32_t materialIdx;
        uint32_t _pad[3];  // std430: struct stride rounds up to mat4's 16-byte alignment → 80 bytes
    };
    static_assert(sizeof(DrawData) == 80);

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
    };

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

    VkResult CreateShaderModule(VkDevice device, const uint32_t* source, size_t codeSize, VkShaderModule& shader, const char* debugName = nullptr);

    VkResult CreateImage(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, Image& image, const char* debugName = nullptr, uint32_t mipLevels = 1);
    VkResult CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& imageView, const char* debugName = nullptr, uint32_t mipLevels = 1);

    VkResult CreateFramebuffer(VkDevice device, VkRenderPass rp, uint32_t attachmentCount, const VkImageView* pAttachments, uint32_t width, uint32_t height, VkFramebuffer& framebuffer, const char* debugName = nullptr);

    VkResult CreateBuffer(VkPhysicalDevice pDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer, const char* debugName = nullptr);

    VkResult SetupGlobalUniforms(imp::Engine& engine, GlobalUniforms& globals);
    void InitializeSceneData(imp::Engine& engine, SceneData& scene, SceneLoader::Scene& scenel);
    void UpdateCamera(imp::Window& window, SceneData& scene, GlobalUniformsData& globalsData, double delta);
    void UpdateGlobalDataDescriptorSetByCopy(imp::Engine& engine, const GlobalUniforms& globals);
    VkResult SetupRenderingDescriptorSet(imp::Engine& engine, RenderingDescriptors& data, SceneLoader::Scene& scenel);

    VkResult CreatePhongPipeline(VkDevice device, VkShaderModule vertModule, VkShaderModule fragModule, PhongPipeline& pipeline);

    void UpdateRenderingDataDescriptorSetByCopy(imp::Engine& engine, const RenderingDescriptors& renderingData, const std::vector<DrawData>& drawData);

    void PaceFrame(VkDevice device, std::vector<imp::SubmitSync>& framePacingData, const imp::SubmitSync& currentFrameSync, uint32_t& frameIndex, uint32_t maxFramesInFlight, imp::SubmitSyncManager& submitSyncManager);

    void InsertPipelineBarrier(VkCommandBuffer cb, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
        , VkAccessFlags srcAccess, VkAccessFlags dstAccess);

    void InsertPipelineBarrier2(VkCommandBuffer cb, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage
        , VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess);
}