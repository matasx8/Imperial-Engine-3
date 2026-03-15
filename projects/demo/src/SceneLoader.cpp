#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "SceneLoader.h"
#include "Engine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <cmath>
#include <algorithm>
#include <future>
#include <thread>
#include <chrono>

namespace SceneLoader
{
    static inline std::atomic_uint32_t temporaryMeshCounter = 0;

    // Maximum texture dimension. Images larger than this are box-filter
    // downscaled on the CPU during parallel decode to stay within VRAM budget.
    static constexpr int kMaxTextureSize = 2048;

    // Stores raw compressed bytes without decoding them.
    // The actual stbi decode is done in parallel after TinyGLTF finishes parsing.
    static bool DeferredImageLoader(tinygltf::Image* image, const int /*imageIdx*/,
        std::string* /*err*/, std::string* /*warn*/,
        int /*reqWidth*/, int /*reqHeight*/,
        const unsigned char* bytes, int size, void* /*userData*/)
    {
        image->as_is = true;
        image->image.resize(static_cast<size_t>(size));
        memcpy(image->image.data(), bytes, static_cast<size_t>(size));
        return true;
    }

    static bool ParseGLTF(const std::filesystem::path& path, Scene& scene, CreationRequests& reqs)
    {
        if (!std::filesystem::exists(path))
            return false;

        if (path.extension() != ".gltf" && path.extension() != ".glb")
            return false;

        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
		std::string err;
		std::string warn;

        loader.SetImageLoader(DeferredImageLoader, nullptr);

        if(path.extension().string() == ".gltf")
			loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
		else
			loader.LoadBinaryFromFile(&model, &err, &warn, path.string());

		if (err.size()) printf("[Asset Importer] Error: %s\n", err.c_str());
		if (warn.size()) printf("[Asset Importer] Warning: %s\n", warn.c_str());

        // Decode all images in parallel. stbi_load_from_memory is thread-safe per-call
        // since each invocation operates on its own independent data.
        // Images larger than kMaxTextureSize are box-filter downscaled on the CPU so
        // we never attempt to allocate more VRAM than the device can supply.

        // Box-filter 2x downscale for RGBA8 data.  Handles odd dimensions correctly.
        auto boxHalve = [](const uint8_t* src, int sw, int sh) -> std::vector<uint8_t>
        {
            const int dw = std::max(1, sw / 2);
            const int dh = std::max(1, sh / 2);
            std::vector<uint8_t> dst(static_cast<size_t>(dw * dh * 4));
            for (int y = 0; y < dh; ++y)
            {
                for (int x = 0; x < dw; ++x)
                {
                    // Clamp the second sample for odd dimensions
                    const int x1 = std::min(x * 2 + 1, sw - 1);
                    const int y1 = std::min(y * 2 + 1, sh - 1);
                    for (int c = 0; c < 4; ++c)
                    {
                        int sum = src[(y*2 * sw + x*2)  * 4 + c]
                                + src[(y*2 * sw + x1)   * 4 + c]
                                + src[(y1  * sw + x*2)  * 4 + c]
                                + src[(y1  * sw + x1)   * 4 + c];
                        dst[(y * dw + x) * 4 + c] = static_cast<uint8_t>(sum >> 2);
                    }
                }
            }
            return dst;
        };

        {
            std::vector<std::future<bool>> decodeFutures;
            decodeFutures.reserve(model.images.size());
            for (auto& image : model.images)
            {
                decodeFutures.push_back(std::async(std::launch::async, [&image, &boxHalve]() -> bool
                {
                    int w, h, comp;
                    unsigned char* pixels = stbi_load_from_memory(
                        image.image.data(), static_cast<int>(image.image.size()),
                        &w, &h, &comp, 4);
                    if (!pixels)
                    {
                        printf("[Scene Loader] Error: Failed to decode image '%s'\n", image.name.c_str());
                        return false;
                    }

                    // Downscale to kMaxTextureSize using iterative 2x box filter.
                    // stbi_image doesn't support decode-time scaling, so we must decode
                    // at full resolution first. We free the stbi buffer immediately after
                    // the first halve to minimise peak host RAM — for a 4096^2 image the
                    // stbi buffer (64 MB) and the first halved buffer (16 MB) coexist
                    // only briefly before the stbi buffer is released.
                    if (w > kMaxTextureSize || h > kMaxTextureSize)
                    {
                        // First halve: read from the raw stbi buffer, then free it.
                        std::vector<uint8_t> current = boxHalve(pixels, w, h);
                        stbi_image_free(pixels);
                        pixels = nullptr;
                        w = std::max(1, w / 2);
                        h = std::max(1, h / 2);

                        // Subsequent halves (if still oversized): in-place replacement.
                        while (w > kMaxTextureSize || h > kMaxTextureSize)
                        {
                            current = boxHalve(current.data(), w, h);
                            w = std::max(1, w / 2);
                            h = std::max(1, h / 2);
                        }

                        image.image = std::move(current);
                    }
                    else
                    {
                        image.image.assign(pixels, pixels + static_cast<size_t>(w * h * 4));
                        stbi_image_free(pixels);
                    }

                    image.width      = w;
                    image.height     = h;
                    image.component  = 4;
                    image.bits       = 8;
                    image.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                    image.as_is      = false;
                    return true;
                }));
            }
            for (auto& f : decodeFutures)
                f.get();
        }

		std::unordered_map<uint32_t, uint32_t> meshIdMap;

        for (const auto& nodeIdx : model.scenes.front().nodes)
        {
            const auto& node = model.nodes[nodeIdx];
            LoadGLTFNode(node, model, meshIdMap, reqs.meshes, scene);
        }

        // Load images
        for (auto& image : model.images)
		{
			ImageCreationRequest req;
			req.id = reqs.images.size();
			req.width = image.width;
			req.height = image.height;

			assert(!image.name.empty()); // images should have names
			req.name = std::move(image.name);
			req.image = std::move(image.image);
			reqs.images.push_back(req);
		}

        // Load samplers
        for (size_t i = 0; i < model.samplers.size(); ++i)
        {
            const auto& sampler = model.samplers[i];
            SamplerCreationRequest req;
            req.id = static_cast<uint32_t>(reqs.samplers.size());
            req.name = sampler.name;
            req.magFilter = sampler.magFilter;
            req.minFilter = sampler.minFilter;
            req.wrapS = sampler.wrapS;
            req.wrapT = sampler.wrapT;
            reqs.samplers.push_back(req);
        }

        // Load textures
        for (size_t i = 0; i < model.textures.size(); ++i)
        {
            const auto& texture = model.textures[i];
            TextureCreationRequest req;
            req.id = static_cast<uint32_t>(reqs.textures.size());
            req.name = texture.name;
            req.imageId = texture.source >= 0 ? static_cast<uint32_t>(texture.source) : kInvalidId;
            req.samplerId = texture.sampler >= 0 ? static_cast<uint32_t>(texture.sampler) : kInvalidId;
            reqs.textures.push_back(req);
        }

        // Load materials
        for (size_t i = 0; i < model.materials.size(); ++i)
        {
            const auto& material = model.materials[i];
            MaterialCreationRequest req;
            req.id = static_cast<uint32_t>(reqs.materials.size());
            req.name = material.name;
            
            // Base color texture
            if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
                req.baseColorTextureId = static_cast<uint32_t>(material.pbrMetallicRoughness.baseColorTexture.index);
            else
                req.baseColorTextureId = kInvalidId;

            // Normal texture
            if (material.normalTexture.index >= 0)
                req.normalTextureId = static_cast<uint32_t>(material.normalTexture.index);
            else
                req.normalTextureId = kInvalidId;

            // Metallic roughness texture
            if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
                req.metallicRoughnessTextureId = static_cast<uint32_t>(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
            else
                req.metallicRoughnessTextureId = kInvalidId;

            // Base color factor
            if (material.pbrMetallicRoughness.baseColorFactor.size() == 4)
            {
                req.baseColorFactor = glm::vec4(
                    material.pbrMetallicRoughness.baseColorFactor[0],
                    material.pbrMetallicRoughness.baseColorFactor[1],
                    material.pbrMetallicRoughness.baseColorFactor[2],
                    material.pbrMetallicRoughness.baseColorFactor[3]
                );
            }
            else
                req.baseColorFactor = glm::vec4(1.0f);

            req.metallicFactor = material.pbrMetallicRoughness.metallicFactor;
            req.roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;
            
            reqs.materials.push_back(req);
        }

        return true;
    }

    static Image RequestToImage(const ImageCreationRequest& req, const VU::Image& img)
    {
        Image image {};
        image.id = req.id;
        image.width = req.width;
        image.height = req.height;
        image.name = req.name;
        image.image = img;
        return image;
    }

    bool LoadScene(const std::filesystem::path& path, imp::Engine& engine, Scene& scene, LoadStats* outStats)
    {
        using Clock = std::chrono::high_resolution_clock;
        using Ms    = std::chrono::duration<double, std::milli>;

        LoadStats stats {};
        const auto totalStart = Clock::now();

        CreationRequests reqs;
        {
            const auto parseStart = Clock::now();
            if (!ParseGLTF(path, scene, reqs))
                return false;
            stats.parseTimeMs = Ms(Clock::now() - parseStart).count();
        }

        VkPhysicalDevice pDevice = engine.GetPhysicalDevice();
        VkDevice device = engine.GetWorkQueue().GetDevice();

        const auto gpuStart = Clock::now();

        VkDeviceSize vertexBufferSize = 0;
        VkDeviceSize indexBufferSize = 0;        
        for (const auto& req : reqs.meshes)
        {
            Mesh mesh {};
            mesh.id = req.id;
            mesh.vertexOffset = static_cast<uint32_t>(vertexBufferSize / sizeof(VU::Vertex));
            mesh.indexOffset = static_cast<uint32_t>(indexBufferSize / sizeof(uint32_t));
            mesh.vertexCount = static_cast<uint32_t>(req.vertices.size());
            mesh.indexCount = static_cast<uint32_t>(req.indices.size());
            scene.meshes.push_back(mesh);

            stats.vertexCount += mesh.vertexCount;
            stats.indexCount  += mesh.indexCount;

            vertexBufferSize += sizeof(VU::Vertex) * req.vertices.size();
            indexBufferSize += sizeof(uint32_t) * req.indices.size();
        }
        stats.meshCount       = static_cast<uint32_t>(reqs.meshes.size());
        stats.vertexMemBytes  = vertexBufferSize;
        stats.indexMemBytes   = indexBufferSize;

        // Create vertex staging buffer
        VU::Buffer vertexStagingBuffer;
        VkResult result = CreateBuffer(pDevice, device,
                                        vertexBufferSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        vertexStagingBuffer);
        if (result != VK_SUCCESS)
            return result;

        // Map and copy vertex data
        void* vertexData;
        vkMapMemory(device, vertexStagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &vertexData);
        for (const auto& req : reqs.meshes)
        {
            memcpy(vertexData, req.vertices.data(), sizeof(VU::Vertex) * req.vertices.size());
            vertexData = static_cast<uint8_t*>(vertexData) + sizeof(VU::Vertex) * req.vertices.size();
        }
        vkUnmapMemory(device, vertexStagingBuffer.memory);

        // Create index buffer
        VU::Buffer indexStagingBuffer;
        result = CreateBuffer(pDevice, device,
                                indexBufferSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        indexStagingBuffer);

        if (result != VK_SUCCESS)
            return result;

        // Map and copy index data
        void* indexData;
        vkMapMemory(device, indexStagingBuffer.memory, 0, VK_WHOLE_SIZE, 0, &indexData);
        for (const auto& req : reqs.meshes)
        {
            memcpy(indexData, req.indices.data(), sizeof(uint32_t) * req.indices.size());
            indexData = static_cast<uint8_t*>(indexData) + sizeof(uint32_t) * req.indices.size();
        }
        vkUnmapMemory(device, indexStagingBuffer.memory);

        // Create device local vertex buffer
        result = CreateBuffer(pDevice, device,
                                vertexBufferSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                scene.vertexBuffer);

        if (result != VK_SUCCESS)
            return result;

        // Create device local index buffer
        result = CreateBuffer(pDevice, device,
                                indexBufferSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                scene.indexBuffer);

        if (result != VK_SUCCESS)
            return result;

         VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);

        // Copy vertex data to device local buffer
        VkBufferCopy copyRegion {};
        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(cb, vertexStagingBuffer.buffer, scene.vertexBuffer.buffer, 1, &copyRegion);

        // Copy index data to device local buffer
        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(cb, indexStagingBuffer.buffer, scene.indexBuffer.buffer, 1, &copyRegion);

        // Add memory barrier to ensure copies are visible before reads
        VU::InsertPipelineBarrier2(cb,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT);

        // Create GPU images from image requests - batch into single command buffer
        std::vector<VU::Buffer> imageStagingBuffers;
        std::vector<VU::Image> gpuImagesCreated;
        // Maps reqs.images[i] → index in scene.images; kInvalidId when creation failed.
        std::vector<size_t> successfulImageReqIndices;
        std::vector<uint32_t> imageIdToSceneIdx(reqs.images.size(), kInvalidId);

        for (size_t imgIdx = 0; imgIdx < reqs.images.size(); ++imgIdx)
        {
            const auto& imageReq = reqs.images[imgIdx];

            if (imageReq.width == 0 || imageReq.height == 0)
            {
                printf("[Scene Loader] WARNING: Skipping image '%s' — stbi decode produced zero dimensions\n", imageReq.name.c_str());
                stats.imageSkipped++;
                continue;
            }

            const uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(imageReq.width, imageReq.height)))) + 1;

            VU::Image gpuImage;
            result = VU::CreateImage(pDevice, device, imageReq.width, imageReq.height, 
                                    VK_FORMAT_R8G8B8A8_SRGB,
                                    VK_IMAGE_TILING_OPTIMAL,
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    gpuImage,
                                    imageReq.name.c_str(),
                                    mipLevels);
            if (result != VK_SUCCESS)
            {
                printf("[Scene Loader] ERROR: vkCreateImage failed for '%s' (VkResult=%d, %ux%u) — out of VRAM?\n",
                    imageReq.name.c_str(), result, imageReq.width, imageReq.height);
                stats.imageFailed++;
                continue;
            }

            result = VU::CreateImageView(device, gpuImage.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, gpuImage.imageView, (imageReq.name + "_view").c_str(), mipLevels);
            if (result != VK_SUCCESS)
            {
                printf("[Scene Loader] ERROR: vkCreateImageView failed for '%s' (VkResult=%d)\n",
                    imageReq.name.c_str(), result);
                vkDestroyImage(device, gpuImage.image, nullptr);
                vkFreeMemory(device, gpuImage.memory, nullptr);
                stats.imageFailed++;
                continue;
            }
            
            // Create staging buffer for image data
            VU::Buffer imageStagingBuffer;
            VkDeviceSize imageDataSize = imageReq.width * imageReq.height * 4;
            result = CreateBuffer(pDevice, device, imageDataSize,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                imageStagingBuffer);
            if (result != VK_SUCCESS)
            {
                printf("[Scene Loader] ERROR: staging buffer allocation failed for '%s' (VkResult=%d)\n",
                    imageReq.name.c_str(), result);
                vkDestroyImageView(device, gpuImage.imageView, nullptr);
                vkDestroyImage(device, gpuImage.image, nullptr);
                vkFreeMemory(device, gpuImage.memory, nullptr);
                stats.imageFailed++;
                continue;
            }
            
            // Copy image data to staging buffer
            void* imageData;
            vkMapMemory(device, imageStagingBuffer.memory, 0, imageDataSize, 0, &imageData);
            memcpy(imageData, imageReq.image.data(), imageDataSize);
            vkUnmapMemory(device, imageStagingBuffer.memory);
            
            imageStagingBuffers.push_back(imageStagingBuffer);
            gpuImagesCreated.push_back(gpuImage);
            successfulImageReqIndices.push_back(imgIdx);

            imageIdToSceneIdx[imgIdx] = static_cast<uint32_t>(scene.images.size());
            scene.images.push_back(RequestToImage(imageReq, gpuImage));
            stats.imageCount++;
            stats.textureMemBytes += imageDataSize;
        }

        if (!gpuImagesCreated.empty())
        {            
            for (size_t i = 0; i < gpuImagesCreated.size(); ++i)
            {
                const auto& imageReq = reqs.images[successfulImageReqIndices[i]];
                const auto& stagingBuffer = imageStagingBuffers[i];
                const auto& gpuImage = gpuImagesCreated[i];
                const uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(imageReq.width, imageReq.height)))) + 1;

                // Transition ALL mip levels to TRANSFER_DST_OPTIMAL up front so each blit can write into them
                VkImageMemoryBarrier barrier {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.image = gpuImage.image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = mipLevels;
                barrier.subresourceRange.layerCount = 1;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0, 0, nullptr, 0, nullptr, 1, &barrier);

                // Copy CPU data into mip level 0
                VkBufferImageCopy region {};
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel   = 0;
                region.imageSubresource.layerCount = 1;
                region.imageExtent = {imageReq.width, imageReq.height, 1};

                vkCmdCopyBufferToImage(cb, stagingBuffer.buffer, gpuImage.image,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                // Blit down the mip chain: each level halves the previous one
                int32_t mipWidth  = static_cast<int32_t>(imageReq.width);
                int32_t mipHeight = static_cast<int32_t>(imageReq.height);

                for (uint32_t mip = 1; mip < mipLevels; ++mip)
                {
                    // Transition the source level to TRANSFER_SRC
                    barrier.subresourceRange.baseMipLevel = mip - 1;
                    barrier.subresourceRange.levelCount   = 1;
                    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        0, 0, nullptr, 0, nullptr, 1, &barrier);

                    int32_t nextWidth  = std::max(1, mipWidth  / 2);
                    int32_t nextHeight = std::max(1, mipHeight / 2);

                    VkImageBlit blit {};
                    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 1};
                    blit.srcOffsets[0]  = {0, 0, 0};
                    blit.srcOffsets[1]  = {mipWidth, mipHeight, 1};
                    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1};
                    blit.dstOffsets[0]  = {0, 0, 0};
                    blit.dstOffsets[1]  = {nextWidth, nextHeight, 1};

                    vkCmdBlitImage(cb,
                        gpuImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        gpuImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit, VK_FILTER_LINEAR);

                    // Transition the source level to SHADER_READ_ONLY — we're done with it
                    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        0, 0, nullptr, 0, nullptr, 1, &barrier);

                    mipWidth  = nextWidth;
                    mipHeight = nextHeight;
                }

                // Transition the last mip level (never read, still DST) to SHADER_READ_ONLY
                barrier.subresourceRange.baseMipLevel = mipLevels - 1;
                barrier.subresourceRange.levelCount   = 1;
                barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                    0, 0, nullptr, 0, nullptr, 1, &barrier);
            }
            
            vkEndCommandBuffer(cb);
            
            // Submit all image copy commands at once
            imp::SubmitParams submit {};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &cb;
            submit.queue = engine.GetWorkQueue().GetGraphicsQueue();
            imp::SubmitSync sync = engine.Submit(&submit, 1);

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
            
            // Enqueue all staging buffers for deferred destruction
            for (auto& stagingBuffer : imageStagingBuffers)
            {
                res.buffer = stagingBuffer.buffer;
                res.memory = stagingBuffer.memory;
                destroyer.EnqueueResourceForDestruction(res, sync.submit);
            }
        }

        // Create samplers
        for (const auto& samplerReq : reqs.samplers)
        {
            VkSampler sampler;
            result = CreateSampler(device, samplerReq.magFilter, samplerReq.minFilter,
                                  samplerReq.wrapS, samplerReq.wrapT, samplerReq.wrapR, sampler);
            if (result != VK_SUCCESS)
            {
                printf("[Scene Loader] ERROR: vkCreateSampler failed for '%s' (VkResult=%d)\n",
                    samplerReq.name.c_str(), result);
                continue;
            }
            const std::string samplerDebugName = samplerReq.name.empty()
                ? "sampler_" + std::to_string(scene.samplers.size())
                : samplerReq.name;
            VU::SetDebugName(device, VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler, samplerDebugName.c_str());
            scene.samplers.push_back(sampler);
            stats.samplerCount++;
        }

        for (const auto& req : reqs.textures)
        {
            const uint32_t sceneImageIdx = (req.imageId < static_cast<uint32_t>(imageIdToSceneIdx.size()))
                ? imageIdToSceneIdx[req.imageId]
                : kInvalidId;
            scene.textures.push_back({ sceneImageIdx, req.samplerId });
        }
        stats.textureCount = static_cast<uint32_t>(scene.textures.size());

        for (const auto& req : reqs.materials)
        {
            Material mat {};
            mat.baseColorFactor         = req.baseColorFactor;
            mat.metallicFactor          = req.metallicFactor;
            mat.roughnessFactor         = req.roughnessFactor;
            mat.albedoIdx               = req.baseColorTextureId;
            mat.normalIdx               = req.normalTextureId;
            mat.metallicRoughnessIdx    = req.metallicRoughnessTextureId;
            scene.materials.push_back(mat);
        }
        stats.materialCount = static_cast<uint32_t>(scene.materials.size());

        // Finalize stats
        stats.gpuUploadTimeMs = Ms(Clock::now() - gpuStart).count();
        stats.totalTimeMs     = Ms(Clock::now() - totalStart).count();
        stats.totalGpuBytes   = stats.vertexMemBytes + stats.indexMemBytes + stats.textureMemBytes;

        auto fmtBytes = [](VkDeviceSize bytes) -> std::string
        {
            if (bytes >= 1024 * 1024 * 1024)
                return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
            if (bytes >= 1024 * 1024)
                return std::to_string(bytes / (1024 * 1024)) + " MB";
            if (bytes >= 1024)
                return std::to_string(bytes / 1024) + " KB";
            return std::to_string(bytes) + " B";
        };

        printf("\n");
        printf("|--------------------------------------------------|\n");
        printf("|              Scene Loader Statistics             |\n");
        printf("|--------------------------------------------------|\n");
        printf("|  Timing                                          |\n");
        printf("|    Parse + image decode : %8.1f ms            |\n", stats.parseTimeMs);
        printf("|    GPU upload           : %8.1f ms            |\n", stats.gpuUploadTimeMs);
        printf("|    Total                : %8.1f ms            |\n", stats.totalTimeMs);
        printf("|--------------------------------------------------|\n");
        printf("|  Scene                                           |\n");
        printf("|    Meshes               : %6u                 |\n", stats.meshCount);
        printf("|    Vertices             : %6u                 |\n", stats.vertexCount);
        printf("|    Indices              : %6u                 |\n", stats.indexCount);
        printf("|    Materials            : %6u                 |\n", stats.materialCount);
        printf("|    Textures             : %6u                 |\n", stats.textureCount);
        printf("|    Samplers             : %6u                 |\n", stats.samplerCount);
        printf("|--------------------------------------------------|\n");
        printf("|  Images                                          |\n");
        printf("|    Max texture size     :  %5d px             |\n", kMaxTextureSize);
        printf("|    Uploaded             : %6u                 |\n", stats.imageCount);
        if (stats.imageSkipped > 0)
        printf("|    Decode failed        : %6u  *** WARNING *** |\n", stats.imageSkipped);
        if (stats.imageFailed > 0)
        printf("|    GPU create failed    : %6u  *** WARNING *** |\n", stats.imageFailed);
        printf("|--------------------------------------------------|\n");
        printf("|  GPU Memory                                      |\n");
        printf("|    Vertex buffer        : %9s              |\n", fmtBytes(stats.vertexMemBytes).c_str());
        printf("|    Index buffer         : %9s              |\n", fmtBytes(stats.indexMemBytes).c_str());
        printf("|    Textures (uncompressed) : %6s              |\n", fmtBytes(stats.textureMemBytes).c_str());
        printf("|    Total                : %9s              |\n", fmtBytes(stats.totalGpuBytes).c_str());
        printf("|--------------------------------------------------|\n");
        printf("\n");

        if (outStats)
            *outStats = stats;

        return true;
    }

    void LoadGLTFNode(const tinygltf::Node& node, const tinygltf::Model& model, std::unordered_map<uint32_t, uint32_t>& meshIdMap
        , std::vector<MeshCreationRequest>& reqs, Scene& scene, glm::mat4 parentTransform)
    {
        // GLTF TRS order: world = T * R * S (applied right-to-left to vertices).
        // Build the local matrix in that order so GLM composes them correctly.
        auto localTransform = glm::mat4x4(1.0f);

		if (node.matrix.size())
			localTransform = glm::make_mat4x4(node.matrix.data());
		else
		{
			if (node.translation.size())
				localTransform = glm::translate(localTransform, glm::vec3(glm::make_vec3(node.translation.data())));

			if (node.rotation.size())
			{
                // GLTF stores quaternions as [x, y, z, w]. GLM's constructor takes (w, x, y, z).
                // glm::make_quat does a raw memcpy into GLM's {w,x,y,z} layout, which
                // would put GLTF's x into w, y into x, etc. — so we must construct explicitly.
                const auto& r = node.rotation;
                const glm::quat q(static_cast<float>(r[3]), static_cast<float>(r[0]),
                                  static_cast<float>(r[1]), static_cast<float>(r[2]));
				localTransform *= glm::mat4(q);
			}

			if (node.scale.size())
				localTransform = glm::scale(localTransform, glm::vec3(glm::make_vec3(node.scale.data())));
		}

        const auto worldTransform = parentTransform * localTransform;

        if (std::string(node.name).find("Camera") != std::string::npos &&
            scene.cameraWasLoaded == false)
        {
            scene.camera.Model = worldTransform;
            scene.cameraWasLoaded = true;
        }

        for (const auto child : node.children)
            LoadGLTFNode(model.nodes[child], model, meshIdMap, reqs, scene, worldTransform);

        // This is so we don't load geometry for linked/duplicate meshes.
        if (meshIdMap.find(node.mesh) != meshIdMap.end())
        {
            const auto& mesh = model.meshes[node.mesh];

            int primIndex = 0;
            for (const auto& prim : mesh.primitives)
            {
                MeshCreationRequest req;
                // Adds primIndex so it maps to the temporary mesh IDs generated below. 
                req.id = meshIdMap[node.mesh] + primIndex;
                req.materialId = model.meshes[node.mesh].primitives.front().material;
                reqs.push_back(req);

                scene.transforms.push_back(worldTransform);

                Entity entity;
                entity.id = static_cast<uint32_t>(scene.entities.size());
                entity.meshId = req.id;
                entity.transformId = static_cast<uint32_t>(scene.transforms.size() - 1);
                entity.materialId = req.materialId;
                scene.entities.push_back(entity);

                primIndex++;
            }

            return;
        }

        if (node.mesh > -1)
        {
            const auto& mesh = model.meshes[node.mesh];

			for (const auto& prim : mesh.primitives)
            {
                MeshCreationRequest req;
				req.id = temporaryMeshCounter.fetch_add(1);
				req.materialId = prim.material >= 0 ? prim.material : kDefaultMaterialIndex;
				if (req.materialId >= kMaxMaterialIndex) // last one is reserved for default
				{
					req.materialId = kDefaultMaterialIndex;
					printf("[Scene Loader] Error: Trying to assign material with ID '%u' when max is '%u'. Will assign default material.\n", prim.material, kMaxMaterialIndex);
				}

                meshIdMap[node.mesh] = req.id; // can keep rewriting this

                const float* positionBuffer = nullptr;
                const float* normalsBuffer = nullptr;
                const float* texCoordsBuffer = nullptr;
                size_t vertexCount = 0;

                if (prim.attributes.find("POSITION") != prim.attributes.end()) 
                {
                    const auto& accessor = model.accessors[prim.attributes.find("POSITION")->second];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }

                if (prim.attributes.find("NORMAL") != prim.attributes.end()) 
                {
                    const auto& accessor = model.accessors[prim.attributes.find("NORMAL")->second];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    normalsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end()) 
                {
                    const auto& accessor = model.accessors[prim.attributes.find("TEXCOORD_0")->second];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    texCoordsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                const float* tangentsBuffer = nullptr;
                if (prim.attributes.find("TANGENT") != prim.attributes.end())
                {
                    const auto& accessor = model.accessors[prim.attributes.find("TANGENT")->second];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    tangentsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                for (size_t i = 0; i < vertexCount; i++)
                {
                    VU::Vertex vertex {};
                    std::memcpy(&vertex.position, &positionBuffer[i * 3], sizeof(float) * 3);
                    if (normalsBuffer)
                        std::memcpy(&vertex.normals, &normalsBuffer[i * 3], sizeof(float) * 3);
                    vertex.uvs.x = texCoordsBuffer ? texCoordsBuffer[i * 2]     : 0.0f;
                    vertex.uvs.y = texCoordsBuffer ? texCoordsBuffer[i * 2 + 1] : 0.0f;
                    if (tangentsBuffer)
                        std::memcpy(&vertex.tangent, &tangentsBuffer[i * 4], sizeof(float) * 4);
                    else
                        vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);  // fallback

                    req.vertices.push_back(vertex);
                }

                
				const auto& accessor = model.accessors[prim.indices];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				const auto& buffer = model.buffers[bufferView.buffer];

				const auto indexCount = accessor.count;

				const auto FillIndices = [&](const auto& buf)
				{
					for (size_t index = 0; index < accessor.count; index++)
					{
						req.indices.push_back(buf[index]);
					}
				};
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: 
				{
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					FillIndices(buf);
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: 
				{
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					FillIndices(buf);
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: 
				{
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					FillIndices(buf);
					break;
				}
				default:
					printf("[Scene Loader] Error: Index component type %i not supported!\n", accessor.componentType);
					return;
				}

                
                scene.transforms.push_back(worldTransform);

                Entity entity;
                entity.id = static_cast<uint32_t>(scene.entities.size());
                entity.meshId = req.id;
                entity.transformId = static_cast<uint32_t>(scene.transforms.size() - 1);
                entity.materialId = req.materialId;
                scene.entities.push_back(entity);

				reqs.push_back(req);
            }
        }
    }

    VkResult CreateSampler(VkDevice device, int magFilter, int minFilter, int wrapS, int wrapT, int wrapR, VkSampler& sampler)
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        
        // Map GLTF filter values to Vulkan
        samplerInfo.magFilter = magFilter == TINYGLTF_TEXTURE_FILTER_LINEAR ? 
            VK_FILTER_LINEAR : VK_FILTER_NEAREST;

        // GLTF minFilter encodes both the base filter and the mipmap mode in a single value
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        switch (minFilter)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                samplerInfo.minFilter = VK_FILTER_NEAREST;
                mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                samplerInfo.minFilter = VK_FILTER_NEAREST;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                samplerInfo.minFilter = VK_FILTER_LINEAR;
                mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            default:
                samplerInfo.minFilter = VK_FILTER_LINEAR;
                break;
        }
        
        // Map GLTF wrap values to Vulkan
        auto mapWrap = [](int wrap) {
            switch(wrap)
            {
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            }
        };
        
        samplerInfo.addressModeU = mapWrap(wrapS);
        samplerInfo.addressModeV = mapWrap(wrapT);
        samplerInfo.addressModeW = mapWrap(wrapR != -1 ? wrapR : TINYGLTF_TEXTURE_WRAP_REPEAT);
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = mipmapMode;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        return vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    }
}