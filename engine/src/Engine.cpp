#include "Engine.h"
#include "Layers.h"
#include "Debug.h"
#include "Platform/PlatformImpl.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <set>

namespace imp
{
    VkCommandBuffer CommandBufferFactory::Create(const CommandBufferFactory::Args& args)
    {
        VkCommandBufferAllocateInfo cbai {};
        cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = args.pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;

        VkCommandBuffer cb;
        VkResult result = vkt.vkAllocateCommandBuffers(args.device, &cbai, &cb);
        if (result != VK_SUCCESS)
            g_Log("Failed to allocate a VkCommandBuffer with result %d\n", result);
        return cb;
    }

    void CommandBufferFactory::Destroy(VkCommandBuffer cb, const CommandBufferFactory::Args& args)
    {
        // No need, can just destroy the VkCommandPool
    }

    static std::vector<const char*> CombineExtensions(const Window& window, uint32_t numReq, const char* const* reqs)
    {
        uint32_t count;
        auto windowExtents = window.GetRequiredDeviceExtensions(&count);

        std::vector<const char*> extents;
        for (uint32_t i = 0; i < count; i++)
            extents.push_back(windowExtents[i]);
        for (uint32_t i = 0; i < numReq; i++)
            extents.push_back(reqs[i]);

        for (auto ext : extents)
            g_Log("Requesting device extension: %s\n", ext);
        
        return extents;
    }

    static VkResult CreateCommandPool(VkDevice device, uint32_t queueFamilyIndex, VkCommandPool* pool)
    {
        VkCommandPoolCreateInfo cpci {};
        cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = queueFamilyIndex;

        VkResult result = vkt.vkCreateCommandPool(device, &cpci, nullptr, pool);
        if (result != VK_SUCCESS)
            g_Log("Failed to create a VkCommandPool with result %d\n", result);
        return result;
    }

    static void DestroyCommandPool(VkDevice device, VkCommandPool pool)
    {
        vkt.vkDestroyCommandPool(device, pool, nullptr);
    }

    static VkResult CreateDescriptorPool(VkDevice device, VkDescriptorPool* pool)
    {
        VkDescriptorPoolSize poolSize {};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 100; // Arbitrary large number

        VkDescriptorPoolCreateInfo dpci {};
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.maxSets = 100; // Arbitrary large number
        dpci.poolSizeCount = 1;
        dpci.pPoolSizes = &poolSize;

        VkResult result = vkt.vkCreateDescriptorPool(device, &dpci, nullptr, pool);
        if (result != VK_SUCCESS)
            g_Log("Failed to create a VkDescriptorPool with result %d\n", result);
        return result;
    }

    VkResult Engine::Initialize(const EngineCreateParams& params)
    {
        m_Platform = new PlatformImpl(); // dont forget destroy
        PlatformInitParams platformInitParams = kDefaultPlatformInitParams;
        if (params.pPlatformInitParams)
            platformInitParams = *params.pPlatformInitParams;

        if (!m_Platform->Initialize(platformInitParams))
            return VK_ERROR_INITIALIZATION_FAILED;

        VkResult result = volkInitialize();
        if (result != VK_SUCCESS)
            return result;
        g_Log("VOLK initialized successfully.\n");

        result = CreateInstance(params);
        if (result != VK_SUCCESS)
            return result;
        g_Log("Vulkan Instance was successfully created.\n");

        volkLoadInstance(m_Instance);

        if (ShouldInitDebugger(m_EnabledInstanceLayers, m_EnabledInstanceExtensions))
        {
            VkResult res = InitializeDebugger(m_Instance);
            if (res != VK_SUCCESS)
                g_Log("Vulkan debugger was not initialized. Return code: %d\n", res);
        }

        result = SelectPhysicalDevice(params);
        if (result != VK_SUCCESS)
            return result;
        g_Log("Vulkan Physical Device was successfully created.");

        std::vector<const char*> requiredDeviceExtensions = CombineExtensions(m_Platform->GetWindow(), params.numRequiredExtensions, params.pRequiredExtensions);
        m_Queue.Initialize(m_PhysicalDevice, requiredDeviceExtensions, &params.requiredFeatures);

        volkLoadDeviceTable(&vkt, m_Queue.GetDevice());

        const auto& queueFamilyIndices = m_Queue.GetQueueFamilyIndices();
        m_GraphicsCommandBufferPool = new CommandBufferPool();
        result = CreateCommandPool(m_Queue.GetDevice(), queueFamilyIndices.graphicsFamily, &m_GraphicsCommandPool);
        if (result != VK_SUCCESS)
            return result;

        if (queueFamilyIndices.graphicsFamily == queueFamilyIndices.computeFamily)
        {
            m_ComputeCommandPool = m_GraphicsCommandPool;
            m_ComputeCommandBufferPool = m_GraphicsCommandBufferPool;
        }
        else
        {
            m_ComputeCommandBufferPool = new CommandBufferPool();
            result = CreateCommandPool(m_Queue.GetDevice(), queueFamilyIndices.computeFamily, &m_ComputeCommandPool);
            if (result != VK_SUCCESS)
                return result;
        }

        result = m_SubmitSyncManager.Initialize(m_Queue.GetDevice());
        if (result != VK_SUCCESS)
            return result;

        result = m_Platform->GetWindow().InitializeSwapchain(m_Instance, m_PhysicalDevice, m_Queue.GetDevice());

        if (result != VK_SUCCESS)
            return result;

        result = CreateDescriptorPool(m_Queue.GetDevice(), &m_DescriptorPool);

        if (result == VK_SUCCESS)
            g_Log("Engine initialized successfully.\n");

        return result;
    }

