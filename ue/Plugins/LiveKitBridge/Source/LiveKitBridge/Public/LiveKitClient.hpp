#pragma once
#include <cstdint>
#include <cstddef>
#include "CoreMinimal.h"
#include "livekit_ffi.h"
#include "LiveKitBridgeModule.h"

class LiveKitClient;

class LiveKitDataChannel
{
public:
    LiveKitDataChannel() = default;
    LiveKitDataChannel(LiveKitClient* InClient, const FString& InLabel, LkReliability InReliability, bool bInOrdered);

    bool IsValid() const { return Client != nullptr && !Label.IsEmpty(); }
    bool IsReliable() const { return Reliability == LkReliable; }
    const FString& GetLabel() const { return Label; }

    bool Send(const void* Bytes, size_t Len) const;
    bool Send(const TArray<uint8>& Payload) const;

private:
    LiveKitClient* Client = nullptr;
    FString Label;
    LkReliability Reliability = LkReliable;
    bool bOrdered = true;
};

class LiveKitAudioTrack
{
public:
    LiveKitAudioTrack() = default;
    LiveKitAudioTrack(LiveKitClient* InClient, LkAudioTrackHandle* InHandle, const FString& InName, int32 InSampleRate, int32 InChannels, int32 InBufferMs);
    ~LiveKitAudioTrack() { Reset(); }

    LiveKitAudioTrack(const LiveKitAudioTrack&) = delete;
    LiveKitAudioTrack& operator=(const LiveKitAudioTrack&) = delete;

    LiveKitAudioTrack(LiveKitAudioTrack&& Other) noexcept { MoveFrom(MoveTemp(Other)); }
    LiveKitAudioTrack& operator=(LiveKitAudioTrack&& Other) noexcept
    {
        if (this != &Other)
        {
            Reset();
            MoveFrom(MoveTemp(Other));
        }
        return *this;
    }

    bool PublishPCM(const int16_t* Interleaved, size_t FramesPerChannel) const;
    bool PublishPCM(const TArray<int16>& Frames, int32 FramesPerChannel) const;

    bool IsValid() const { return Handle != nullptr; }
    const FString& GetName() const { return Name; }
    int32 GetSampleRate() const { return SampleRate; }
    int32 GetChannels() const { return Channels; }
    int32 GetBufferMs() const { return BufferMs; }

private:
    void Reset();
    void MoveFrom(LiveKitAudioTrack&& Other);
    LkAudioTrackHandle* GetHandle() const { return Handle; }

    LiveKitClient* Client = nullptr;
    LkAudioTrackHandle* Handle = nullptr;
    FString Name;
    int32 SampleRate = 0;
    int32 Channels = 0;
    int32 BufferMs = 0;

