#include "SceneLoader.h"
#include "Engine.h"

#include "shaders/spv/phong_frag.h"
#include "shaders/spv/phong_vert.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <string>

struct PushConstants
{
    float offsetX;
    float offsetY;
};


int main(int argc, char* argv[])
{
    // Get GLTF scene path from command-line arguments
    std::string scenePath;
    if (argc > 1)
    {
        scenePath = argv[1];
    }
    else
    {
        printf("[Main] Usage: demo.exe <path_to_gltf_scene>\n");
        return 1;
    }

    imp::WindowInitParams windowInitParams {}; // default

    imp::PlatformInitParams platformParams {};
    platformParams.pWindowInitParams = &windowInitParams;

    std::initializer_list<const char*> requiredDeviceExtensions = {
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };

    imp::EngineCreateParams createParams {};
    createParams.numRequiredExtensions = static_cast<uint32_t>(requiredDeviceExtensions.size());
    createParams.pRequiredExtensions = requiredDeviceExtensions.begin();

    createParams.pPlatformInitParams = &platformParams;

    createParams.requiredFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceVulkan11Features vulkan11Features {};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.storageBuffer16BitAccess = VK_TRUE;
    vulkan11Features.shaderDrawParameters = VK_TRUE;
    createParams.requiredFeatures.pNext = &vulkan11Features;

    VkPhysicalDeviceVulkan12Features features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.drawIndirectCount = VK_TRUE;
    vulkan11Features.pNext = &features12;

    VkPhysicalDeviceSynchronization2Features synchronization2Features {};
    synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2Features.synchronization2 = VK_TRUE;
    features12.pNext = &synchronization2Features;


    imp::Engine engine {};
    VkResult result = engine.Initialize(createParams);

    VkDevice device = engine.GetWorkQueue().GetDevice();

    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;
    if (VU::CreateShaderModule(device, phong_vert, sizeof(phong_vert), vertModule) != VK_SUCCESS)
        return 0;
    if (VU::CreateShaderModule(device, phong_frag, sizeof(phong_frag), fragModule) != VK_SUCCESS)
        return 0;

    imp::Swapchain& swapchain = engine.GetPlatform().GetWindow().GetSwapchain();
    imp::Window& window = engine.GetPlatform().GetWindow();

    // Load GLTF scene
    SceneLoader::Scene scenel {};
    if (!SceneLoader::LoadScene(scenePath, engine, scenel))
    {
        printf("[Main] Failed to load GLTF scene: %s\n", scenePath.c_str());
        engine.Shutdown();
        return 1;
    }

    VU::GlobalUniforms globals {};
    VU::SetupGlobalUniforms(engine, globals);

    VU::SceneData scene {};
    VU::InitializeSceneData(engine, scene);

    VU::RenderingDescriptors renderingData {};
    VU::SetupRenderingDescriptorSet(engine, renderingData, scenel.vertexBuffer, scenel.indexBuffer, 1);

    VU::PhongPipeline phongPipeline {};
    phongPipeline.pGlobalUniforms = &globals;
    phongPipeline.pRenderingDescriptors = &renderingData;
    VU::CreatePhongPipeline(device, vertModule, fragModule, phongPipeline);

    std::vector<VkFramebuffer> framebuffers;
    framebuffers.resize(swapchain.GetSwapchainImageCount());
    VU::CreateImage(engine.GetPhysicalDevice(), device, window.GetWidth(), window.GetHeight(),
        VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        phongPipeline.depthImage);
    VU::CreateImageView(device, phongPipeline.depthImage.image, VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_ASPECT_DEPTH_BIT, phongPipeline.depthImageView);
    for (uint32_t i = 0; i < swapchain.GetSwapchainImageCount(); i++)
    {
        VkImageView attachment = swapchain.GetSwapchainImageView(i);
        std::array<VkImageView, 2> attachments = { attachment, phongPipeline.depthImageView };
        VU::CreateFramebuffer(device, phongPipeline.renderPass, attachments.size(), attachments.data(), window.GetWidth(), window.GetHeight(), framebuffers[i]);
    }

    // Setting up simples form of frame pacing
    std::vector<imp::SubmitSync> simpleFramePacing {};
    uint32_t frameIndex = 0;
    simpleFramePacing.resize(swapchain.GetSwapchainImageCount());

    // Main loop
    auto frameStartTime = std::chrono::high_resolution_clock::now();
    while (!engine.GetPlatform().GetWindow().ShouldClose())
    {
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        double frameTimeMs = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();
        frameStartTime = frameEndTime;
        engine.GetPlatform().GetWindow().UpdateInfo(frameTimeMs);
        
        engine.PaceFrame(device, simpleFramePacing, frameIndex);

        uint32_t imageIndex = 0;
        engine.AcquireNextImage(engine.GetPlatform().GetWindow(), &imageIndex);

        VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);

        float delta = static_cast<float>(frameTimeMs / 1000.0);
        VU::UpdateCamera(engine.GetPlatform().GetWindow(), scene, globals.data, delta);
        VU::UpdateRenderingDataDescriptorSetByCopy(engine, renderingData, cb, { { glm::mat4(1.0f) } });
        VU::UpdateGlobalDataDescriptorSetByCopy(engine, globals, cb);

        std::array<VkClearValue, 2> clearValues {};
        clearValues[0].color.float32[0] = 0.0f;
        clearValues[0].color.float32[1] = 0.0f;
        clearValues[0].color.float32[2] = 0.0f;
        clearValues[0].color.float32[3] = 1.0f;
        clearValues[1].depthStencil.depth = 1.0f;

        VkViewport viewport {};
        viewport.width = static_cast<float>(window.GetWidth());
        viewport.height = static_cast<float>(window.GetHeight());
        viewport.maxDepth = 1.0f;

        VkRenderPassBeginInfo rpbi {};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = phongPipeline.renderPass;
        rpbi.framebuffer = framebuffers[imageIndex];
        rpbi.renderArea.offset = { 0, 0 };
        rpbi.renderArea.extent = { window.GetWidth(), window.GetHeight() };
        rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size());
        rpbi.pClearValues = clearValues.data();

        std::array<VkImageMemoryBarrier, 2> imageBarriers {};
        imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarriers[0].srcAccessMask = 0;
        imageBarriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageBarriers[0].image = swapchain.GetSwapchainImage(imageIndex);
        imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarriers[0].subresourceRange.levelCount = 1;
        imageBarriers[0].subresourceRange.layerCount = 1;
        imageBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarriers[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        imageBarriers[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        imageBarriers[1].image = phongPipeline.depthImage.image;
        imageBarriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageBarriers[1].subresourceRange.levelCount = 1;
        imageBarriers[1].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());

        vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, phongPipeline.pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, phongPipeline.pipelineLayout, 0, 1, &globals.descriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, phongPipeline.pipelineLayout, 1, 1, &renderingData.descriptorSet, 0, nullptr);
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &rpbi.renderArea);
        vkCmdBindIndexBuffer(cb, scenel.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, scenel.indexCount, 1, 0, 0, 0);
        vkCmdEndRenderPass(cb);
      
        vkEndCommandBuffer(cb);
        imp::SubmitParams submitParams {};
        submitParams.commandBufferCount = 1;
        submitParams.pCommandBuffers = &cb;
        submitParams.queue = engine.GetWorkQueue().GetGraphicsQueue();
        imp::SubmitSync sync = engine.Submit(&submitParams, 1);

        simpleFramePacing[frameIndex] = sync;
        frameIndex = (frameIndex + 1) % simpleFramePacing.size();

        engine.Present(engine.GetPlatform().GetWindow(), imageIndex);
    }

    engine.Shutdown();
    return 1;
}