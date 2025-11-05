#pragma once
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FLiveKitBridgeModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
