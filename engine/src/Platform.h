#pragma once
#include "Window.h"
#include "Log.h"
#include "volk.h"

namespace imp
{
    struct PlatformInitParams
    {
        void* pNext;
        WindowInitParams* pWindowInitParams;
    };

    inline static constexpr PlatformInitParams kDefaultPlatformInitParams {};

    class Platform
    {
    public:
        Platform() = default;
        virtual ~Platform() = default;

        Platform(const Platform&) = delete;
        Platform operator=(const Platform&) = delete;

        virtual bool Initialize(const PlatformInitParams& params);
        virtual bool Shutdown(VkInstance instance, VkDevice device);

        Window& GetWindow() { return *m_Window; }

    protected:

        Window* m_Window = nullptr;
    };
}