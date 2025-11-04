#pragma once
#include <cstdint>
#include <cstddef>
#include "CoreMinimal.h"
#include "livekit_ffi.h"

class LiveKitClient
{
public:
    LiveKitClient() : Handle(lk_client_create()) {}
    ~LiveKitClient() { if (Handle) lk_client_destroy(Handle); }

    bool Connect(const char* Url, const char* Token)
    {
        LkResult r = lk_connect(Handle, Url, Token);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            if (r.message) { UE_LOG(LogTemp, Error, TEXT("LiveKit connect failed: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            // Free any message if provided by the FFI to avoid leaks.
            lk_free_str((char*)r.message);
        }
        return ok;
    }

    bool Disconnect()
    {
        LkResult r = lk_disconnect(Handle);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit disconnect: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
        }
        return ok;
    }

    bool PublishPCM(const int16_t* Interleaved, size_t FramesPerChannel, int32_t Channels, int32_t SampleRate)
    {
        LkResult r = lk_publish_audio_pcm_i16(Handle, Interleaved, FramesPerChannel, Channels, SampleRate);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit publish audio: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
        }
        return ok;
    }

    bool SendData(const void* Bytes, size_t Len, bool bReliable)
    {
        LkResult r = lk_send_data(Handle, static_cast<const uint8_t*>(Bytes), Len, bReliable ? LkReliable : LkLossy);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit send data: %s"), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
        }
        return ok;
    }

private:
    LkClientHandle* Handle = nullptr;
};
