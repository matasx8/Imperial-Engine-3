#include "Debug.h"
#include "Log.h"

namespace imp
{
    static VkDebugUtilsMessengerEXT g_DebugMessenger;

    static void logMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, const char* msgType, const char* msg)
    {
        switch (messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            g_Log("[VVL] [Error] %s %s\n", msgType, msg);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            g_Log("[VVL] [Warning] %s %s\n", msgType, msg);
            break;
        default:
            g_Log("[VVL] [Info] %s %s\n", msgType, msg);
            break;
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugLogCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        switch (messageType)
        {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            logMessage(messageSeverity, "[Validation] %s\n", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            logMessage(messageSeverity, "[Performance] %s\n", pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            logMessage(messageSeverity, "[General] %s\n", pCallbackData->pMessage);
            break;
        default:
            logMessage(messageSeverity, "[Unknown] %s\n", pCallbackData->pMessage);
            break;
        }

        return VK_TRUE;
    }

    bool ShouldInitDebugger(const std::vector<std::string>& enabledLayers, const std::vector<std::string>& enabledExtensions)
    {
        for (const auto& layer : enabledLayers)
            if (layer == "VK_LAYER_KHRONOS_validation")
            {
                for (const auto& extension : enabledExtensions)
                    if (extension == "VK_EXT_debug_utils")
                        return true;
                break;
            }
        return false;
    }

    VkResult InitializeDebugger(VkInstance instance)
    {
        VkDebugUtilsMessengerCreateInfoEXT ci {};
        ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        ci.pfnUserCallback = DebugLogCallback;

        return vkCreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &g_DebugMessenger);
    }

    void DestroyDebugger(VkInstance instance)
    {
        if (g_DebugMessenger)
        {
            vkDestroyDebugUtilsMessengerEXT(instance, g_DebugMessenger, nullptr);
            g_DebugMessenger = VK_NULL_HANDLE;
        }
    }
}