    VkResult Engine::Shutdown()
    {
        VkResult result;

        m_Platform->Shutdown(m_Instance, m_Queue.GetDevice());

        result = m_SubmitSyncManager.Shutdown(m_Queue.GetDevice());
        if (m_GraphicsCommandPool != m_ComputeCommandPool)
        {
            CommandBufferFactory::Args args;
            args.device = m_Queue.GetDevice();
            args.pool = m_ComputeCommandPool;

            m_ComputeCommandBufferPool->Destroy(args);
            delete m_ComputeCommandBufferPool;
            vkDestroyCommandPool(m_Queue.GetDevice(), m_ComputeCommandPool, nullptr);
        }

        CommandBufferFactory::Args args;
        args.device = m_Queue.GetDevice();
        args.pool = m_GraphicsCommandPool;

        m_GraphicsCommandBufferPool->Destroy(args);
        delete m_GraphicsCommandBufferPool;
        vkDestroyCommandPool(m_Queue.GetDevice(), m_GraphicsCommandPool, nullptr);

        result = m_Queue.ShutDown();
        DestroyDebugger(m_Instance);
        DestroyInstance();
        volkFinalize();
        return result;
    }

    static uint32_t FindNumberOfUniqueQueues(const SubmitParams* pParams, uint32_t paramsCount)
    {
        uint32_t numUniqueQueues = 1;
        if (paramsCount > 1)
        {
            std::set<VkQueue> uniqueQueues;
            for (uint32_t i = 0; i < paramsCount; i++)
                uniqueQueues.insert(pParams[i].queue);
            numUniqueQueues = uniqueQueues.size();
        }
        return numUniqueQueues;
    }