    friend class LiveKitClient;
};

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

    bool SendDataOnChannel(const void* Bytes, size_t Len, LkReliability Reliability, bool bOrdered, const FString& Label)
    {
        if (!Handle || Bytes == nullptr || Len == 0)
        {
            return false;
        }
        FTCHARToUTF8 Utf8Label(*Label);
        const char* LabelPtr = Utf8Label.Length() > 0 ? Utf8Label.Get() : nullptr;
        LkResult r = lk_send_data_ex(Handle, static_cast<const uint8_t*>(Bytes), Len, Reliability, bOrdered ? 1 : 0, LabelPtr);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            CaptureError(r);
            if (r.message)
            {
                UE_LOG(LogTemp, Warning, TEXT("LiveKit send data (channel '%s'): %s"), *Label, UTF8_TO_TCHAR(r.message));
                lk_free_str((char*)r.message);
            }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
            ClearError();
        }
        return ok;
    }

    bool SendDataOnChannel(const TArray<uint8>& Payload, LkReliability Reliability, bool bOrdered, const FString& Label)
    {
        return SendDataOnChannel(Payload.GetData(), static_cast<size_t>(Payload.Num()), Reliability, bOrdered, Label);
    }

    TUniquePtr<LiveKitDataChannel> CreateDataChannel(const FString& Label, bool bReliable, bool bOrdered)
    {
        if (Label.IsEmpty() || Handle == nullptr)
        {
            return nullptr;
        }
        return MakeUnique<LiveKitDataChannel>(this, Label, bReliable ? LkReliable : LkLossy, bOrdered);
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

    bool IsReady() const
    {
        return Handle && lk_client_is_ready(Handle) != 0;
    }

    TUniquePtr<LiveKitAudioTrack> CreateAudioTrack(const FString& TrackName, int32 SampleRate, int32 Channels, int32 BufferMs = 1000)
    {
        if (!Handle || SampleRate <= 0 || Channels <= 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("LiveKit create audio track: invalid params (sr=%d, ch=%d)"), SampleRate, Channels);
            return nullptr;
        }
        FTCHARToUTF8 Utf8Name(*TrackName);
        LkAudioTrackConfig Config;
        Config.track_name = Utf8Name.Length() > 0 ? Utf8Name.Get() : nullptr;
        Config.sample_rate = SampleRate;
        Config.channels = Channels;
        Config.buffer_ms = BufferMs;
        LkAudioTrackHandle* TrackHandle = nullptr;
        LkResult r = lk_audio_track_create(Handle, &Config, &TrackHandle);
        const bool ok = (r.code == 0) && TrackHandle != nullptr;
        if (!ok)
        {
            CaptureError(r);
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit create audio track '%s' failed: %s"), *TrackName, UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
            return nullptr;
        }
        if (r.message)
        {
            lk_free_str((char*)r.message);
            ClearError();
        }
        return MakeUnique<LiveKitAudioTrack>(this, TrackHandle, TrackName, SampleRate, Channels, BufferMs);
    }

    bool PublishAudioOnTrack(const LiveKitAudioTrack& Track, const int16_t* Interleaved, size_t FramesPerChannel)
    {
        if (!Handle || !Track.IsValid() || Interleaved == nullptr || FramesPerChannel == 0)
        {
            return false;
        }
        LkResult r = lk_audio_track_publish_pcm_i16(Track.GetHandle(), Interleaved, FramesPerChannel);
        const bool ok = (r.code == 0);
        if (!ok)
        {
            CaptureError(r);
            if (r.message) { UE_LOG(LogTemp, Warning, TEXT("LiveKit publish audio (track '%s') failed: %s"), *Track.GetName(), UTF8_TO_TCHAR(r.message)); lk_free_str((char*)r.message); }
        }
        else if (r.message)
        {
            lk_free_str((char*)r.message);
            ClearError();
        }
        return ok;
    }

    bool PublishAudioOnTrack(const LiveKitAudioTrack& Track, const TArray<int16>& Frames, int32 FramesPerChannel)
    {
        return PublishAudioOnTrack(Track, Frames.GetData(), static_cast<size_t>(FramesPerChannel));
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

inline LiveKitDataChannel::LiveKitDataChannel(LiveKitClient* InClient, const FString& InLabel, LkReliability InReliability, bool bInOrdered)
    : Client(InClient)
    , Label(InLabel)
    , Reliability(InReliability)
    , bOrdered(bInOrdered)
{
}

inline bool LiveKitDataChannel::Send(const void* Bytes, size_t Len) const
{
    return Client && Client->SendDataOnChannel(Bytes, Len, Reliability, bOrdered, Label);
}

inline bool LiveKitDataChannel::Send(const TArray<uint8>& Payload) const
{
    return Send(Payload.GetData(), static_cast<size_t>(Payload.Num()));
}

inline LiveKitAudioTrack::LiveKitAudioTrack(LiveKitClient* InClient, LkAudioTrackHandle* InHandle, const FString& InName, int32 InSampleRate, int32 InChannels, int32 InBufferMs)
    : Client(InClient)
    , Handle(InHandle)
    , Name(InName)
    , SampleRate(InSampleRate)
    , Channels(InChannels)
    , BufferMs(InBufferMs)
{
}

inline bool LiveKitAudioTrack::PublishPCM(const int16_t* Interleaved, size_t FramesPerChannel) const
{
    return Client && Client->PublishAudioOnTrack(*this, Interleaved, FramesPerChannel);
}

inline bool LiveKitAudioTrack::PublishPCM(const TArray<int16>& Frames, int32 FramesPerChannel) const
{
    return PublishPCM(Frames.GetData(), static_cast<size_t>(FramesPerChannel));
}

inline void LiveKitAudioTrack::Reset()
{
    if (Handle)
    {
        LkAudioTrackHandle* ToDestroy = Handle;
        Handle = nullptr;
        LkResult r = lk_audio_track_destroy(ToDestroy);
        if (r.code != 0 && r.message)
        {
            const TCHAR* TrackLabel = Name.IsEmpty() ? TEXT("<unnamed>") : *Name;
            UE_LOG(LogTemp, Warning, TEXT("LiveKit destroy audio track '%s' failed: %s"), TrackLabel, UTF8_TO_TCHAR(r.message));
        }
        if (r.message)
        {
            lk_free_str((char*)r.message);
        }
    }
    Client = nullptr;
    Name.Reset();
    SampleRate = 0;
    Channels = 0;
    BufferMs = 0;
}

inline void LiveKitAudioTrack::MoveFrom(LiveKitAudioTrack&& Other)
{
    Client = Other.Client;
    Handle = Other.Handle;
    Name = MoveTemp(Other.Name);
    SampleRate = Other.SampleRate;
    Channels = Other.Channels;
    BufferMs = Other.BufferMs;
    Other.Client = nullptr;
    Other.Handle = nullptr;
    Other.SampleRate = 0;
    Other.Channels = 0;
    Other.BufferMs = 0;
}
