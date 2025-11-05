#pragma once
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FLiveKitBridgeModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

// Ensures the LiveKit FFI DLL is loaded at runtime.
// Safe to call multiple times; returns true if loaded or unnecessary.
bool LiveKitEnsureFfiLoaded();
