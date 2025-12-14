#include "vkutilities.h"
#include "Engine.h"

#include "shaders/spv/triangle_frag.h"
#include "shaders/spv/triangle_vert.h"

#include <array>
#include <chrono>

struct PushConstants
{
    float offsetX;
    float offsetY;
};

VkResult CreatePipeline(VkDevice device, VkShaderModule vertModule, VkShaderModule fragModule, VkPipelineLayout& pipelineLayout, VkPipeline& pipeline, VkRenderPass& renderPass)
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
    pushConstantRange.size = sizeof(PushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo plci {};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pushConstantRange;
    VkResult result = vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout);
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
    prsi.cullMode = VK_CULL_MODE_NONE;
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

    result = vkCreateRenderPass(device, &prci, nullptr, &renderPass);
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
    gpci.layout = pipelineLayout;
    gpci.renderPass = renderPass;

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline);
    return result;
};

int main()
{
    imp::WindowInitParams windowInitParams {}; // default

    imp::PlatformInitParams platformParams {};
    platformParams.pWindowInitParams = &windowInitParams;

    std::initializer_list<const char*> requiredDeviceExtensions = {
        //"VK_KHR_dynamic_rendering"
    };

    imp::EngineCreateParams createParams {};
    createParams.numRequiredExtensions = static_cast<uint32_t>(requiredDeviceExtensions.size());
    createParams.pRequiredExtensions = requiredDeviceExtensions.begin();

    createParams.pPlatformInitParams = &platformParams;

    imp::Engine engine {};
    VkResult result = engine.Initialize(createParams);

    VkDevice device = engine.GetWorkQueue().GetDevice();

    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;
    if (VU::CreateShaderModule(device, triangle_vert, sizeof(triangle_vert), vertModule) != VK_SUCCESS)
        return 0;
    if (VU::CreateShaderModule(device, triangle_frag, sizeof(triangle_frag), fragModule) != VK_SUCCESS)
        return 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    CreatePipeline(device, vertModule, fragModule, pipelineLayout, pipeline, renderPass);

    imp::Swapchain& swapchain = engine.GetPlatform().GetWindow().GetSwapchain();
    imp::Window& window = engine.GetPlatform().GetWindow();

    std::vector<VkFramebuffer> framebuffers;
    framebuffers.resize(swapchain.GetSwapchainImageCount());
    for (uint32_t i = 0; i < swapchain.GetSwapchainImageCount(); i++)
    {
        VkImageView attachment = swapchain.GetSwapchainImageView(i);
        VU::CreateFramebuffer(device, renderPass, 1, &attachment, window.GetWidth(), window.GetHeight(), framebuffers[i]);
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
        rpbi.renderPass = renderPass;
        rpbi.framebuffer = framebuffers[imageIndex];
        rpbi.renderArea.offset = { 0, 0 };
        rpbi.renderArea.extent = { window.GetWidth(), window.GetHeight() };
        rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size());
        rpbi.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &rpbi.renderArea);
        vkCmdDraw(cb, 3, 1, 0, 0);
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