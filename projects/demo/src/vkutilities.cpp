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
#include <string>

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

     VkResult SetupGlobalUniforms(imp::Engine& engine, GlobalUniforms& globals, uint32_t framesInFlight)
     {
        VkDevice device = engine.GetWorkQueue().GetDevice();

        // Allocate one HOST_VISIBLE UBO per frame-in-flight so the CPU can overwrite the
        // next frame's data while the GPU is still reading the previous frame's buffer.
        globals.ubos.resize(framesInFlight);
        globals.descriptorSets.resize(framesInFlight);
        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            const std::string name = "globals_ubo_" + std::to_string(i);
            VkResult result = CreateBuffer(engine.GetPhysicalDevice(), device,
                sizeof(GlobalUniformsData),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                globals.ubos[i], name.c_str());
            if (result != VK_SUCCESS) return result;
        }

        // One shared DSL; each DS points to its own UBO slot.
        VkDescriptorSetLayoutBinding binding {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo dslci {};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 1;
        dslci.pBindings = &binding;

        VkResult result = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &globals.descriptorSetLayout);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)globals.descriptorSetLayout, "globals_dsl");

        // Allocate all descriptor sets in a single call
        std::vector<VkDescriptorSetLayout> layouts(framesInFlight, globals.descriptorSetLayout);
        VkDescriptorSetAllocateInfo dsai {};
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = engine.GetDescriptorPool();
        dsai.descriptorSetCount = framesInFlight;
        dsai.pSetLayouts = layouts.data();
        result = vkAllocateDescriptorSets(device, &dsai, globals.descriptorSets.data());
        if (result != VK_SUCCESS) return result;

        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            const std::string dsName = "globals_ds_" + std::to_string(i);
            SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)globals.descriptorSets[i], dsName.c_str());

            VkDescriptorBufferInfo bi {};
            bi.buffer = globals.ubos[i].buffer;
            bi.range  = VK_WHOLE_SIZE;

            VkWriteDescriptorSet write {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = globals.descriptorSets[i];
            write.dstBinding      = 0;
            write.descriptorCount = 1;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo     = &bi;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }

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

        // Extract frustum planes once on the CPU (Gribb-Hartmann, Vulkan [0,1] depth).
        // GLM is column-major: vp[col][row], so we reconstruct rows manually.
        const glm::mat4& vp = globalsData.viewProj;
        glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
        glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
        glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
        glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);
        globalsData.frustumPlanes[0] = row3 + row0;  // left
        globalsData.frustumPlanes[1] = row3 - row0;  // right
        globalsData.frustumPlanes[2] = row3 + row1;  // bottom
        globalsData.frustumPlanes[3] = row3 - row1;  // top
        globalsData.frustumPlanes[4] = row2;          // near  (Vulkan [0,1])
        globalsData.frustumPlanes[5] = row3 - row2;  // far
    }

    void UpdateGlobalDataDescriptorSetByCopy(imp::Engine& engine, const GlobalUniforms& globals, uint32_t frameIndex)
    {
        void* data;
        vkMapMemory(engine.GetWorkQueue().GetDevice(), globals.ubos[frameIndex].memory, 0, sizeof(GlobalUniformsData), 0, &data);
        memcpy(data, &globals.data, sizeof(GlobalUniformsData));
        vkUnmapMemory(engine.GetWorkQueue().GetDevice(), globals.ubos[frameIndex].memory);
    }

    void UpdateRenderingDataDescriptorSetByCopy(imp::Engine& engine, const RenderingDescriptors& renderingData, const std::vector<DrawData>& drawData)
    {
        // Deprecated: drawDataBuffer is now DEVICE_LOCAL.  Use a staged copy for dirty meshes instead.
        (void)engine; (void)renderingData; (void)drawData;
    }

    VkResult SetupRenderingDescriptorSet(imp::Engine& engine, RenderingDescriptors& data,
        SceneLoader::Scene& scenel, const std::vector<DrawData>& drawDatas)
    {
        VkDevice device = engine.GetWorkQueue().GetDevice();

        const uint32_t meshCount = static_cast<uint32_t>(scenel.meshes.size());
        const uint32_t materialCount = std::max(static_cast<uint32_t>(scenel.materials.size()), 1u);
        const uint32_t textureCount = static_cast<uint32_t>(scenel.textures.size());

        // ---------------------------------------------------------
        // drawData, materialBuffer, boundingSpheres: all DEVICE_LOCAL.
        // Written once at startup via staging; never mapped again.
        // Future dirty-mesh updates should use partial staged copies.
        // ---------------------------------------------------------
        const VkDeviceSize ddSize  = sizeof(DrawData)             * std::max(meshCount, 1u);
        const VkDeviceSize matSize = sizeof(SceneLoader::Material) * std::max(materialCount, 1u);
        const VkDeviceSize bvSize  = sizeof(BoundingSphere)       * std::max(meshCount, 1u);

        // Staging buffers (HOST_VISIBLE, temporary)
        Buffer ddStaging, matStaging, bvStaging;
        VkResult result = CreateBuffer(engine.GetPhysicalDevice(), device, ddSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            ddStaging, "draw_data_staging");
        if (result != VK_SUCCESS) return result;

        result = CreateBuffer(engine.GetPhysicalDevice(), device, matSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            matStaging, "material_staging");
        if (result != VK_SUCCESS) return result;

        result = CreateBuffer(engine.GetPhysicalDevice(), device, bvSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bvStaging, "bounding_sphere_staging");
        if (result != VK_SUCCESS) return result;

        // Fill staging buffers from CPU data
        {
            void* mapped;
            vkMapMemory(device, ddStaging.memory, 0, ddSize, 0, &mapped);
            if (!drawDatas.empty())
                memcpy(mapped, drawDatas.data(), sizeof(DrawData) * drawDatas.size());
            vkUnmapMemory(device, ddStaging.memory);

            vkMapMemory(device, matStaging.memory, 0, matSize, 0, &mapped);
            if (!scenel.materials.empty())
                memcpy(mapped, scenel.materials.data(), sizeof(SceneLoader::Material) * scenel.materials.size());
            vkUnmapMemory(device, matStaging.memory);

            vkMapMemory(device, bvStaging.memory, 0, bvSize, 0, &mapped);
            if (!scenel.boundingSpheres.empty())
                memcpy(mapped, scenel.boundingSpheres.data(), sizeof(BoundingSphere) * scenel.boundingSpheres.size());
            vkUnmapMemory(device, bvStaging.memory);
        }

        // Device-local destination buffers
        result = CreateBuffer(engine.GetPhysicalDevice(), device, ddSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            data.drawDataBuffer, "draw_data_buffer");
        if (result != VK_SUCCESS) return result;

        result = CreateBuffer(engine.GetPhysicalDevice(), device, matSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            data.materialBuffer, "material_buffer");
        if (result != VK_SUCCESS) return result;

        result = CreateBuffer(engine.GetPhysicalDevice(), device, bvSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            data.boundingSphereBuffer, "bounding_sphere_buffer");
        if (result != VK_SUCCESS) return result;

        // Single command buffer: copy all three + one barrier covering all shader stages that read them
        {
            VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);
            VkBufferCopy region {};
            region.size = ddSize;  vkCmdCopyBuffer(cb, ddStaging.buffer,  data.drawDataBuffer.buffer,      1, &region);
            region.size = matSize; vkCmdCopyBuffer(cb, matStaging.buffer, data.materialBuffer.buffer,      1, &region);
            region.size = bvSize;  vkCmdCopyBuffer(cb, bvStaging.buffer,  data.boundingSphereBuffer.buffer, 1, &region);

            InsertPipelineBarrier2(cb,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
            vkEndCommandBuffer(cb);

            imp::SubmitParams submit {};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers    = &cb;
            submit.queue              = engine.GetWorkQueue().GetGraphicsQueue();
            imp::SubmitSync sync = engine.Submit(&submit, 1);

            // Enqueue all staging buffers for deferred destruction
            imp::SafeResourceDestroyer& destroyer = engine.GetSafeResourceDestroyer();
            for (Buffer* staging : { &ddStaging, &matStaging, &bvStaging })
            {
                imp::VulkanResource res {};
                res.type   = imp::VulkanResourceType::Buffer;
                res.buffer = staging->buffer;
                res.memory = staging->memory;
                destroyer.EnqueueResourceForDestruction(res, sync.submit);
            }
        }

        std::array<VkDescriptorSetLayoutBinding, 6> bindings {};
        bindings[0] = { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr };
        bindings[1] = { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        bindings[2] = { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        bindings[3] = { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::max(textureCount, 1u), VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        bindings[4] = { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        bindings[5] = { 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

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

        {   // Allocate the draw index buffer (device-local, written by compute, read by vertex shader)
            const VkDeviceSize diSize = sizeof(uint32_t) * std::max(meshCount, 1u);
            result = CreateBuffer(engine.GetPhysicalDevice(), device,
                                    diSize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    data.drawIndexBuffer, "draw_index_buffer");
            if (result != VK_SUCCESS)
                return result;
        }

        std::array<VkDescriptorBufferInfo, 5> bi {};
        bi[0] = { scenel.vertexBuffer.buffer,        0, VK_WHOLE_SIZE };
        bi[1] = { data.drawDataBuffer.buffer,        0, VK_WHOLE_SIZE };
        bi[2] = { data.materialBuffer.buffer,        0, VK_WHOLE_SIZE };
        bi[3] = { data.boundingSphereBuffer.buffer,  0, VK_WHOLE_SIZE };
        bi[4] = { data.drawIndexBuffer.buffer,       0, VK_WHOLE_SIZE };

        std::array<VkWriteDescriptorSet, 5> bufferWrites {};
        for (uint32_t i = 0; i < 5; i++)
        {
            bufferWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            bufferWrites[i].dstSet = data.descriptorSet;
            // bindings: 0,1,2,4,5  (slot 3 is the texture array, not a plain buffer)
            const uint32_t bindingMap[] = { 0, 1, 2, 4, 5 };
            bufferWrites[i].dstBinding = bindingMap[i];
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
        
    VkResult BuildIndirectDrawBuffer(imp::Engine& engine, const SceneLoader::Scene& scenel, IndirectDrawBuffer& out)
    {
        VkPhysicalDevice pDevice = engine.GetPhysicalDevice();
        VkDevice         device  = engine.GetWorkQueue().GetDevice();

        const uint32_t   drawCount    = static_cast<uint32_t>(scenel.meshes.size());
        const VkDeviceSize commandsSize = sizeof(VkDrawIndexedIndirectCommand) * drawCount;
        out.maxDrawCount = drawCount;

        // Staging: commands
        Buffer commandsStaging;
        VkResult result = CreateBuffer(pDevice, device, commandsSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            commandsStaging, "indirect_commands_staging");
        if (result != VK_SUCCESS) return result;

        void* mapped;
        vkMapMemory(device, commandsStaging.memory, 0, commandsSize, 0, &mapped);
        auto* cmds = static_cast<VkDrawIndexedIndirectCommand*>(mapped);
        for (uint32_t i = 0; i < drawCount; i++)
        {
            const auto& mesh = scenel.meshes[i];
            cmds[i].indexCount    = mesh.indexCount;
            cmds[i].instanceCount = 1;
            cmds[i].firstIndex    = mesh.indexOffset;
            cmds[i].vertexOffset  = mesh.vertexOffset;
            cmds[i].firstInstance = 0;
        }
        vkUnmapMemory(device, commandsStaging.memory);

        // Staging: count
        Buffer countStaging;
        result = CreateBuffer(pDevice, device, sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            countStaging, "indirect_count_staging");
        if (result != VK_SUCCESS) return result;

        vkMapMemory(device, countStaging.memory, 0, sizeof(uint32_t), 0, &mapped);
        memcpy(mapped, &drawCount, sizeof(uint32_t));
        vkUnmapMemory(device, countStaging.memory);

        // Device-local destinations
        // STORAGE_BUFFER_BIT is required because the culling compute shader reads srcCmds
        // (binding 0) as a storage buffer via vkUpdateDescriptorSets.
        result = CreateBuffer(pDevice, device, commandsSize,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            out.commands, "indirect_commands");
        if (result != VK_SUCCESS) return result;

        result = CreateBuffer(pDevice, device, sizeof(uint32_t),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            out.countBuffer, "indirect_count");
        if (result != VK_SUCCESS) return result;

        // Upload
        VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);

        VkBufferCopy region {};
        region.size = commandsSize;
        vkCmdCopyBuffer(cb, commandsStaging.buffer, out.commands.buffer, 1, &region);
        region.size = sizeof(uint32_t);
        vkCmdCopyBuffer(cb, countStaging.buffer, out.countBuffer.buffer, 1, &region);

        InsertPipelineBarrier2(cb,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

        vkEndCommandBuffer(cb);

        imp::SubmitParams submit {};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cb;
        submit.queue              = engine.GetWorkQueue().GetGraphicsQueue();
        imp::SubmitSync sync = engine.Submit(&submit, 1);

        // Staging buffers are no longer needed once the GPU finishes the transfer
        imp::SafeResourceDestroyer& destroyer = engine.GetSafeResourceDestroyer();
        imp::VulkanResource res {};
        res.type   = imp::VulkanResourceType::Buffer;
        res.buffer = commandsStaging.buffer;
        res.memory = commandsStaging.memory;
        destroyer.EnqueueResourceForDestruction(res, sync.submit);
        res.buffer = countStaging.buffer;
        res.memory = countStaging.memory;
        destroyer.EnqueueResourceForDestruction(res, sync.submit);

        return VK_SUCCESS;
    }

    VkResult CreateCullingPipeline(imp::Engine& engine, VkShaderModule compModule,
        const IndirectDrawBuffer& srcDrawBuffer,
        const RenderingDescriptors& renderingDescriptors,
        GlobalUniforms& globals,
        CullingPipeline& out)
    {
        VkPhysicalDevice pDevice = engine.GetPhysicalDevice();
        VkDevice         device  = engine.GetWorkQueue().GetDevice();

        out.pGlobalUniforms = &globals;

        const uint32_t   maxDraws = srcDrawBuffer.maxDrawCount;
        const VkDeviceSize cmdSize = sizeof(VkDrawIndexedIndirectCommand) * std::max(maxDraws, 1u);

        // Device-local output command buffer (written by compute, read as indirect)
        VkResult result = CreateBuffer(pDevice, device, cmdSize,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            out.culledBuffer.commands, "culled_commands");
        if (result != VK_SUCCESS) return result;

        // Device-local output count buffer (zeroed via vkCmdFillBuffer each frame)
        result = CreateBuffer(pDevice, device, sizeof(uint32_t),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            out.culledBuffer.countBuffer, "culled_count");
        if (result != VK_SUCCESS) return result;

        out.culledBuffer.maxDrawCount = maxDraws;

        // set 0: 6 storage buffers — srcCmds, BVs, DrawData, dstCmds, dstCount, dstIndices
        std::array<VkDescriptorSetLayoutBinding, 6> bindings {};
        for (uint32_t i = 0; i < 6; ++i)
            bindings[i] = { i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

        VkDescriptorSetLayoutCreateInfo dslci {};
        dslci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = static_cast<uint32_t>(bindings.size());
        dslci.pBindings    = bindings.data();
        result = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &out.descriptorSetLayout);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)out.descriptorSetLayout, "culling_dsl");

        VkDescriptorSetAllocateInfo dsai {};
        dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool     = engine.GetDescriptorPool();
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts        = &out.descriptorSetLayout;
        result = vkAllocateDescriptorSets(device, &dsai, &out.descriptorSet);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)out.descriptorSet, "culling_ds");

        std::array<VkDescriptorBufferInfo, 6> bi {};
        bi[0] = { srcDrawBuffer.commands.buffer,                    0, VK_WHOLE_SIZE };
        bi[1] = { renderingDescriptors.boundingSphereBuffer.buffer,  0, VK_WHOLE_SIZE };
        bi[2] = { renderingDescriptors.drawDataBuffer.buffer,        0, VK_WHOLE_SIZE };
        bi[3] = { out.culledBuffer.commands.buffer,                  0, VK_WHOLE_SIZE };
        bi[4] = { out.culledBuffer.countBuffer.buffer,               0, VK_WHOLE_SIZE };
        bi[5] = { renderingDescriptors.drawIndexBuffer.buffer,       0, VK_WHOLE_SIZE };

        std::array<VkWriteDescriptorSet, 6> writes {};
        for (uint32_t i = 0; i < 6; ++i)
        {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = out.descriptorSet;
            writes[i].dstBinding      = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo     = &bi[i];
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        VkPushConstantRange pcRange {};
        pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pcRange.size       = sizeof(CullingPushConstants);

        // set 1: globals UBO (frustum planes + camera data) — reuse existing descriptor set layout
        std::array<VkDescriptorSetLayout, 2> setLayouts = { out.descriptorSetLayout, globals.descriptorSetLayout };

        VkPipelineLayoutCreateInfo plci {};
        plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
        plci.pSetLayouts            = setLayouts.data();
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges    = &pcRange;
        result = vkCreatePipelineLayout(device, &plci, nullptr, &out.pipelineLayout);
        if (result != VK_SUCCESS) return result;
        SetDebugName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)out.pipelineLayout, "culling_layout");

        VkPipelineShaderStageCreateInfo stage {};
        stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = compModule;
        stage.pName  = "main";

        VkComputePipelineCreateInfo cpci {};
        cpci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpci.stage  = stage;
        cpci.layout = out.pipelineLayout;
        result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &out.pipeline);
        if (result == VK_SUCCESS)
            SetDebugName(device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)out.pipeline, "culling_pipeline");
        return result;
    }

    void DispatchCulling(VkCommandBuffer cb, const CullingPipeline& pipeline, uint32_t totalDraws, uint32_t frameIndex)
    {
        // Reset the output draw count to 0 so the atomic counter starts fresh.
        vkCmdFillBuffer(cb, pipeline.culledBuffer.countBuffer.buffer, 0, sizeof(uint32_t), 0);

        // Ensure the fill is visible before the compute shader increments the counter.
        InsertPipelineBarrier2(cb,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);

        // set 0 = culling storage buffers, set 1 = globals UBO for this frame slot
        std::array<VkDescriptorSet, 2> sets = { pipeline.descriptorSet, pipeline.pGlobalUniforms->descriptorSets[frameIndex] };
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipelineLayout,
            0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

        CullingPushConstants pc { totalDraws };
        vkCmdPushConstants(cb, pipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(pc), &pc);

        const uint32_t groupCount = (totalDraws + 63u) / 64u;
        vkCmdDispatch(cb, groupCount, 1, 1);

        // Culled commands and draw indices must be visible before indirect draw and vertex reads.
        InsertPipelineBarrier2(cb,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
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

        // Pipeline layout: set 0 = globals UBO, set 1 = rendering descriptors
        // No push constants - draw index is encoded in firstInstance (gl_BaseInstanceARB).
        std::array<VkDescriptorSetLayout, 2> setLayouts = {
            pipeline.pGlobalUniforms->descriptorSetLayout,
            pipeline.pRenderingDescriptors->descriptorSetLayout
        };

        VkPipelineLayoutCreateInfo plci {};
        plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        plci.pSetLayouts    = setLayouts.data();

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