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
    struct Image
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
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
    };
    static_assert(sizeof(Vertex) == 32);

    struct GlobalUniformsData
    {
        glm::mat4 viewProj;
        glm::vec3 lightPos;
        float padding1;
        glm::vec3 viewPos;
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
    };

    struct SceneData
    {
        glm::mat4 cameraTransform;
        glm::mat4 projection;
        glm::vec3 lightPos;

        std::vector<DrawData> drawDatas;
    };

    struct RenderingDescriptors
    {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        Buffer drawDataBuffer;
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

    VkResult CreateShaderModule(VkDevice device, const uint32_t* source, size_t codeSize, VkShaderModule& shader);

    VkResult CreateImage(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, Image& image);
VkResult CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& imageView);

    VkResult CreateFramebuffer(VkDevice device, VkRenderPass rp, uint32_t attachmentCount, const VkImageView* pAttachments, uint32_t width, uint32_t height, VkFramebuffer& framebuffer);

    VkResult CreateBuffer(VkPhysicalDevice pDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer);

    VkResult SetupGlobalUniforms(imp::Engine& engine, GlobalUniforms& globals);
    void InitializeSceneData(imp::Engine& engine, SceneData& scene, SceneLoader::Scene& scenel);
    void UpdateCamera(imp::Window& window, SceneData& scene, GlobalUniformsData& globalsData, double delta);
    void UpdateGlobalDataDescriptorSetByCopy(imp::Engine& engine, const GlobalUniforms& globals, VkCommandBuffer cb);
    VkResult SetupRenderingDescriptorSet(imp::Engine& engine, RenderingDescriptors& data, SceneLoader::Scene& scenel);

    VkResult CreatePhongPipeline(VkDevice device, VkShaderModule vertModule, VkShaderModule fragModule, PhongPipeline& pipeline);

    void UpdateRenderingDataDescriptorSetByCopy(imp::Engine& engine, const RenderingDescriptors& renderingData, VkCommandBuffer cb, const std::vector<DrawData>& drawData);

    void PaceFrame(VkDevice device, std::vector<imp::SubmitSync>& framePacingData, const imp::SubmitSync& currentFrameSync, uint32_t& frameIndex, uint32_t maxFramesInFlight, imp::SubmitSyncManager& submitSyncManager);

    void InsertPipelineBarrier(VkCommandBuffer cb, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
        , VkAccessFlags srcAccess, VkAccessFlags dstAccess);

    void InsertPipelineBarrier2(VkCommandBuffer cb, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage
        , VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess);
}