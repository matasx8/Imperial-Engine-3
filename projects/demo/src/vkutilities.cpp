#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "vkutilities.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <array>

namespace VU
{
    VkResult CreateShaderModule(VkDevice device, const uint32_t* source, size_t codeSize, VkShaderModule& shader)
    {
        VkShaderModuleCreateInfo smci {};
        smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = codeSize;
        smci.pCode = source;
        
        return vkCreateShaderModule(device, &smci, nullptr, &shader);
    }

    VkResult CreateImage(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, Image& image)
    {
        VkImageCreateInfo ici {};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent.width = width;
        ici.extent.height = height;
        ici.extent.depth = 1;
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.format = format;
        ici.tiling = tiling;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = usage;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult result = vkCreateImage(device, &ici, nullptr, &image.image);
        if (result != VK_SUCCESS)
            return result;

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image.image, &memRequirements);

        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(pDevice, &memProperties);

        bool memTypeFound = false;
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((memRequirements.memoryTypeBits & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                allocInfo.memoryTypeIndex = i;
                memTypeFound = true;
                break;
            }
        }

        if (!memTypeFound)
            return VK_ERROR_MEMORY_MAP_FAILED;

        result = vkAllocateMemory(device, &allocInfo, nullptr, &image.memory);
        if (result != VK_SUCCESS)
            return result;

