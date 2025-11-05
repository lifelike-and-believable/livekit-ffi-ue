#include "LiveKitBridgeModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"

IMPLEMENT_MODULE(FLiveKitBridgeModule, LiveKitBridge)

static void* GLiveKitFfiDllHandle = nullptr;
static FCriticalSection GLiveKitFfiMutex;

static FString GetSystemError()
{
    const uint32 Err = FPlatformMisc::GetLastError();
    TCHAR Buffer[1024] = {0};
    FPlatformMisc::GetSystemErrorMessage(Buffer, UE_ARRAY_COUNT(Buffer), static_cast<int32>(Err));
    return FString(Buffer);
}

void FLiveKitBridgeModule::StartupModule()
{
    const FScopeLock Lock(&GLiveKitFfiMutex);
    if (GLiveKitFfiDllHandle)
    {
        return; // already loaded
    }

#if PLATFORM_WINDOWS
    const FString PluginName = TEXT("LiveKitBridge");
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
    if (!Plugin.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: Plugin descriptor not found; skipping FFI DLL preload"), *PluginName);
        return;
    }

    const FString BaseDir = Plugin->GetBaseDir();
    const FString BinariesPath = FPaths::Combine(BaseDir, TEXT("Binaries"), TEXT("Win64"), TEXT("livekit_ffi.dll"));
    const FString ThirdPartyBinPath = FPaths::Combine(BaseDir, TEXT("Source"), TEXT("LiveKitBridge"), TEXT("ThirdParty"), TEXT("livekit_ffi"), TEXT("bin"), TEXT("Win64"), TEXT("Release"), TEXT("livekit_ffi.dll"));

    FString TryPaths[2] = { BinariesPath, ThirdPartyBinPath };

    for (const FString& DllPath : TryPaths)
    {
        if (FPaths::FileExists(DllPath))
        {
            GLiveKitFfiDllHandle = FPlatformProcess::GetDllHandle(*DllPath);
            if (GLiveKitFfiDllHandle)
            {
                UE_LOG(LogTemp, Display, TEXT("Loaded LiveKit FFI DLL: %s"), *DllPath);
                break;
            }
            else
            {
                const FString Err = GetSystemError();
                UE_LOG(LogTemp, Warning, TEXT("Failed to load LiveKit FFI DLL from '%s': %s"), *DllPath, *Err);
            }
        }
    }

    if (!GLiveKitFfiDllHandle)
    {
        // Fallback: try loading by name in case it was copied to another directory on PATH
        GLiveKitFfiDllHandle = FPlatformProcess::GetDllHandle(TEXT("livekit_ffi.dll"));
        if (!GLiveKitFfiDllHandle)
        {
            const FString Err2 = GetSystemError();
            UE_LOG(LogTemp, Error, TEXT("LoadLibrary fallback for 'livekit_ffi.dll' failed: %s"), *Err2);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Loaded LiveKit FFI DLL from PATH successfully (fallback)."));
        }
    }
#endif // PLATFORM_WINDOWS
}

void FLiveKitBridgeModule::ShutdownModule()
{
    const FScopeLock Lock(&GLiveKitFfiMutex);
    if (GLiveKitFfiDllHandle)
    {
        FPlatformProcess::FreeDllHandle(GLiveKitFfiDllHandle);
        GLiveKitFfiDllHandle = nullptr;
    }
}
