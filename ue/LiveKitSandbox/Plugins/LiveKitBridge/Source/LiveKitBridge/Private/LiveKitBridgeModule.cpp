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
    // Returns last system error as human-readable string
    const uint32 Err = FPlatformMisc::GetLastError();
    TCHAR Buffer[1024] = {0};
    FPlatformMisc::GetSystemErrorMessage(Buffer, UE_ARRAY_COUNT(Buffer), static_cast<int32>(Err));
    return FString(Buffer);
}

void FLiveKitBridgeModule::StartupModule()
{
    // Proactively load the delay-loaded DLL so we can log a clear error if it fails.
    // This also ensures delay-load thunks succeed later during gameplay.
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
    const FString DllPath = FPaths::Combine(BaseDir, TEXT("Binaries"), TEXT("Win64"), TEXT("livekit_ffi.dll"));

    if (!FPaths::FileExists(DllPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("LiveKit FFI DLL not found at '%s'"), *DllPath);
    }

    GLiveKitFfiDllHandle = FPlatformProcess::GetDllHandle(*DllPath);
    if (!GLiveKitFfiDllHandle)
    {
        const FString Err = GetSystemError();
        UE_LOG(LogTemp, Error, TEXT("Failed to load LiveKit FFI DLL from '%s': %s"), *DllPath, *Err);

        // Fallback: try loading by name in case it was copied to another directory on PATH
        GLiveKitFfiDllHandle = FPlatformProcess::GetDllHandle(TEXT("livekit_ffi.dll"));
        if (!GLiveKitFfiDllHandle)
        {
            const FString Err2 = GetSystemError();
            UE_LOG(LogTemp, Error, TEXT("Fallback LoadLibrary('livekit_ffi.dll') also failed: %s"), *Err2);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("Loaded LiveKit FFI DLL from PATH successfully (fallback)."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Loaded LiveKit FFI DLL: %s"), *DllPath);
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

bool LiveKitEnsureFfiLoaded()
{
    const FScopeLock Lock(&GLiveKitFfiMutex);
    if (GLiveKitFfiDllHandle)
    {
        return true;
    }

#if PLATFORM_WINDOWS
    const FString PluginName = TEXT("LiveKitBridge");
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
    if (Plugin.IsValid())
    {
        const FString BaseDir = Plugin->GetBaseDir();
        const FString BinariesPath = FPaths::Combine(BaseDir, TEXT("Binaries"), TEXT("Win64"), TEXT("livekit_ffi.dll"));
        const FString ThirdPartyBinPath = FPaths::Combine(BaseDir, TEXT("ThirdParty"), TEXT("livekit_ffi"), TEXT("bin"), TEXT("Win64"), TEXT("Release"), TEXT("livekit_ffi.dll"));

        FString TryPaths[2] = { BinariesPath, ThirdPartyBinPath };
        for (const FString& DllPath : TryPaths)
        {
            if (FPaths::FileExists(DllPath))
            {
                GLiveKitFfiDllHandle = FPlatformProcess::GetDllHandle(*DllPath);
                if (GLiveKitFfiDllHandle)
                {
                    UE_LOG(LogTemp, Verbose, TEXT("EnsureFfiLoaded: Loaded LiveKit FFI DLL: %s"), *DllPath);
                    return true;
                }
            }
        }

        // Fallback by name via PATH
        GLiveKitFfiDllHandle = FPlatformProcess::GetDllHandle(TEXT("livekit_ffi.dll"));
        if (GLiveKitFfiDllHandle)
        {
            UE_LOG(LogTemp, Verbose, TEXT("EnsureFfiLoaded: Loaded LiveKit FFI DLL via PATH"));
            return true;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EnsureFfiLoaded: LiveKitBridge plugin descriptor not found"));
    }
#endif
    return false;
}
