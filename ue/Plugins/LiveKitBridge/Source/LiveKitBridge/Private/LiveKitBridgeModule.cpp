#include "LiveKitBridgeModule.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FLiveKitBridgeModule, LiveKitBridge)

void FLiveKitBridgeModule::StartupModule()
{
    // Validate third-party lib presence here if desired.
}

void FLiveKitBridgeModule::ShutdownModule()
{
}
