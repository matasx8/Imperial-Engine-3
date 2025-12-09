#pragma once
#include "Platform.h"

namespace imp
{

    class PlatformImpl : public Platform
    {
    public:
        PlatformImpl() = default;

        virtual bool Initialize(const PlatformInitParams& params) override;
        virtual bool Shutdown(VkInstance instance, VkDevice device) override;

    private:


    };
}