#pragma once
#include <cstdint>
#include <cstddef>
#include "CoreMinimal.h"
#include "livekit_ffi.h"
#include "LiveKitBridgeModule.h"

class LiveKitClient
{
public:
    LiveKitClient()
    {
        if (!LiveKitEnsureFfiLoaded())
        {
            UE_LOG(LogTemp, Error, TEXT("LiveKit FFI DLL not loaded; FFI calls may fail"));
        }
        Handle = lk_client_create();
    }
    ~LiveKitClient() { if (Handle) lk_client_destroy(Handle); }

    bool Connect(const char* Url, const char* Token)
    {
        LkResult r = lk_connect(Handle, Url, Token);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            CaptureError(r);
            if (r.message) { UE_LOG(LogTemp, Error, TEXT("LiveKit connect failed: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            // Free any message if provided by the FFI to avoid leaks.
            lk_free_str((char*)r.message);
            ClearError();
        }
        return ok;
    }

    bool ConnectWithRole(const char* Url, const char* Token, LkRole Role)
    {
        LkResult r = lk_connect_with_role(Handle, Url, Token, Role);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            CaptureError(r);
            if (r.message) { UE_LOG(LogTemp, Error, TEXT("LiveKit connect (role) failed: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
            ClearError();
        }
        return ok;
    }

    bool Disconnect()
    {
        LkResult r = lk_disconnect(Handle);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            CaptureError(r);
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit disconnect: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
            ClearError();
        }
        return ok;
    }

    bool PublishPCM(const int16_t* Interleaved, size_t FramesPerChannel, int32_t Channels, int32_t SampleRate)
    {
        LkResult r = lk_publish_audio_pcm_i16(Handle, Interleaved, FramesPerChannel, Channels, SampleRate);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            CaptureError(r);
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit publish audio: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
            ClearError();
        }
        return ok;
    }

    bool SendData(const void* Bytes, size_t Len, bool bReliable)
    {
        LkResult r = lk_send_data(Handle, static_cast<const uint8_t*>(Bytes), Len, bReliable ? LkReliable : LkLossy);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            CaptureError(r);
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit send data: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
            ClearError();
        }
        return ok;
    }

    bool SetDataCallback(LkDataCallback Cb, void* User)
    {
        LkResult r = lk_client_set_data_callback(Handle, Cb, User);
        const bool ok = (r.code == 0);
        if (!ok) { CaptureError(r); if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit set data callback: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); } }
        else if (r.message) { lk_free_str((char*)r.message); ClearError(); }
        return ok;
    }

    bool SetAudioCallback(LkAudioCallback Cb, void* User)
    {
        LkResult r = lk_client_set_audio_callback(Handle, Cb, User);
        const bool ok = (r.code == 0);
        if (!ok) { CaptureError(r); if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit set audio callback: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); } }
        else if (r.message) { lk_free_str((char*)r.message); ClearError(); }
        return ok;
    }

    bool SetConnectionCallback(LkConnectionCallback Cb, void* User)
    {
        LkResult r = lk_set_connection_callback(Handle, Cb, User);
        const bool ok = (r.code == 0);
        if (!ok) { CaptureError(r); if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit set connection callback: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); } }
        else if (r.message) { lk_free_str((char*)r.message); ClearError(); }
        return ok;
    }

    bool IsReady() const
    {
        return Handle && lk_client_is_ready(Handle) != 0;
    }

    bool ConnectAsyncWithRole(const char* Url, const char* Token, LkRole Role)
    {
        LkResult r = lk_connect_with_role_async(Handle, Url, Token, Role);
        const bool ok = (r.code == 0);
        if (!ok) { CaptureError(r); if (r.message) { UE_LOG(LogTemp, Error, TEXT("LiveKit connect async: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); } }
        else if (r.message) { lk_free_str((char*)r.message); ClearError(); }
        return ok;
    }

    int GetLastErrorCode() const { return LastCode; }
    FString GetLastErrorMessage() const { return LastMessage; }

private:
    LkClientHandle* Handle = nullptr;
    int LastCode = 0;
    FString LastMessage;

    void CaptureError(const LkResult& R)
    {
        LastCode = R.code;
        if (R.message) { LastMessage = UTF8_TO_TCHAR(R.message); } else { LastMessage.Reset(); }
    }
    void ClearError()
    {
        LastCode = 0;
        LastMessage.Reset();
    }
};