        vkBindImageMemory(device, image.image, image.memory, 0);
        return VK_SUCCESS;
    }

    VkResult CreateFramebuffer(VkDevice device, VkRenderPass rp, uint32_t attachmentCount, const VkImageView* pAttachments, uint32_t width, uint32_t height, VkFramebuffer& framebuffer)
    {
        VkFramebufferCreateInfo fbci {};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = rp;
        fbci.attachmentCount = attachmentCount;
        fbci.pAttachments = pAttachments;
        fbci.width = width;
        fbci.height = height;
        fbci.layers = 1;

        return vkCreateFramebuffer(device, &fbci, nullptr, &framebuffer);
    }

    VkResult CreateBuffer(VkPhysicalDevice pDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer)
    {
        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer.buffer);
        if (result != VK_SUCCESS)
            return result;

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer.buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(pDevice, &memProperties);

        bool memTypeFound = false;
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((memRequirements.memoryTypeBits & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                allocInfo.memoryTypeIndex = i;
                memTypeFound = true;
                break;
            }
        }

        if (!memTypeFound)
            return VK_ERROR_MEMORY_MAP_FAILED;

        result = vkAllocateMemory(device, &allocInfo, nullptr, &buffer.memory);
        if (result != VK_SUCCESS)
            return result;

        vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);
        return VK_SUCCESS;
    }

    VkResult GenerateCubeMesh(VkPhysicalDevice pDevice, VkDevice device, Buffer& vertexBuffer
        , Buffer& indexBuffer, uint32_t& indexCount, imp::Engine& engine)
    {
        // Define cube vertices with positions and normals
        std::vector<Vertex> vertices = {
        // +X face
            {{ 0.5f, -0.5f, -0.5f}, { 1,  0,  0}, 0.0f, 0.0f},
            {{ 0.5f,  0.5f, -0.5f}, { 1,  0,  0}, 0.0f, 0.0f},
            {{ 0.5f,  0.5f,  0.5f}, { 1,  0,  0}, 0.0f, 0.0f},
            {{ 0.5f, -0.5f,  0.5f}, { 1,  0,  0}, 0.0f, 0.0f},

            // -X face
            {{-0.5f, -0.5f,  0.5f}, {-1,  0,  0}, 0.0f, 0.0f},
            {{-0.5f,  0.5f,  0.5f}, {-1,  0,  0}, 0.0f, 0.0f},
            {{-0.5f,  0.5f, -0.5f}, {-1,  0,  0}, 0.0f, 0.0f},
            {{-0.5f, -0.5f, -0.5f}, {-1,  0,  0}, 0.0f, 0.0f},

            // +Y face
            {{-0.5f,  0.5f, -0.5f}, { 0,  1,  0}, 0.0f, 0.0f},
            {{-0.5f,  0.5f,  0.5f}, { 0,  1,  0}, 0.0f, 0.0f},
            {{ 0.5f,  0.5f,  0.5f}, { 0,  1,  0}, 0.0f, 0.0f},
            {{ 0.5f,  0.5f, -0.5f}, { 0,  1,  0}, 0.0f, 0.0f},

            // -Y face
            {{-0.5f, -0.5f,  0.5f}, { 0, -1,  0}, 0.0f, 0.0f},
            {{-0.5f, -0.5f, -0.5f}, { 0, -1,  0}, 0.0f, 0.0f},
            {{ 0.5f, -0.5f, -0.5f}, { 0, -1,  0}, 0.0f, 0.0f},
            {{ 0.5f, -0.5f,  0.5f}, { 0, -1,  0}, 0.0f, 0.0f},

            // +Z face
            {{-0.5f, -0.5f,  0.5f}, { 0,  0,  1}, 0.0f, 0.0f},
            {{ 0.5f, -0.5f,  0.5f}, { 0,  0,  1}, 0.0f, 0.0f},
            {{ 0.5f,  0.5f,  0.5f}, { 0,  0,  1}, 0.0f, 0.0f},
            {{-0.5f,  0.5f,  0.5f}, { 0,  0,  1}, 0.0f, 0.0f},

            // -Z face
            {{ 0.5f, -0.5f, -0.5f}, { 0,  0, -1}, 0.0f, 0.0f},
            {{-0.5f, -0.5f, -0.5f}, { 0,  0, -1}, 0.0f, 0.0f},
            {{-0.5f,  0.5f, -0.5f}, { 0,  0, -1}, 0.0f, 0.0f},
            {{ 0.5f,  0.5f, -0.5f}, { 0,  0, -1}, 0.0f, 0.0f},
        };

        // Define cube indices
        const std::vector<uint32_t> indices = {
            0,  1,  2,   0,  2,  3,    // +X
            4,  5,  6,   4,  6,  7,    // -X
            8,  9, 10,   8, 10, 11,    // +Y
            12,13,14,   12,14,15,     // -Y
            16,17,18,   16,18,19,     // +Z
            20,21,22,   20,22,23      // -Z
        };

        indexCount = static_cast<uint32_t>(indices.size());

        // Create vertex staging buffer
        Buffer vertexStagingBuffer;
        VkResult result = CreateBuffer(pDevice, device,
                                        sizeof(Vertex) * vertices.size(),
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        vertexStagingBuffer);
        if (result != VK_SUCCESS)
            return result;

        // Map and copy vertex data
        void* vertexData;
        vkMapMemory(device, vertexStagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &vertexData);
        memcpy(vertexData, vertices.data(), sizeof(Vertex) * vertices.size());
        vkUnmapMemory(device, vertexStagingBuffer.memory);

        // Create index buffer
        Buffer indexStagingBuffer;
        result = CreateBuffer(pDevice, device,
                                sizeof(uint32_t) * indices.size(),
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        indexStagingBuffer);

        if (result != VK_SUCCESS)
            return result;

        // Map and copy index data
        void* indexData;
        vkMapMemory(device, indexStagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &indexData);
        memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
        vkUnmapMemory(device, indexStagingBuffer.memory);

        // Create device local vertex buffer
        result = CreateBuffer(pDevice, device,
                                sizeof(Vertex) * vertices.size(),
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                vertexBuffer);

        if (result != VK_SUCCESS)
            return result;

        // Create device local index buffer
        result = CreateBuffer(pDevice, device,
                                sizeof(uint32_t) * indices.size(),
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                indexBuffer);

        if (result != VK_SUCCESS)
            return result;

         VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);

        // Copy vertex data to device local buffer
        VkBufferCopy copyRegion {};
        copyRegion.size = sizeof(Vertex) * vertices.size();
        vkCmdCopyBuffer(cb, vertexStagingBuffer.buffer, vertexBuffer.buffer, 1, &copyRegion);

        // Copy index data to device local buffer
        copyRegion.size = sizeof(uint32_t) * indices.size();
        vkCmdCopyBuffer(cb, indexStagingBuffer.buffer, indexBuffer.buffer, 1, &copyRegion);

        // Submit copy commands
        vkEndCommandBuffer(cb);
        imp::SubmitParams submitParams {};
        submitParams.commandBufferCount = 1;
        submitParams.pCommandBuffers = &cb;
        submitParams.queue = engine.GetWorkQueue().GetGraphicsQueue();
        imp::SubmitSync sync = engine.Submit(&submitParams, 1);

        // Enqueue staging buffers for deferred destruction
        imp::SafeResourceDestroyer& destroyer = engine.GetSafeResourceDestroyer();
        imp::VulkanResource res {};
        res.buffer = vertexStagingBuffer.buffer;
        res.memory = vertexStagingBuffer.memory;
        res.type = imp::VulkanResourceType::Buffer;
        destroyer.EnqueueResourceForDestruction(res, sync.submit);
        res.buffer = indexStagingBuffer.buffer;
        res.memory = indexStagingBuffer.memory;
        destroyer.EnqueueResourceForDestruction(res, sync.submit);

        return VK_SUCCESS;
    }

     VkResult SetupGlobalUniforms(imp::Engine& engine, GlobalUniforms& globals)
     {
        VkDevice device = engine.GetWorkQueue().GetDevice();

        Buffer uniformBuffer {};
        VkResult result = CreateBuffer(engine.GetPhysicalDevice(), device,
                                        sizeof(GlobalUniformsData),
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        uniformBuffer);

        if (result != VK_SUCCESS)
            return result;

        globals.ubo = uniformBuffer;

        VkDescriptorSetLayoutBinding binding {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo dslci {};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 1;
        dslci.pBindings = &binding;

        result = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &globals.descriptorSetLayout);

        if (result != VK_SUCCESS)
            return result;

        VkDescriptorSetAllocateInfo dsai {};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = engine.GetDescriptorPool();
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &globals.descriptorSetLayout;

        vkAllocateDescriptorSets(device, &dsai, &globals.descriptorSet);

        VkDescriptorBufferInfo bi {};
        bi.buffer = globals.ubo.buffer;
        bi.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = globals.descriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bi;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
     }

     void InitializeSceneData(imp::Engine& engine, SceneData& scene)
     {
        scene.lightPos = glm::vec3(2.0f, 2.0f, 2.0f);

        static constexpr float defaultCameraYRotationRad = 0.0f;
        scene.cameraTransform = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 15.0f)), defaultCameraYRotationRad, glm::vec3(0.0f, 1.0f, 0.0f));
        
        imp::Window& window = engine.GetPlatform().GetWindow();
        scene.projection = glm::perspective(glm::radians(90.0f), (float)window.GetWidth() / (float)window.GetHeight(), 1.0f, 1000.0f);
    }

    void UpdateCamera(imp::Window& window, SceneData& scene, GlobalUniformsData& globalsData, double delta)
    {
        // Vulkan's fixed-function steps expect to look down -Z, Y is "down" and RH
        static constexpr glm::vec3 front(0.0f, 0.0f, -1.0f);// look down -z
        static constexpr glm::vec3 up(0.0f, -1.0f, 0.0f);

        const auto quat = glm::toQuat(scene.cameraTransform);
        const auto newFront = glm::rotate(quat, front);
        const auto newUp = glm::rotate(quat, up);

        window.MoveCamera(scene.cameraTransform, delta);

        // update view matrix
        const glm::vec3 pos = glm::vec3(scene.cameraTransform[3]);
        glm::mat4 view = glm::lookAtRH(pos, pos + newFront, newUp);

        globalsData.viewPos = pos;
        globalsData.viewProj = scene.projection * view;

    }

    void UpdateGlobalDataDescriptorSetByCopy(imp::Engine& engine, const GlobalUniforms& globals, VkCommandBuffer cb)
    {
        Buffer stagingBuffer {};
        CreateBuffer(engine.GetPhysicalDevice(), engine.GetWorkQueue().GetDevice(),
                                        sizeof(GlobalUniformsData),
                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        stagingBuffer);

        void* data;
        vkMapMemory(engine.GetWorkQueue().GetDevice(), stagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, &globals.data, sizeof(GlobalUniformsData));
        vkUnmapMemory(engine.GetWorkQueue().GetDevice(), stagingBuffer.memory);

        VkBufferCopy copyRegion {};
        copyRegion.size = sizeof(GlobalUniformsData);
        vkCmdCopyBuffer(cb, stagingBuffer.buffer, globals.ubo.buffer, 1, &copyRegion);

        VkMemoryBarrier memoryBarrier {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;

        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr);
    }

    void UpdateRenderingDataDescriptorSetByCopy(imp::Engine& engine, const RenderingDescriptors& renderingData, VkCommandBuffer cb, const std::vector<DrawData>& drawData)
    {
        Buffer stagingBuffer {};
        CreateBuffer(engine.GetPhysicalDevice(), engine.GetWorkQueue().GetDevice(),
                                        sizeof(DrawData) * drawData.size(),
                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        stagingBuffer);

        void* data;
        vkMapMemory(engine.GetWorkQueue().GetDevice(), stagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &data);
        memcpy(data, drawData.data(), sizeof(DrawData) * drawData.size());
        vkUnmapMemory(engine.GetWorkQueue().GetDevice(), stagingBuffer.memory);

        VkBufferCopy copyRegion {};
        copyRegion.size = sizeof(DrawData) * drawData.size();
        vkCmdCopyBuffer(cb, stagingBuffer.buffer, renderingData.drawDataBuffer.buffer, 1, &copyRegion);
    }

    VkResult SetupRenderingDescriptorSet(imp::Engine& engine, RenderingDescriptors& data, Buffer& vertexBuffer
        , Buffer& indexBuffer, uint64_t meshCount)
    {
        VkDevice device = engine.GetWorkQueue().GetDevice();

        Buffer drawDataBuffer {};
        VkResult result = CreateBuffer(engine.GetPhysicalDevice(), device,
                                        sizeof(DrawData) * meshCount,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        drawDataBuffer);

        if (result != VK_SUCCESS)
            return result;

        data.drawDataBuffer = drawDataBuffer;
        
        std::array<VkDescriptorSetLayoutBinding, 3> bindings {};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        bindings[1] = bindings[0];
        bindings[1].binding = 1;
        bindings[2] = bindings[0];
        bindings[2].binding = 2;

        VkDescriptorSetLayoutCreateInfo dslci {};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = bindings.size();
        dslci.pBindings = bindings.data();

        result = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &data.descriptorSetLayout);

        if (result != VK_SUCCESS)
            return result;

        VkDescriptorSetAllocateInfo dsai {};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = engine.GetDescriptorPool();
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &data.descriptorSetLayout;

        vkAllocateDescriptorSets(device, &dsai, &data.descriptorSet);

        std::array<VkDescriptorBufferInfo, 3> bi {};
        bi[0].buffer = vertexBuffer.buffer;
        bi[0].range = VK_WHOLE_SIZE;
        bi[1].buffer = indexBuffer.buffer;
        bi[1].range = VK_WHOLE_SIZE;
        bi[2].buffer = drawDataBuffer.buffer;
        bi[2].range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet write {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = data.descriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 3;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = bi.data();

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
        
    VkResult CreatePhongPipeline(VkDevice device, VkShaderModule vertModule, VkShaderModule fragModule
        , PhongPipeline& pipeline)
    {
        VkPipelineShaderStageCreateInfo vertStageInfo {};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertModule;
        vertStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo {};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragModule;
        fragStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

        //VkPushConstantRange pushConstantRange {};
        //pushConstantRange.size = sizeof(PushConstants);
        //pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array<VkDescriptorSetLayout, 2> setLayouts = {
            pipeline.pGlobalUniforms->descriptorSetLayout,
            pipeline.pRenderingDescriptors->descriptorSetLayout
        };

        VkPipelineLayoutCreateInfo plci {};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 2;
        plci.pSetLayouts = setLayouts.data();

        //plci.pushConstantRangeCount = 0;
        //plci.pPushConstantRanges = &pushConstantRange;

        VkResult result = vkCreatePipelineLayout(device, &plci, nullptr, &pipeline.pipelineLayout);
        if (result != VK_SUCCESS)
            return result;

        VkPipelineVertexInputStateCreateInfo pvisi {};
        pvisi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo piasi {};
        piasi.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        piasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo pvsi {};
        pvsi.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pvsi.viewportCount = 1;
        pvsi.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo prsi {};
        prsi.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        prsi.polygonMode = VK_POLYGON_MODE_FILL;
        prsi.cullMode = VK_CULL_MODE_BACK_BIT;
        prsi.frontFace = VK_FRONT_FACE_CLOCKWISE;
        prsi.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo pmsi {};
        pmsi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pmsi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState pcbas {};
        pcbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo pcbsci {};
        pcbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pcbsci.attachmentCount = 1;
        pcbsci.pAttachments = &pcbas;

        VkPipelineDepthStencilStateCreateInfo pdsci {};
        pdsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo pdsci2 {};
        pdsci2.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pdsci2.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        pdsci2.pDynamicStates = dynamicStates.data();

        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;

        std::array<VkAttachmentDescription, 1> colorAttachmentDescs {};
        colorAttachmentDescs[0].format = colorFormat;
        colorAttachmentDescs[0].samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        std::array<VkAttachmentReference, 1> colorAttachmentRefs {};
        colorAttachmentRefs[0].attachment = 0;
        colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc {};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
        subpassDesc.pColorAttachments = colorAttachmentRefs.data();

        VkRenderPassCreateInfo prci {};
        prci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        prci.attachmentCount = static_cast<uint32_t>(colorAttachmentDescs.size());
        prci.pAttachments = colorAttachmentDescs.data();
        prci.subpassCount = 1;
        prci.pSubpasses = &subpassDesc;

        result = vkCreateRenderPass(device, &prci, nullptr, &pipeline.renderPass);
        if (result != VK_SUCCESS)
            return result;

        VkGraphicsPipelineCreateInfo gpci {};
        gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount = 2;
        gpci.pStages = shaderStages;
        gpci.pVertexInputState = &pvisi;
        gpci.pInputAssemblyState = &piasi;
        gpci.pViewportState = &pvsi;
        gpci.pRasterizationState = &prsi;
        gpci.pMultisampleState = &pmsi;
        gpci.pColorBlendState = &pcbsci;
        gpci.pDepthStencilState = &pdsci;
        gpci.pDynamicState = &pdsci2;
        gpci.layout = pipeline.pipelineLayout;
        gpci.renderPass = pipeline.renderPass;

        result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline.pipeline);
        return result;
    };
}