    static SubmitSync CreateFailedSubmitSync()
    {
        return {0, VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    SubmitSync Engine::Submit(const SubmitParams* pParams, uint32_t paramsCount)
    {
        uint32_t numUniqueQueues = FindNumberOfUniqueQueues(pParams, paramsCount);
        
        // Multi Queue Form/Merge Submit
        if (numUniqueQueues > 1)
        {
            std::vector<SubmitSync> syncs;
            syncs.reserve(numUniqueQueues);
            std::vector<VkSemaphore> semaphores;
            semaphores.reserve(numUniqueQueues);
            std::vector<VkPipelineStageFlags> waitMasks {numUniqueQueues, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};

            for (uint32_t i = 0; i < numUniqueQueues; i++)
            {
                syncs.push_back(m_SubmitSyncManager.GetSubmitSync(m_Queue.GetDevice()));
                semaphores.push_back(syncs.back().semaphore);
            }

            const SubmitSync* sync = m_SubmitSyncManager.GetLastSubmitSync();

            VkSubmitInfo emptyForkingSubmit {};
            emptyForkingSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            emptyForkingSubmit.pWaitSemaphores = sync ? &sync->semaphore : nullptr;
            emptyForkingSubmit.waitSemaphoreCount = sync ? 1 : 0;
            emptyForkingSubmit.pWaitDstStageMask = sync ? waitMasks.data() : nullptr;
            emptyForkingSubmit.pSignalSemaphores = semaphores.data();
            emptyForkingSubmit.signalSemaphoreCount = semaphores.size();

            VkResult result = vkt.vkQueueSubmit(pParams->queue, 1, &emptyForkingSubmit, VK_NULL_HANDLE);
            if (result != VK_SUCCESS)
            {
                g_Log("Failed to submit to Queue wit result: &d\n", result);
                return CreateFailedSubmitSync();
            }

            // Insert into timeline, but we won't return these to the user.
            for (uint32_t i = 0; i < numUniqueQueues; i++)
                m_SubmitSyncManager.InsertIntoTimeline(syncs[i]);

            std::vector<VkSubmitInfo> submits;
            submits.reserve(paramsCount);
            std::vector<VkSemaphore> dependenciesForMergeSubmit {numUniqueQueues};
            // SubmitParams must be ordered by thier VkQueues
            int uniqueQueueIndex = 0;
            bool firstSubmitOfNewQueue = true;
            for (uint32_t i = 0; i < paramsCount; i++)
            {
                SubmitSync submitSync = m_SubmitSyncManager.GetSubmitSync(m_Queue.GetDevice());

                VkSubmitInfo submit {};
                submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                if (firstSubmitOfNewQueue)
                    submit.pWaitSemaphores = &semaphores[uniqueQueueIndex];
                else
                    submit.pWaitSemaphores = &m_SubmitSyncManager.GetLastSubmitSync()->semaphore;
                submit.waitSemaphoreCount = 1;
                submit.pWaitDstStageMask = waitMasks.data();
                submit.pSignalSemaphores = &submitSync.semaphore;
                submit.signalSemaphoreCount = 1;
                submit.pCommandBuffers = pParams[i].pCommandBuffers;
                submit.commandBufferCount = pParams[i].commandBufferCount;

                submits.push_back(submit);

                m_SubmitSyncManager.InsertIntoTimeline(submitSync);

                const bool nextSubmitFromDifferentQueue = i < paramsCount - 1 && pParams[i].queue != pParams[i + 1].queue;
                const bool isLastSubmit = i == paramsCount - 1;
                if (nextSubmitFromDifferentQueue || isLastSubmit)
                {
                    result = vkt.vkQueueSubmit(pParams[i].queue, submits.size(), submits.data(), VK_NULL_HANDLE);
                    if (result != VK_SUCCESS)
                    {
                        g_Log("Failed to submit to Queue with result: %d\n", result);
                        return CreateFailedSubmitSync();
                    }

                    dependenciesForMergeSubmit[uniqueQueueIndex] = submitSync.semaphore;
                    uniqueQueueIndex++;
                }
            }

            SubmitSync submitSync = m_SubmitSyncManager.GetSubmitSync(m_Queue.GetDevice());

            VkSubmitInfo emptyMergingSubmit {};
            emptyMergingSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            emptyMergingSubmit.pWaitSemaphores = dependenciesForMergeSubmit.data();
            emptyMergingSubmit.waitSemaphoreCount = dependenciesForMergeSubmit.size();
            emptyMergingSubmit.pWaitDstStageMask = waitMasks.data();
            emptyMergingSubmit.pSignalSemaphores = &submitSync.semaphore;
            emptyMergingSubmit.signalSemaphoreCount = 1;

            result = vkt.vkQueueSubmit(pParams->queue, 1, &emptyMergingSubmit, submitSync.fence);
            if (result != VK_SUCCESS)
            {
                g_Log("Failed to submit to Queue with result: %d\n", result);
                return CreateFailedSubmitSync();
            }

            m_SubmitSyncManager.InsertIntoTimeline(submitSync);
        }

        std::vector<VkSubmitInfo> submits;
        submits.resize(paramsCount);
        VkPipelineStageFlags waitMasks = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        for (uint32_t i = 0; i < paramsCount; i++)
        {
            SubmitSync submitSync = m_SubmitSyncManager.GetSubmitSync(m_Queue.GetDevice());
            const SubmitSync* lastSubmitSync = m_SubmitSyncManager.GetLastSubmitSync();

            auto& si = submits[i];
            si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            si.pWaitSemaphores = lastSubmitSync ? &lastSubmitSync->semaphore : nullptr;
            si.waitSemaphoreCount = lastSubmitSync ? 1 : 0;
            si.pWaitDstStageMask = &waitMasks;
            si.pSignalSemaphores = &submitSync.semaphore;
            si.signalSemaphoreCount = 1;
            si.pCommandBuffers = pParams[i].pCommandBuffers;
            si.commandBufferCount = pParams[i].commandBufferCount;

            m_SubmitSyncManager.InsertIntoTimeline(submitSync);
        }

        const SubmitSync* submitSync = m_SubmitSyncManager.GetLastSubmitSync();
        VkResult result = vkt.vkQueueSubmit(pParams->queue, 1, submits.data(), submitSync->fence);
        if (result != VK_SUCCESS)
        {
            g_Log("Failed to submit to Queue with result: %d\n", result);
            return CreateFailedSubmitSync();
        }

        return *submitSync;
    }

    VkResult Engine::Present(Window& window, uint32_t imageIndex)
    {
        const SubmitSync* lastSubmit = m_SubmitSyncManager.GetLastSubmitSync();
        VkSwapchainKHR swapchain = window.GetSwapchain().GetSwapchain();

        VkPresentInfoKHR pi {};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.pWaitSemaphores = lastSubmit ? &lastSubmit->semaphore : nullptr;
        pi.waitSemaphoreCount = lastSubmit ? 1 : 0;
        pi.pSwapchains = &swapchain;
        pi.swapchainCount = 1;
        pi.pImageIndices = &imageIndex;

        VkResult result = vkt.vkQueuePresentKHR(m_Queue.GetGraphicsQueue(), &pi);
        if (result != VK_SUCCESS)
            g_Log("Failed to present to Swapchain with result %d\n", result);
        return result;
    }

    VkResult Engine::WaitForSubmitSync(const SubmitSync& sync, uint64_t timeout)
    {
        return m_SubmitSyncManager.WaitForSubmitSync(m_Queue.GetDevice(), sync, timeout);
    }

    VkPhysicalDeviceMemoryProperties Engine::GetMemoryProperties() const
    {
        VkPhysicalDeviceMemoryProperties props;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &props);
        return props;
    }

    VkCommandBuffer Engine::AcquireCommandBuffer(CommandBufferType type)
    {
        CommandBufferFactory::Args args { m_Queue.GetDevice() };
        const SubmitSync* lastSubmit = m_SubmitSyncManager.GetLastSubmitSync();
        uint64_t lastSyncedPoint = m_SubmitSyncManager.GetLastSyncedPoint();

        VkCommandBuffer cb = VK_NULL_HANDLE;
        switch (type)
        {
        case CommandBufferType::Graphics:
            args.pool = m_GraphicsCommandPool;
            cb = m_GraphicsCommandBufferPool->Acquire(args, lastSyncedPoint);
            break;
        case CommandBufferType::Compute:
            args.pool = m_ComputeCommandPool;
            cb = m_ComputeCommandBufferPool->Acquire(args, lastSyncedPoint);
            break;

        default:
            break;
        }

        VkCommandBufferBeginInfo cbi {};
        cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult result = vkt.vkBeginCommandBuffer(cb, &cbi);
        if (result != VK_SUCCESS)
            g_Log("Failed to begin a VkCommandBuffer with result %d\n");

        return cb;
    }

    SubmitSync Engine::AcquireNextImage(Window& window, uint32_t* nextImageIndex, uint64_t timeout)
    {
        SubmitSync sync = m_SubmitSyncManager.GetSubmitSync(m_Queue.GetDevice());

        VkSwapchainKHR swapchain = window.GetSwapchain().GetSwapchain();
        VkResult result = vkt.vkAcquireNextImageKHR(m_Queue.GetDevice(), swapchain, timeout,
            sync.semaphore, sync.fence, nextImageIndex);

        if (result != VK_SUCCESS)
        {
            g_Log("Failed to acquire next image from Swapchain with result %d\n", result);
            return CreateFailedSubmitSync();
        }
        m_SubmitSyncManager.InsertIntoTimeline(sync);
        return sync;
    }

    VkResult Engine::CreateInstance(const EngineCreateParams& params)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Imperial Engine 3";
        appInfo.applicationVersion = 1;
        appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 0);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        VkResult result = GetInstanceLayers(m_EnabledInstanceLayers);
        if (result != VK_SUCCESS)
            return result;

