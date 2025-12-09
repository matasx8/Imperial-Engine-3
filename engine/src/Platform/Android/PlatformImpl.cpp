#include "Platform/PlatformImpl.h"
#include "Platform/Android/AndroidPlatform.h"
#include "Platform/Android/WindowAndroid.h"
#include "Log.h"

#include <cassert>

namespace imp
{
    bool PlatformImpl::Initialize(const PlatformInitParams& params)
    {
        g_Log = LogInfo;
        m_Window = new WindowAndroid();
        if (params.pWindowInitParams)
            m_Window->Initialize(*params.pWindowInitParams);
        else
        {
            const WindowInitParams defaultParams {};
            m_Window->Initialize(defaultParams);
        }
        return true;
    }

    bool PlatformImpl::Shutdown(VkInstance instance, VkDevice device)
    {
        m_Window->Shutdown(instance, device);
        delete m_Window;
        return true;
    }
}