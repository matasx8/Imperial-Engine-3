#include "vkutilities.h"
#include "Engine.h"

#include "shaders/spv/phong_frag.h"
#include "shaders/spv/phong_vert.h"

#include <array>
#include <chrono>

struct PushConstants
{
    float offsetX;
    float offsetY;
};


int main()
{
    imp::WindowInitParams windowInitParams {}; // default

    imp::PlatformInitParams platformParams {};
    platformParams.pWindowInitParams = &windowInitParams;

    std::initializer_list<const char*> requiredDeviceExtensions = {

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

    VU::Buffer vertexBuffer {};
    VU::Buffer indexBuffer {};
    uint32_t indexCount = 0;
    VU::GenerateCubeMesh(engine.GetPhysicalDevice(), device, vertexBuffer, indexBuffer, indexCount, engine);

    VU::GlobalUniforms globals {};
    VU::SetupGlobalUniforms(engine, globals);

    VU::SceneData scene {};
    VU::InitializeSceneData(engine, scene);

    VU::RenderingDescriptors renderingData {};
    VU::SetupRenderingDescriptorSet(engine, renderingData, vertexBuffer, indexBuffer, 1);

    VU::PhongPipeline phongPipeline {};
    phongPipeline.pGlobalUniforms = &globals;
    phongPipeline.pRenderingDescriptors = &renderingData;
    VU::CreatePhongPipeline(device, vertModule, fragModule, phongPipeline);

    std::vector<VkFramebuffer> framebuffers;
    framebuffers.resize(swapchain.GetSwapchainImageCount());
    for (uint32_t i = 0; i < swapchain.GetSwapchainImageCount(); i++)
    {
        VkImageView attachment = swapchain.GetSwapchainImageView(i);
        VU::CreateFramebuffer(device, phongPipeline.renderPass, 1, &attachment, window.GetWidth(), window.GetHeight(), framebuffers[i]);
    }

    // Main loop
    auto frameStartTime = std::chrono::high_resolution_clock::now();
    while (!engine.GetPlatform().GetWindow().ShouldClose())
    {
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        double frameTimeMs = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();
        frameStartTime = frameEndTime;
        engine.GetPlatform().GetWindow().UpdateInfo(frameTimeMs);

        uint32_t imageIndex = 0;
        engine.AcquireNextImage(engine.GetPlatform().GetWindow(), &imageIndex);

        VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);

        VU::UpdateCamera(scene, globals.data);
        VU::UpdateRenderingDataDescriptorSetByCopy(engine, renderingData, cb, { { glm::mat4(1.0f) } });
        VU::UpdateGlobalDataDescriptorSetByCopy(engine, globals, cb);

        std::array<VkClearValue, 1> clearValues {};
        clearValues[0].color.float32[0] = 0.0f;
        clearValues[0].color.float32[1] = 0.0f;
        clearValues[0].color.float32[2] = 0.0f;
        clearValues[0].color.float32[3] = 1.0f;

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

        vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, phongPipeline.pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, phongPipeline.pipelineLayout, 0, 1, &globals.descriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, phongPipeline.pipelineLayout, 1, 1, &renderingData.descriptorSet, 0, nullptr);
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &rpbi.renderArea);
        vkCmdBindIndexBuffer(cb, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
        vkCmdEndRenderPass(cb);

        vkEndCommandBuffer(cb);
        imp::SubmitParams submitParams {};
        submitParams.commandBufferCount = 1;
        submitParams.pCommandBuffers = &cb;
        submitParams.queue = engine.GetWorkQueue().GetGraphicsQueue();
        engine.Submit(&submitParams, 1);

        engine.Present(engine.GetPlatform().GetWindow(), imageIndex);
    }

    engine.Shutdown();
    return 1;
}