        std::vector<const char*> preferredExtensions = g_PreferredInstanceExtensions;

        // additionally retrieve from window
        uint32_t instanceExtensionsFromWindowCount = 0;
        const char* const* instanceExtensionsFromWindow = 
            m_Platform->GetWindow().GetRequiredInstanceExtensions(&instanceExtensionsFromWindowCount);
        preferredExtensions.insert(preferredExtensions.end(), instanceExtensionsFromWindow
            , instanceExtensionsFromWindow + instanceExtensionsFromWindowCount);

        result = GetInstanceExtensions(m_EnabledInstanceLayers, preferredExtensions, m_EnabledInstanceExtensions);
        if (result != VK_SUCCESS)
            return result;

        for (const auto& layer : m_EnabledInstanceLayers)
            g_Log("Will enable layer: %s\n", layer.c_str());

        for (const auto& extension : m_EnabledInstanceExtensions)
            g_Log("Will enable instance extension: %s\n", extension.c_str());

        createInfo.enabledLayerCount = static_cast<uint32_t>(m_EnabledInstanceLayers.size());
        std::vector<const char*> layerNames(m_EnabledInstanceLayers.size());
        std::transform(m_EnabledInstanceLayers.begin(), m_EnabledInstanceLayers.end(), layerNames.begin(), [](const std::string& str) { return str.c_str(); });
        createInfo.ppEnabledLayerNames = layerNames.data();

        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_EnabledInstanceExtensions.size());
        std::vector<const char*> extensionNames(m_EnabledInstanceExtensions.size());
        std::transform(m_EnabledInstanceExtensions.begin(), m_EnabledInstanceExtensions.end(), extensionNames.begin(), [](const std::string& str) { return str.c_str(); });
        createInfo.ppEnabledExtensionNames = extensionNames.data();

        result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
        return result;
    }

    void Engine::DestroyInstance()
    {
        vkDestroyInstance(m_Instance, nullptr);
    }

