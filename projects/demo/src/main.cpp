#include "SceneLoader.h"
#include "Engine.h"
#include "QueryManager.h"

#include "shaders/spv/gbuffer_vert.h"
#include "shaders/spv/gbuffer_frag.h"
#include "shaders/spv/lighting_vert.h"
#include "shaders/spv/lighting_frag.h"
#include "shaders/spv/cull_comp.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <string>


int main(int argc, char* argv[])
{
    // Get GLTF scene path from command-line arguments
    std::string scenePath;
    bool windowed = false;
    for (int i = 1; i < argc; i++)
    {
        if (std::string(argv[i]) == "--windowed")
            windowed = true;
        else if (scenePath.empty())
            scenePath = argv[i];
    }
    if (scenePath.empty())
    {
        printf("[Main] Usage: demo.exe <path_to_gltf_scene> [--windowed]\n");
        return 1;
    }

    imp::WindowInitParams windowInitParams {};
    windowInitParams.fullscreen = !windowed;

    imp::PlatformInitParams platformParams {};
    platformParams.pWindowInitParams = &windowInitParams;

    std::initializer_list<const char*> requiredDeviceExtensions = {
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
    };

    imp::EngineCreateParams createParams {};
    createParams.numRequiredExtensions = static_cast<uint32_t>(requiredDeviceExtensions.size());
    createParams.pRequiredExtensions = requiredDeviceExtensions.begin();

    createParams.pPlatformInitParams = &platformParams;

    createParams.requiredFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    createParams.requiredFeatures.features.pipelineStatisticsQuery = VK_TRUE;

    VkPhysicalDeviceVulkan11Features vulkan11Features {};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.storageBuffer16BitAccess = VK_TRUE;
    vulkan11Features.shaderDrawParameters = VK_TRUE;
    createParams.requiredFeatures.pNext = &vulkan11Features;

    VkPhysicalDeviceVulkan12Features features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.drawIndirectCount = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    vulkan11Features.pNext = &features12;

    VkPhysicalDeviceSynchronization2Features synchronization2Features {};
    synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    synchronization2Features.synchronization2 = VK_TRUE;
    features12.pNext = &synchronization2Features;


    imp::Engine engine {};
    VkResult result = engine.Initialize(createParams);

    VkDevice device = engine.GetWorkQueue().GetDevice();

    VkShaderModule gbufferVertModule  = VK_NULL_HANDLE;
    VkShaderModule gbufferFragModule  = VK_NULL_HANDLE;
    VkShaderModule lightingVertModule = VK_NULL_HANDLE;
    VkShaderModule lightingFragModule = VK_NULL_HANDLE;
    VkShaderModule cullCompModule     = VK_NULL_HANDLE;
    if (VU::CreateShaderModule(device, gbuffer_vert,  sizeof(gbuffer_vert),  gbufferVertModule,  "gbuffer_vert")  != VK_SUCCESS) return 0;
    if (VU::CreateShaderModule(device, gbuffer_frag,  sizeof(gbuffer_frag),  gbufferFragModule,  "gbuffer_frag")  != VK_SUCCESS) return 0;
    if (VU::CreateShaderModule(device, lighting_vert, sizeof(lighting_vert), lightingVertModule, "lighting_vert") != VK_SUCCESS) return 0;
    if (VU::CreateShaderModule(device, lighting_frag, sizeof(lighting_frag), lightingFragModule, "lighting_frag") != VK_SUCCESS) return 0;
    if (VU::CreateShaderModule(device, cull_comp,     sizeof(cull_comp),     cullCompModule,     "cull_comp")     != VK_SUCCESS) return 0;

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
    VU::SetupGlobalUniforms(engine, globals, swapchain.GetSwapchainImageCount());

    VU::SceneData scene {};
    VU::InitializeSceneData(engine, scene, scenel);

    VU::RenderingDescriptors renderingData {};
    VU::SetupRenderingDescriptorSet(engine, renderingData, scenel, scene.drawDatas);

    // Geometry pass - fills G-Buffer (also allocates G-Buffer images and framebuffer)
    VU::GBufferPipeline gbufferPipeline {};
    gbufferPipeline.pGlobalUniforms       = &globals;
    gbufferPipeline.pRenderingDescriptors = &renderingData;
    if (VU::CreateGBufferPipeline(engine, gbufferVertModule, gbufferFragModule, gbufferPipeline,
            window.GetWidth(), window.GetHeight()) != VK_SUCCESS)
    {
        printf("[Main] Failed to create G-Buffer pipeline\n");
        engine.Shutdown();
        return 1;
    }

    // Lighting pass - reads G-Buffer, writes to swapchain (also creates per-swapchain framebuffers)
    VU::LightingPipeline lightingPipeline {};
    lightingPipeline.pGlobalUniforms = &globals;
    if (VU::CreateLightingPipeline(engine, lightingVertModule, lightingFragModule, lightingPipeline,
            gbufferPipeline.gbuffer, window.GetWidth(), window.GetHeight()) != VK_SUCCESS)
    {
        printf("[Main] Failed to create lighting pipeline\n");
        engine.Shutdown();
        return 1;
    }

    // drawData, materialBuffer, and boundingSpheres are DEVICE_LOCAL and uploaded once at init.
    // Globals UBO is HOST_VISIBLE but per-frame-in-flight, so no CPU/GPU race exists.
    // Multiple frames in flight are now safe.
    std::vector<imp::SubmitSync> simpleFramePacing {};
    uint32_t frameIndex = 0;
    simpleFramePacing.resize(swapchain.GetSwapchainImageCount());

    VU::IndirectDrawBuffer indirectDrawBuffer {};
    if (VU::BuildIndirectDrawBuffer(engine, scenel, indirectDrawBuffer) != VK_SUCCESS)
    {
        printf("[Main] Failed to build indirect draw buffer\n");
        engine.Shutdown();
        return 1;
    }

    VU::CullingPipeline cullingPipeline {};
    if (VU::CreateCullingPipeline(engine, cullCompModule, indirectDrawBuffer, renderingData, globals, cullingPipeline) != VK_SUCCESS)
    {
        printf("[Main] Failed to create culling pipeline\n");
        engine.Shutdown();
        return 1;
    }

    // QueryManager slot count must match the frame pacing ring-buffer size.
    const uint32_t framesInFlight = static_cast<uint32_t>(simpleFramePacing.size());
    QueryManager queryManager {};
    queryManager.Initialize(engine.GetPhysicalDevice(), device, framesInFlight);

    // Main loop
    uint32_t frameCount = 0;
    auto frameStartTime = std::chrono::high_resolution_clock::now();
    while (!engine.GetPlatform().GetWindow().ShouldClose())
    {
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        double frameTimeMs = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();
        frameStartTime = frameEndTime;
        engine.GetPlatform().GetWindow().UpdateInfo(frameTimeMs);

        engine.PaceFrame(device, simpleFramePacing, frameIndex);

        // Readback queries here: PaceFrame's fence wait guarantees the previous frame
        // finished on the GPU, so slot frameIndex results are valid and not yet reset by the
        // new BeginFrame call below.  Skip until the ring has fully cycled once.
        if (frameCount >= framesInFlight)
            queryManager.ReadbackAndPrint(device, frameIndex);

        uint32_t imageIndex = 0;
        engine.AcquireNextImage(engine.GetPlatform().GetWindow(), &imageIndex);

        VkCommandBuffer cb = engine.AcquireCommandBuffer(imp::CommandBufferType::Graphics);
        queryManager.BeginFrame(cb, frameIndex);

        float delta = static_cast<float>(frameTimeMs / 1000.0);
        VU::UpdateCamera(engine.GetPlatform().GetWindow(), scene, globals.data, delta);
        VU::UpdateGlobalDataDescriptorSetByCopy(engine, globals, frameIndex);
        
        VkViewport viewport {};
        viewport.width    = static_cast<float>(window.GetWidth());
        viewport.height   = static_cast<float>(window.GetHeight());
        viewport.maxDepth = 1.0f;
        VkRect2D scissor { { 0, 0 }, { window.GetWidth(), window.GetHeight() } };

        // ---------------------------------------------------------
        //  Culling pass - frustum cull meshes, write visible draws
        // ---------------------------------------------------------
        VU::CmdBeginLabel(cb, "Culling Pass", 0.5f, 1.0f, 0.3f);
        queryManager.BeginCullingPass(cb, frameIndex);
        VU::DispatchCulling(cb, cullingPipeline, indirectDrawBuffer.maxDrawCount, frameIndex);
        queryManager.EndCullingPass(cb, frameIndex);
        VU::CmdEndLabel(cb);

        // ---------------------------------------------------------
        //  Geometry pass - fill G-Buffer
        // ---------------------------------------------------------
        VU::CmdBeginLabel(cb, "Geometry Pass", 0.2f, 0.6f, 1.0f);
        queryManager.BeginGBufferPass(cb, frameIndex);
        std::array<VkClearValue, 3> gbClear {};
        gbClear[2].depthStencil.depth = 1.0f;  // far plane; color attachments zero-init by default

        VkRenderPassBeginInfo gbRpbi {};
        gbRpbi.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        gbRpbi.renderPass      = gbufferPipeline.renderPass;
        gbRpbi.framebuffer     = gbufferPipeline.framebuffer;
        gbRpbi.renderArea      = { { 0, 0 }, { window.GetWidth(), window.GetHeight() } };
        gbRpbi.clearValueCount = static_cast<uint32_t>(gbClear.size());
        gbRpbi.pClearValues    = gbClear.data();

        vkCmdBeginRenderPass(cb, &gbRpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline.pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline.pipelineLayout, 0, 1, &globals.descriptorSets[frameIndex], 0, nullptr);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline.pipelineLayout, 1, 1, &renderingData.descriptorSet, 0, nullptr);
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &scissor);

        vkCmdBindIndexBuffer(cb, scenel.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirectCount(
            cb,
            cullingPipeline.culledBuffer.commands.buffer, 0,
            cullingPipeline.culledBuffer.countBuffer.buffer, 0,
            cullingPipeline.culledBuffer.maxDrawCount,
            sizeof(VkDrawIndexedIndirectCommand));
        vkCmdEndRenderPass(cb);
        queryManager.EndGBufferPass(cb, frameIndex);
        VU::CmdEndLabel(cb);

        // ---------------------------------------------------------
        //  Lighting pass - PBR shading, write to swapchain
        // ---------------------------------------------------------
        VU::CmdBeginLabel(cb, "Lighting Pass", 1.0f, 0.6f, 0.2f);
        queryManager.BeginLightingPass(cb, frameIndex);
        VkClearValue lightClear {};

        VkRenderPassBeginInfo lightRpbi {};
        lightRpbi.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        lightRpbi.renderPass      = lightingPipeline.renderPass;
        lightRpbi.framebuffer     = lightingPipeline.framebuffers[imageIndex];
        lightRpbi.renderArea      = { { 0, 0 }, { window.GetWidth(), window.GetHeight() } };
        lightRpbi.clearValueCount = 1;
        lightRpbi.pClearValues    = &lightClear;

        vkCmdBeginRenderPass(cb, &lightRpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline.pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline.pipelineLayout, 0, 1, &globals.descriptorSets[frameIndex], 0, nullptr);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline.pipelineLayout, 1, 1, &lightingPipeline.descriptorSet, 0, nullptr);
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &scissor);
        vkCmdDraw(cb, 3, 1, 0, 0);  // fullscreen triangle - no vertex/index buffer
        vkCmdEndRenderPass(cb);
        queryManager.EndLightingPass(cb, frameIndex);
        VU::CmdEndLabel(cb);

        vkEndCommandBuffer(cb);
        imp::SubmitParams submitParams {};
        submitParams.commandBufferCount = 1;
        submitParams.pCommandBuffers = &cb;
        submitParams.queue = engine.GetWorkQueue().GetGraphicsQueue();
        imp::SubmitSync sync = engine.Submit(&submitParams, 1);

        simpleFramePacing[frameIndex] = sync;

        frameIndex = (frameIndex + 1) % simpleFramePacing.size();
        frameCount++;

        engine.Present(engine.GetPlatform().GetWindow(), imageIndex);
    }

    engine.Shutdown();
    return 1;
}