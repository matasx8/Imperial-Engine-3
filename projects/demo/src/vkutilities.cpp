#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "vkutilities.h"
#include "SceneLoader.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <array>
#include <unordered_map>
#include <cstdio>

namespace VU
{
    VkResult CreateShaderModule(VkDevice device, const uint32_t* source, size_t codeSize, VkShaderModule& shader, const char* debugName)
    {
        VkShaderModuleCreateInfo smci {};
        smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = codeSize;
        smci.pCode = source;
        
        VkResult result = vkCreateShaderModule(device, &smci, nullptr, &shader);
        if (result == VK_SUCCESS && debugName)
            SetDebugName(device, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)shader, debugName);
        return result;
    }

    VkResult CreateImage(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, Image& image, const char* debugName, uint32_t mipLevels)
    {
        VkImageCreateInfo ici {};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent.width = width;
        ici.extent.height = height;
        ici.extent.depth = 1;
        ici.mipLevels = mipLevels;
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
        
        if (debugName)
        {
            SetDebugName(device, VK_OBJECT_TYPE_IMAGE, (uint64_t)image.image, debugName);
            char memName[128];
            snprintf(memName, sizeof(memName), "%s_mem", debugName);
            SetDebugName(device, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)image.memory, memName);
        }
        
        return VK_SUCCESS;
    }

    VkResult CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& imageView, const char* debugName, uint32_t mipLevels)
    {
        VkImageViewCreateInfo ivci {};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = image;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = format;
        ivci.subresourceRange.aspectMask = aspectFlags;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = mipLevels;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device, &ivci, nullptr, &imageView);
        if (result == VK_SUCCESS && debugName)
            SetDebugName(device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)imageView, debugName);
        return result;
    }

    VkResult CreateFramebuffer(VkDevice device, VkRenderPass rp, uint32_t attachmentCount, const VkImageView* pAttachments, uint32_t width, uint32_t height, VkFramebuffer& framebuffer, const char* debugName)
    {
        VkFramebufferCreateInfo fbci {};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = rp;
        fbci.attachmentCount = attachmentCount;
        fbci.pAttachments = pAttachments;
        fbci.width = width;
        fbci.height = height;
        fbci.layers = 1;

        VkResult result = vkCreateFramebuffer(device, &fbci, nullptr, &framebuffer);
        if (result == VK_SUCCESS && debugName)
            SetDebugName(device, VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)framebuffer, debugName);
        return result;
    }

    VkResult CreateBuffer(VkPhysicalDevice pDevice, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer, const char* debugName)
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

        if (debugName)
        {
            SetDebugName(device, VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer.buffer, debugName);
            char memName[128];
            snprintf(memName, sizeof(memName), "%s_mem", debugName);
            SetDebugName(device, VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)buffer.memory, memName);
        }
        return VK_SUCCESS;
    }

     VkResult SetupGlobalUniforms(imp::Engine& engine, GlobalUniforms& globals)
     {
        VkDevice device = engine.GetWorkQueue().GetDevice();

        VkResult result = CreateBuffer(engine.GetPhysicalDevice(), device,
                                        sizeof(GlobalUniformsData),
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        globals.ubo, "globals_ubo");

        VkDescriptorSetLayoutBinding binding {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo dslci {};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 1;
        dslci.pBindings = &binding;

        result = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &globals.descriptorSetLayout);

        if (result != VK_SUCCESS)
            return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)globals.descriptorSetLayout, "globals_dsl");

        VkDescriptorSetAllocateInfo dsai {};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = engine.GetDescriptorPool();
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &globals.descriptorSetLayout;

        vkAllocateDescriptorSets(device, &dsai, &globals.descriptorSet);
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)globals.descriptorSet, "globals_ds");

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
        return VK_SUCCESS;
     }

     void InitializeSceneData(imp::Engine& engine, SceneData& scene, SceneLoader::Scene& scenel)
     {
        scene.lightDir = glm::normalize(glm::vec3(1.0f, 2.0f, 1.0f));

        if (scenel.cameraWasLoaded)
            scene.cameraTransform = scenel.camera.Model;
        else
            scene.cameraTransform = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 15.0f)), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        
        imp::Window& window = engine.GetPlatform().GetWindow();
        scene.projection = glm::perspective(glm::radians(90.0f), (float)window.GetWidth() / (float)window.GetHeight(), 1.0f, 1000.0f);

        std::unordered_map<uint32_t, const SceneLoader::Entity*> meshToEntity;
        for (const auto& entity : scenel.entities)
            meshToEntity.emplace(entity.meshId, &entity);

        for (const auto& mesh : scenel.meshes)
        {
            DrawData drawData {};
            auto it = meshToEntity.find(mesh.id);
            if (it != meshToEntity.end())
            {
                const auto& entity = *it->second;
                drawData.transform = scenel.transforms[entity.transformId];
                drawData.materialIdx = entity.materialId < scenel.materials.size() ? entity.materialId : 0;
            }
            scene.drawDatas.push_back(drawData);
        }
    }

    void UpdateCamera(imp::Window& window, SceneData& scene, GlobalUniformsData& globalsData, double delta)
    {
        static constexpr glm::vec3 front(0.0f, 0.0f, -1.0f);
        static constexpr glm::vec3 up(0.0f, -1.0f, 0.0f);

        const auto quat = glm::toQuat(scene.cameraTransform);
        const auto newFront = glm::rotate(quat, front);
        const auto newUp = glm::rotate(quat, up);

        window.MoveCamera(scene.cameraTransform, delta);

        const glm::vec3 pos = glm::vec3(scene.cameraTransform[3]);
        glm::mat4 view = glm::lookAtRH(pos, pos + newFront, newUp);

        globalsData.cameraPos  = pos;
        globalsData.lightDir   = scene.lightDir;
        globalsData.viewProj   = scene.projection * view;
        globalsData.invViewProj = glm::inverse(globalsData.viewProj);
    }

    void UpdateGlobalDataDescriptorSetByCopy(imp::Engine& engine, const GlobalUniforms& globals)
    {
        void* data;
        vkMapMemory(engine.GetWorkQueue().GetDevice(), globals.ubo.memory, 0, sizeof(GlobalUniformsData), 0, &data);
        memcpy(data, &globals.data, sizeof(GlobalUniformsData));
        vkUnmapMemory(engine.GetWorkQueue().GetDevice(), globals.ubo.memory);
    }

    void UpdateRenderingDataDescriptorSetByCopy(imp::Engine& engine, const RenderingDescriptors& renderingData, const std::vector<DrawData>& drawData)
    {
        void* data;
        vkMapMemory(engine.GetWorkQueue().GetDevice(), renderingData.drawDataBuffer.memory, 0, sizeof(DrawData) * drawData.size(), 0, &data);
        memcpy(data, drawData.data(), sizeof(DrawData) * drawData.size());
        vkUnmapMemory(engine.GetWorkQueue().GetDevice(), renderingData.drawDataBuffer.memory);
    }

    VkResult SetupRenderingDescriptorSet(imp::Engine& engine, RenderingDescriptors& data, SceneLoader::Scene& scenel)
    {
        VkDevice device = engine.GetWorkQueue().GetDevice();

        const uint32_t meshCount = static_cast<uint32_t>(scenel.meshes.size());
        const uint32_t materialCount = std::max(static_cast<uint32_t>(scenel.materials.size()), 1u);
        const uint32_t textureCount = static_cast<uint32_t>(scenel.textures.size());

        VkResult result = CreateBuffer(engine.GetPhysicalDevice(), device,
                                        sizeof(DrawData) * meshCount,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        data.drawDataBuffer, "draw_data_buffer");
        if (result != VK_SUCCESS)
            return result;

        result = CreateBuffer(engine.GetPhysicalDevice(), device,
                                sizeof(SceneLoader::Material) * materialCount,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                data.materialBuffer, "material_buffer");
        if (result != VK_SUCCESS)
            return result;

        if (!scenel.materials.empty())
        {
            void* matData;
            vkMapMemory(device, data.materialBuffer.memory, 0, sizeof(SceneLoader::Material) * scenel.materials.size(), 0, &matData);
            memcpy(matData, scenel.materials.data(), sizeof(SceneLoader::Material) * scenel.materials.size());
            vkUnmapMemory(device, data.materialBuffer.memory);
        }

        std::array<VkDescriptorSetLayoutBinding, 4> bindings {};
        bindings[0] = { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr };
        bindings[1] = { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        bindings[2] = { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        bindings[3] = { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::max(textureCount, 1u), VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo dslci {};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = static_cast<uint32_t>(bindings.size());
        dslci.pBindings = bindings.data();

        result = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &data.descriptorSetLayout);
        if (result != VK_SUCCESS)
            return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)data.descriptorSetLayout, "rendering_dsl");

        VkDescriptorSetAllocateInfo dsai {};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = engine.GetDescriptorPool();
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &data.descriptorSetLayout;

        result = vkAllocateDescriptorSets(device, &dsai, &data.descriptorSet);
        if (result != VK_SUCCESS)
            return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)data.descriptorSet, "rendering_ds");

        std::array<VkDescriptorBufferInfo, 3> bi {};
        bi[0] = { scenel.vertexBuffer.buffer, 0, VK_WHOLE_SIZE };
        bi[1] = { data.drawDataBuffer.buffer, 0, VK_WHOLE_SIZE };
        bi[2] = { data.materialBuffer.buffer, 0, VK_WHOLE_SIZE };

        std::array<VkWriteDescriptorSet, 3> bufferWrites {};
        for (uint32_t i = 0; i < 3; i++)
        {
            bufferWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            bufferWrites[i].dstSet = data.descriptorSet;
            bufferWrites[i].dstBinding = i;
            bufferWrites[i].descriptorCount = 1;
            bufferWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bufferWrites[i].pBufferInfo = &bi[i];
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(bufferWrites.size()), bufferWrites.data(), 0, nullptr);

        if (!scenel.textures.empty())
        {
            VkSampler defaultSampler = VK_NULL_HANDLE;
            VkSamplerCreateInfo sci {};
            sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sci.magFilter = VK_FILTER_LINEAR;
            sci.minFilter = VK_FILTER_LINEAR;
            sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            vkCreateSampler(device, &sci, nullptr, &defaultSampler);
            SetDebugName(device, VK_OBJECT_TYPE_SAMPLER, (uint64_t)defaultSampler, "default_sampler");

            std::vector<VkDescriptorImageInfo> imageInfos;
            imageInfos.reserve(scenel.textures.size());
            const VkImageView fallbackView = scenel.images.empty() ? VK_NULL_HANDLE : scenel.images[0].image.imageView;
            for (const auto& texRef : scenel.textures)
            {
                VkDescriptorImageInfo ii {};
                ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                ii.imageView = (texRef.imageId != kInvalidId && texRef.imageId < static_cast<uint32_t>(scenel.images.size()))
                    ? scenel.images[texRef.imageId].image.imageView
                    : fallbackView;
                ii.sampler = texRef.samplerId != kInvalidId ? scenel.samplers[texRef.samplerId] : defaultSampler;
                imageInfos.push_back(ii);
            }

            VkWriteDescriptorSet imageWrite {};
            imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            imageWrite.dstSet = data.descriptorSet;
            imageWrite.dstBinding = 3;
            imageWrite.descriptorCount = static_cast<uint32_t>(imageInfos.size());
            imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            imageWrite.pImageInfo = imageInfos.data();

            vkUpdateDescriptorSets(device, 1, &imageWrite, 0, nullptr);
        }

        return VK_SUCCESS;
    }
        
    // Stage 1: G-Buffer image allocation
    //
    // Creates the three device-local images that make up the G-Buffer.
    // The lighting output (swapchain image) is NOT allocated here.
    VkResult CreateGBuffer(VkPhysicalDevice pDevice, VkDevice device, uint32_t width, uint32_t height, GBuffer& gbuffer)
    {
        // Attachment 0: albedo (RGB) + metallic (A)
        VkResult result = CreateImage(pDevice, device, width, height,
            GBuffer::kAlbedoMetallicFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            gbuffer.albedoMetallic, "gbuf_albedo_metallic");
        if (result != VK_SUCCESS) return result;

        result = CreateImageView(device, gbuffer.albedoMetallic.image,
            GBuffer::kAlbedoMetallicFormat, VK_IMAGE_ASPECT_COLOR_BIT,
            gbuffer.albedoMetallic.imageView, "gbuf_albedo_metallic_view");
        if (result != VK_SUCCESS) return result;

        // Attachment 1: world normal (XYZ) + roughness (W)
        result = CreateImage(pDevice, device, width, height,
            GBuffer::kNormalRoughnessFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            gbuffer.normalRoughness, "gbuf_normal_roughness");
        if (result != VK_SUCCESS) return result;

        result = CreateImageView(device, gbuffer.normalRoughness.image,
            GBuffer::kNormalRoughnessFormat, VK_IMAGE_ASPECT_COLOR_BIT,
            gbuffer.normalRoughness.imageView, "gbuf_normal_roughness_view");
        if (result != VK_SUCCESS) return result;

        // Attachment 2: depth (also sampled in lighting pass for world pos reconstruction)
        result = CreateImage(pDevice, device, width, height,
            GBuffer::kDepthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            gbuffer.depth, "gbuf_depth");
        if (result != VK_SUCCESS) return result;

        result = CreateImageView(device, gbuffer.depth.image,
            GBuffer::kDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT,
            gbuffer.depth.imageView, "gbuf_depth_view");
        return result;
    }

    VkResult CreateGBufferPipeline(imp::Engine& engine, VkShaderModule vertModule, VkShaderModule fragModule,
        GBufferPipeline& pipeline, uint32_t width, uint32_t height)
    {
        VkDevice         device  = engine.GetWorkQueue().GetDevice();
        VkPhysicalDevice pDevice = engine.GetPhysicalDevice();

        // Allocate G-Buffer images
        VkResult result = CreateGBuffer(pDevice, device, width, height, pipeline.gbuffer);
        if (result != VK_SUCCESS) return result;

        // Render pass: 2 color outputs + depth.
        // All start UNDEFINED (cleared on load) and end SHADER_READ_ONLY_OPTIMAL
        // so the lighting pass can sample them without an explicit barrier.
        std::array<VkAttachmentDescription, 3> attachments {};
        attachments[0].format         = GBuffer::kAlbedoMetallicFormat;
        attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[1].format         = GBuffer::kNormalRoughnessFormat;
        attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[2].format         = GBuffer::kDepthFormat;
        attachments[2].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[2].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[2].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[2].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkAttachmentReference, 2> colorRefs {};
        colorRefs[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        colorRefs[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthRef { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments       = colorRefs.data();
        subpass.pDepthStencilAttachment = &depthRef;

        // Ensure G-Buffer writes are visible to the lighting-pass fragment reads.
        VkSubpassDependency dep {};
        dep.srcSubpass    = 0;
        dep.dstSubpass    = VK_SUBPASS_EXTERNAL;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rpci {};
        rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = static_cast<uint32_t>(attachments.size());
        rpci.pAttachments    = attachments.data();
        rpci.subpassCount    = 1;
        rpci.pSubpasses      = &subpass;
        rpci.dependencyCount = 1;
        rpci.pDependencies   = &dep;

        result = vkCreateRenderPass(device, &rpci, nullptr, &pipeline.renderPass);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)pipeline.renderPass, "gbuffer_render_pass");

        // Framebuffer (swapchain-independent - uses only G-Buffer images)
        std::array<VkImageView, 3> fbViews = {
            pipeline.gbuffer.albedoMetallic.imageView,
            pipeline.gbuffer.normalRoughness.imageView,
            pipeline.gbuffer.depth.imageView
        };
        result = CreateFramebuffer(device, pipeline.renderPass,
            static_cast<uint32_t>(fbViews.size()), fbViews.data(),
            width, height, pipeline.framebuffer, "gbuffer_framebuffer");
        if (result != VK_SUCCESS) return result;

        // Pipeline layout: set 0 = globals UBO, set 1 = rendering descriptors, push constant = drawIndex + vertexOffset
        VkPushConstantRange pushRange {};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.size       = sizeof(uint32_t) * 2;

        std::array<VkDescriptorSetLayout, 2> setLayouts = {
            pipeline.pGlobalUniforms->descriptorSetLayout,
            pipeline.pRenderingDescriptors->descriptorSetLayout
        };

        VkPipelineLayoutCreateInfo plci {};
        plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
        plci.pSetLayouts            = setLayouts.data();
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges    = &pushRange;

        result = vkCreatePipelineLayout(device, &plci, nullptr, &pipeline.pipelineLayout);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)pipeline.pipelineLayout, "gbuffer_layout");

        VkPipelineShaderStageCreateInfo stages[2] {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo pvisi {};
        pvisi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo piasi {};
        piasi.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        piasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo pvsi {};
        pvsi.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pvsi.viewportCount = 1;
        pvsi.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo prsi {};
        prsi.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        prsi.polygonMode = VK_POLYGON_MODE_FILL;
        prsi.cullMode    = VK_CULL_MODE_BACK_BIT;
        prsi.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        prsi.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo pmsi {};
        pmsi.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pmsi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // One blend state per color output (albedoMetallic + normalRoughness)
        VkPipelineColorBlendAttachmentState blendAtt {};
        blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        std::array<VkPipelineColorBlendAttachmentState, 2> blendAtts = { blendAtt, blendAtt };

        VkPipelineColorBlendStateCreateInfo pcbsci {};
        pcbsci.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pcbsci.attachmentCount = static_cast<uint32_t>(blendAtts.size());
        pcbsci.pAttachments    = blendAtts.data();

        VkPipelineDepthStencilStateCreateInfo pdsci {};
        pdsci.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pdsci.depthTestEnable  = VK_TRUE;
        pdsci.depthWriteEnable = VK_TRUE;
        pdsci.depthCompareOp   = VK_COMPARE_OP_LESS;

        std::array<VkDynamicState, 2> dynStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo pdsi {};
        pdsi.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pdsi.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
        pdsi.pDynamicStates    = dynStates.data();

        VkGraphicsPipelineCreateInfo gpci {};
        gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount          = 2;
        gpci.pStages             = stages;
        gpci.pVertexInputState   = &pvisi;
        gpci.pInputAssemblyState = &piasi;
        gpci.pViewportState      = &pvsi;
        gpci.pRasterizationState = &prsi;
        gpci.pMultisampleState   = &pmsi;
        gpci.pColorBlendState    = &pcbsci;
        gpci.pDepthStencilState  = &pdsci;
        gpci.pDynamicState       = &pdsi;
        gpci.layout              = pipeline.pipelineLayout;
        gpci.renderPass          = pipeline.renderPass;

        result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline.pipeline);
        if (result == VK_SUCCESS)
            SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline.pipeline, "gbuffer_pipeline");
        return result;
    }

    VkResult CreateLightingPipeline(imp::Engine& engine, VkShaderModule vertModule, VkShaderModule fragModule,
        LightingPipeline& pipeline, const GBuffer& gbuffer, uint32_t width, uint32_t height)
    {
        VkDevice        device    = engine.GetWorkQueue().GetDevice();
        imp::Swapchain& swapchain = engine.GetPlatform().GetWindow().GetSwapchain();

        // Nearest/clamp sampler - G-Buffer texels map 1:1 to screen pixels
        VkSamplerCreateInfo sci {};
        sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter    = VK_FILTER_NEAREST;
        sci.minFilter    = VK_FILTER_NEAREST;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        VkResult result = vkCreateSampler(device, &sci, nullptr, &pipeline.gbufferSampler);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_SAMPLER, (uint64_t)pipeline.gbufferSampler, "gbuffer_sampler");

        // Descriptor set layout: bindings 0/1/2 = albedoMetallic / normalRoughness / depth
        std::array<VkDescriptorSetLayoutBinding, 3> bindings {};
        bindings[0] = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        bindings[1] = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        bindings[2] = { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo dslci {};
        dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = static_cast<uint32_t>(bindings.size());
        dslci.pBindings    = bindings.data();
        result = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &pipeline.descriptorSetLayout);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)pipeline.descriptorSetLayout, "lighting_dsl");

        VkDescriptorSetAllocateInfo dsai {};
        dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool     = engine.GetDescriptorPool();
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts        = &pipeline.descriptorSetLayout;
        result = vkAllocateDescriptorSets(device, &dsai, &pipeline.descriptorSet);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pipeline.descriptorSet, "lighting_ds");

        // Write G-Buffer image views into the descriptor set
        std::array<VkDescriptorImageInfo, 3> ii {};
        ii[0] = { pipeline.gbufferSampler, gbuffer.albedoMetallic.imageView,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        ii[1] = { pipeline.gbufferSampler, gbuffer.normalRoughness.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        ii[2] = { pipeline.gbufferSampler, gbuffer.depth.imageView,           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        std::array<VkWriteDescriptorSet, 3> writes {};
        for (uint32_t i = 0; i < 3; i++)
        {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = pipeline.descriptorSet;
            writes[i].dstBinding      = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].pImageInfo      = &ii[i];
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        // Render pass: single color attachment (swapchain), no depth
        VkAttachmentDescription colorAtt {};
        colorAtt.format        = swapchain.GetSurfaceFormat();
        colorAtt.samples       = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;

        // Wait for the swapchain image acquire before writing to it.
        VkSubpassDependency dep {};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpci {};
        rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = 1;
        rpci.pAttachments    = &colorAtt;
        rpci.subpassCount    = 1;
        rpci.pSubpasses      = &subpass;
        rpci.dependencyCount = 1;
        rpci.pDependencies   = &dep;

        result = vkCreateRenderPass(device, &rpci, nullptr, &pipeline.renderPass);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)pipeline.renderPass, "lighting_render_pass");

        // One framebuffer per swapchain image
        uint32_t imageCount = swapchain.GetSwapchainImageCount();
        pipeline.framebuffers.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; i++)
        {
            VkImageView view = swapchain.GetSwapchainImageView(i);
            char name[32];
            snprintf(name, sizeof(name), "lighting_fb_%u", i);
            result = CreateFramebuffer(device, pipeline.renderPass, 1, &view, width, height, pipeline.framebuffers[i], name);
            if (result != VK_SUCCESS) return result;
        }

        // Pipeline layout: set 0 = globals UBO, set 1 = G-Buffer samplers (no push constants)
        std::array<VkDescriptorSetLayout, 2> setLayouts = {
            pipeline.pGlobalUniforms->descriptorSetLayout,
            pipeline.descriptorSetLayout
        };

        VkPipelineLayoutCreateInfo plci {};
        plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        plci.pSetLayouts    = setLayouts.data();
        result = vkCreatePipelineLayout(device, &plci, nullptr, &pipeline.pipelineLayout);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)pipeline.pipelineLayout, "lighting_layout");

        VkPipelineShaderStageCreateInfo stages[2] {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName  = "main";

        // No vertex input - fullscreen triangle is generated in the vertex shader
        VkPipelineVertexInputStateCreateInfo pvisi {};
        pvisi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo piasi {};
        piasi.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        piasi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo pvsi {};
        pvsi.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        pvsi.viewportCount = 1;
        pvsi.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo prsi {};
        prsi.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        prsi.polygonMode = VK_POLYGON_MODE_FILL;
        prsi.cullMode    = VK_CULL_MODE_NONE;  // fullscreen pass - no backface culling
        prsi.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        prsi.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo pmsi {};
        pmsi.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pmsi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAtt {};
        blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo pcbsci {};
        pcbsci.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        pcbsci.attachmentCount = 1;
        pcbsci.pAttachments    = &blendAtt;

        // No depth test or write for the fullscreen lighting pass
        VkPipelineDepthStencilStateCreateInfo pdsci {};
        pdsci.sType           = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        pdsci.depthTestEnable = VK_FALSE;

        std::array<VkDynamicState, 2> dynStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo pdsi {};
        pdsi.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pdsi.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
        pdsi.pDynamicStates    = dynStates.data();

        VkGraphicsPipelineCreateInfo gpci {};
        gpci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount          = 2;
        gpci.pStages             = stages;
        gpci.pVertexInputState   = &pvisi;
        gpci.pInputAssemblyState = &piasi;
        gpci.pViewportState      = &pvsi;
        gpci.pRasterizationState = &prsi;
        gpci.pMultisampleState   = &pmsi;
        gpci.pColorBlendState    = &pcbsci;
        gpci.pDepthStencilState  = &pdsci;
        gpci.pDynamicState       = &pdsi;
        gpci.layout              = pipeline.pipelineLayout;
        gpci.renderPass          = pipeline.renderPass;

        result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline.pipeline);
        if (result == VK_SUCCESS)
            SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline.pipeline, "lighting_pipeline");
        return result;
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

        VkPushConstantRange pushConstantRange {};
        pushConstantRange.size = sizeof(uint32_t) * 2;
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array<VkDescriptorSetLayout, 2> setLayouts = {
            pipeline.pGlobalUniforms->descriptorSetLayout,
            pipeline.pRenderingDescriptors->descriptorSetLayout
        };

        VkPipelineLayoutCreateInfo plci {};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 2;
        plci.pSetLayouts = setLayouts.data();
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pushConstantRange;

        VkResult result = vkCreatePipelineLayout(device, &plci, nullptr, &pipeline.pipelineLayout);
        if (result != VK_SUCCESS)
            return result;
        SetDebugName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)pipeline.pipelineLayout, "phong_layout");

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
        pdsci.depthTestEnable = VK_TRUE;
        pdsci.depthWriteEnable = VK_TRUE;
        pdsci.depthCompareOp = VK_COMPARE_OP_LESS;
        
        std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo pdsci2 {};
        pdsci2.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pdsci2.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        pdsci2.pDynamicStates = dynamicStates.data();

        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;

        std::array<VkAttachmentDescription, 2> colorAttachmentDescs {};
        colorAttachmentDescs[0].format = colorFormat;
        colorAttachmentDescs[0].samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachmentDescs[1].format = VK_FORMAT_D32_SFLOAT;
        colorAttachmentDescs[1].samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentReference, 2> colorAttachmentRefs {};
        colorAttachmentRefs[0].attachment = 0;
        colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentRefs[1].attachment = 1;
        colorAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc {};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = static_cast<uint32_t>(1);
        subpassDesc.pColorAttachments = colorAttachmentRefs.data();
        subpassDesc.pDepthStencilAttachment = &colorAttachmentRefs[1];

        VkRenderPassCreateInfo prci {};
        prci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        prci.attachmentCount = static_cast<uint32_t>(colorAttachmentDescs.size());
        prci.pAttachments = colorAttachmentDescs.data();
        prci.subpassCount = 1;
        prci.pSubpasses = &subpassDesc;

        result = vkCreateRenderPass(device, &prci, nullptr, &pipeline.renderPass);
        if (result != VK_SUCCESS)
            return result;
        SetDebugName(device, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)pipeline.renderPass, "phong_render_pass");

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
        if (result == VK_SUCCESS)
            SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline.pipeline, "phong_pipeline");
        return result;
    };

    void InsertPipelineBarrier(VkCommandBuffer cb, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
        , VkAccessFlags srcAccess, VkAccessFlags dstAccess)
    {
        VkMemoryBarrier memoryBarrier {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = srcAccess;
        memoryBarrier.dstAccessMask = dstAccess;

        vkCmdPipelineBarrier(cb,
            srcStage, dstStage,
            0,
            1, &memoryBarrier,
            0, nullptr,
            0, nullptr);
    }

    void InsertPipelineBarrier2(VkCommandBuffer cb, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage
        , VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess)
    {
        VkMemoryBarrier2 memoryBarrier {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = srcStage;
        memoryBarrier.srcAccessMask = srcAccess;
        memoryBarrier.dstStageMask = dstStage;
        memoryBarrier.dstAccessMask = dstAccess;

        VkDependencyInfo dependencyInfo {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &memoryBarrier;

        vkCmdPipelineBarrier2KHR(cb, &dependencyInfo);
    }
}