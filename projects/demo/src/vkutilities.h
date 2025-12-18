#include "Engine.h"

#include <glm/glm.hpp>

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
        float pad0;
        float pad1;
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

    struct SceneData
    {
        glm::mat4 cameraTransform;
        glm::mat4 projection;
        glm::vec3 lightPos;
    };

    struct DrawData
    {
        glm::mat4 transform;
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

        GlobalUniforms* pGlobalUniforms;
        RenderingDescriptors* pRenderingDescriptors;
    };

    VkResult CreateShaderModule(VkDevice device, const uint32_t* source, size_t codeSize, VkShaderModule& shader);

    VkResult CreateImage(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, Image& image);

    VkResult CreateFramebuffer(VkDevice device, VkRenderPass rp, uint32_t attachmentCount, const VkImageView* pAttachments, uint32_t width, uint32_t height, VkFramebuffer& framebuffer);

    VkResult CreateBuffer(VkPhysicalDevice pDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer);

    VkResult GenerateCubeMesh(VkPhysicalDevice pDevice, VkDevice device, Buffer& vertexBuffer
        , Buffer& indexBuffer, uint32_t& indexCount, imp::Engine& engine);

    VkResult SetupGlobalUniforms(imp::Engine& engine, GlobalUniforms& globals);
    void InitializeSceneData(imp::Engine& engine, SceneData& scene);
    void UpdateCamera(imp::Window& window, SceneData& scene, GlobalUniformsData& globalsData, double delta);
    void UpdateGlobalDataDescriptorSetByCopy(imp::Engine& engine, const GlobalUniforms& globals, VkCommandBuffer cb);
    VkResult SetupRenderingDescriptorSet(imp::Engine& engine, RenderingDescriptors& data, Buffer& vertexBuffer
        , Buffer& indexBuffer, uint64_t meshCount);

    VkResult CreatePhongPipeline(VkDevice device, VkShaderModule vertModule, VkShaderModule fragModule, PhongPipeline& pipeline);

    void UpdateRenderingDataDescriptorSetByCopy(imp::Engine& engine, const RenderingDescriptors& renderingData, VkCommandBuffer cb, const std::vector<DrawData>& drawData);
}