VkResult Engine::SelectPhysicalDevice(const EngineCreateParams& params)
{
    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS)
        return result;

    if (deviceCount == 0)
    {
        g_Log("Error: Could not find any Vulkan Physical Devices.\n");
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    std::vector<VkPhysicalDevice> devices { deviceCount };
    result = vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS)
        return result;

    VkPhysicalDevice discreteGPU = VK_NULL_HANDLE;
    VkPhysicalDevice integratedGPU = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties discreteProps = {};
    VkPhysicalDeviceProperties integratedProps = {};

    VkPhysicalDeviceProperties props;
    for (const auto& device : devices)
    {
        vkGetPhysicalDeviceProperties(device, &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && discreteGPU == VK_NULL_HANDLE)
        {
            discreteGPU = device;
            discreteProps = props;
        }
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && integratedGPU == VK_NULL_HANDLE)
        {
            integratedGPU = device;
            integratedProps = props;
        }
    }

    if (discreteGPU != VK_NULL_HANDLE)
    {
        m_PhysicalDevice = discreteGPU;
        g_Log("Selected Physical Device (Discrete): \"%s\"\n", discreteProps.deviceName);
        return VK_SUCCESS;
    }
    else if (integratedGPU != VK_NULL_HANDLE)
    {
        m_PhysicalDevice = integratedGPU;
        g_Log("Selected Physical Device (Integrated): \"%s\"\n", integratedProps.deviceName);
        return VK_SUCCESS;
    }

    return VK_ERROR_DEVICE_LOST;
}
}