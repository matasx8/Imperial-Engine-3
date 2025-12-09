#include "Platform/PlatformImpl.h"
#include "Platform/Windows/WindowGLFW.h"
#include "Log.h"

namespace imp
{
    bool PlatformImpl::Initialize(const PlatformInitParams& params)
    {
        m_Window = new WindowGLFW();
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