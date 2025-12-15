#pragma once

#include "VulkanFunctionTable.h"
#include "Log.h"
#include "Queue.h"
#include "SubmitSyncManager.h"
#include "Platform.h"
#include "SafeResourceDestroyer.h"

#include <vector>
#include <string>
#include <limits>

namespace imp
{
    struct EngineCreateParams
    {
        const void* pNext;
        const PlatformInitParams* pPlatformInitParams;
        uint32_t numRequiredExtensions;
        const char* const* pRequiredExtensions;
        VkPhysicalDeviceFeatures2 requiredFeatures;
    };

    struct SubmitParams
    {
        VkQueue queue; // Probably need to wrap VkQueue so I can differentiate between types of queues
        const VkCommandBuffer* pCommandBuffers;
        uint32_t commandBufferCount;
    };

    enum class CommandBufferType
    {
        Graphics,
        Compute
    };

    struct CommandBufferFactory
    {
        struct Args
        {
            VkDevice device;
            VkCommandPool pool;
        };

        static VkCommandBuffer Create(const Args& args);
        static void Destroy(VkCommandBuffer cb, const Args& args);
    };

    typedef PrimitiveInTimelinePool<VkCommandBuffer, CommandBufferFactory, CommandBufferFactory::Args> CommandBufferPool;

    class Engine
    {
    public:
        Engine() = default;

        VkResult Initialize(const EngineCreateParams& params);
        VkResult Shutdown();

        SubmitSync Submit(const SubmitParams* pParams, uint32_t paramsCount);
        SubmitSync AcquireNextImage(Window& window, uint32_t* nextImageIndex, uint64_t timeout = ULLONG_MAX);
        VkResult Present(Window& window, uint32_t imageIndex);
        VkResult WaitForSubmitSync(const SubmitSync& sync, uint64_t timeout = ULLONG_MAX);

        inline Platform& GetPlatform() { return *m_Platform; }
        inline Queue& GetWorkQueue() { return m_Queue; }
        inline VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkPhysicalDeviceMemoryProperties GetMemoryProperties() const;
        inline SafeResourceDestroyer& GetSafeResourceDestroyer() { return m_SafeResourceDestroyer; }
        inline VkDescriptorPool GetDescriptorPool() const { return m_DescriptorPool; }

        // Command buffers will be automatically recycled when they are submitted
        VkCommandBuffer AcquireCommandBuffer(CommandBufferType type);
        
    private:

        VkResult CreateInstance(const EngineCreateParams& params);
        void DestroyInstance();

        VkResult SelectPhysicalDevice(const EngineCreateParams& params);

        VkInstance m_Instance = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        Platform* m_Platform = nullptr;

        std::vector<std::string> m_EnabledInstanceLayers = {};
        std::vector<std::string> m_EnabledInstanceExtensions = {};

        // May be the same pool as Compute
        VkCommandPool m_GraphicsCommandPool = VK_NULL_HANDLE;
        CommandBufferPool* m_GraphicsCommandBufferPool = nullptr;
        // May be the same pool as Graphics
        VkCommandPool m_ComputeCommandPool = VK_NULL_HANDLE;
        CommandBufferPool* m_ComputeCommandBufferPool = nullptr;

        SubmitSyncManager m_SubmitSyncManager = {};

        SafeResourceDestroyer m_SafeResourceDestroyer = {};

        Queue m_Queue = {};
    };   
}