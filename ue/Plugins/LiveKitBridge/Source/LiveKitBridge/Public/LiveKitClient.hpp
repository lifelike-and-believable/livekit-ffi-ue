#pragma once
#include <cstdint>
#include <cstddef>
#include "livekit_ffi.h"

class LiveKitClient
{
public:
    LiveKitClient() : Handle(lk_client_create()) {}
    ~LiveKitClient() { if (Handle) lk_client_destroy(Handle); }

    bool Connect(const char* Url, const char* Token)
    {
        LkResult r = lk_connect(Handle, Url, Token);
        return r.code == 0;
    }

    bool Disconnect()
    {
        LkResult r = lk_disconnect(Handle);
        return r.code == 0;
    }

    bool PublishPCM(const int16_t* Interleaved, size_t FramesPerChannel, int32_t Channels, int32_t SampleRate)
    {
        LkResult r = lk_publish_audio_pcm_i16(Handle, Interleaved, FramesPerChannel, Channels, SampleRate);
        return r.code == 0;
    }

    bool SendData(const void* Bytes, size_t Len, bool bReliable)
    {
        LkResult r = lk_send_data(Handle, static_cast<const uint8_t*>(Bytes), Len, bReliable ? LkReliable : LkLossy);
        return r.code == 0;
    }

private:
    LkClientHandle* Handle = nullptr;
};
