#include "Engine.h"

int main()
{
    imp::WindowInitParams windowInitParams {}; // default

    imp::PlatformInitParams platformParams {};
    platformParams.pWindowInitParams = &windowInitParams;

    imp::EngineCreateParams createParams {};
    createParams.pPlatformInitParams = &platformParams;

    imp::Engine engine {};
    VkResult result = engine.Initialize(createParams);
    return 1;
}