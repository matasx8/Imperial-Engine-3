#include "Platform.h"

#include <cstdarg>

namespace imp
{
    bool Platform::Initialize(const PlatformInitParams& params)
    {
        
        return true;
    }

    bool Platform::Shutdown(VkInstance instance, VkDevice device)
    {
        return true;
    }
}