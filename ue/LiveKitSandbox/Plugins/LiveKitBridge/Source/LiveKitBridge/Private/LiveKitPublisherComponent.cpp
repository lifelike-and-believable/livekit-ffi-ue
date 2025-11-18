#include "LiveKitPublisherComponent.h"
#include "LiveKitClient.hpp"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Async/Async.h"
#include <cmath>
DEFINE_LOG_CATEGORY_STATIC(LogLiveKitBridge, Log, All);

void ULiveKitPublisherComponent::BeginPlay()
{
    Super::BeginPlay();
    Client = new LiveKitClient();
    // Map role
    LkRole LkRoleVal = LkRoleBoth;
    switch (Role)
    {
        case ELiveKitClientRole::Publisher:   LkRoleVal = LkRolePublisher; break;
        case ELiveKitClientRole::Subscriber:  LkRoleVal = LkRoleSubscriber; break;
        case ELiveKitClientRole::Auto:        LkRoleVal = LkRoleAuto; break;
        case ELiveKitClientRole::Both:        LkRoleVal = LkRoleBoth; break;
        default:                               LkRoleVal = LkRoleBoth; break;
    }

    if (bReceiveMocap)
    {
        Client->SetDataCallback(&ULiveKitPublisherComponent::DataThunk, this);
    }
    if (bReceiveAudio)
    {
        Client->SetAudioCallback(&ULiveKitPublisherComponent::AudioThunk, this);
    }
    // Always bind connection state callback for async lifecycle
    Client->SetConnectionCallback(&ULiveKitPublisherComponent::ConnectionThunk, this);

    if (bConnectAsync)
    {
        const bool scheduled = Client->ConnectAsyncWithRole(TCHAR_TO_UTF8(*RoomUrl), TCHAR_TO_UTF8(*Token), LkRoleVal);
        if (!scheduled)
        {
            const FString Reason = Client ? Client->GetLastErrorMessage() : FString();
            UE_LOG(LogLiveKitBridge, Error, TEXT("Failed to schedule async connect: %s"), Reason.IsEmpty()?TEXT("unknown"):*Reason);
        }
        // Optional timeout supervision (does not cancel connect, only logs/pulses feedback)
        if (GetWorld() && ConnectTimeoutSec > 0.f)
        {
            GetWorld()->GetTimerManager().SetTimer(ConnectTimeoutHandle, [this]()
            {
                if (!IsValid(this) || !Client) return;
                if (!Client->IsReady())
                {
                    UE_LOG(LogLiveKitBridge, Warning, TEXT("LiveKit connect not ready after %.1fs (server down?)"), ConnectTimeoutSec);
                }
            }, ConnectTimeoutSec, false);
        }
    }
    else
    {
        const bool bOk = Client->ConnectWithRole(TCHAR_TO_UTF8(*RoomUrl), TCHAR_TO_UTF8(*Token), LkRoleVal);
        if (!bOk)
        {
            const FString Reason = Client ? Client->GetLastErrorMessage() : FString();
            if (!Reason.IsEmpty())
            {
                UE_LOG(LogLiveKitBridge, Error, TEXT("LiveKit connect failed for %s: %s"), *RoomUrl, *Reason);
            }
            else
            {
                UE_LOG(LogLiveKitBridge, Error, TEXT("LiveKit connect failed for %s"), *RoomUrl);
            }
        } else
        {
            const TCHAR* RoleStr = TEXT("Both");
            switch (Role)
            {
                case ELiveKitClientRole::Publisher: RoleStr = TEXT("Publisher"); break;
                case ELiveKitClientRole::Subscriber: RoleStr = TEXT("Subscriber"); break;
                case ELiveKitClientRole::Auto: RoleStr = TEXT("Auto"); break;
                case ELiveKitClientRole::Both: default: RoleStr = TEXT("Both"); break;
            }
            UE_LOG(LogLiveKitBridge, Log, TEXT("LiveKit connected to %s (Role=%s, Recv: mocap=%s audio=%s)"), *RoomUrl, RoleStr, bReceiveMocap?TEXT("on"):TEXT("off"), bReceiveAudio?TEXT("on"):TEXT("off"));
            AsyncTask(ENamedThreads::GameThread, [this]()
            {
                if (IsValid(this)) { OnConnected(RoomUrl, Role, bReceiveMocap, bReceiveAudio); }
            });
        }
    }

    if (bStartDebugTone)
    {
        StartDebugTone();
    }
    if (bStartTestData)
    {
        StartTestData();
    }
}

void ULiveKitPublisherComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    DataChannels.Empty();
    AudioTracks.Empty();
    if (Client) { Client->Disconnect(); delete Client; Client = nullptr; }
    StopDebugTone();
    StopTestData();
    AsyncTask(ENamedThreads::GameThread, [this]()
    {
        if (IsValid(this)) { OnDisconnected(); }
    });
    Super::EndPlay(Reason);
}

void ULiveKitPublisherComponent::PushAudioPCM(const TArray<int16>& InterleavedFrames, int32 FramesPerChannel)
{
    if (Client && InterleavedFrames.Num() > 0)
    {
        const bool bOk = Client->PublishPCM(InterleavedFrames.GetData(), (size_t)FramesPerChannel, Channels, SampleRate);
        if (!bOk)
        {
            const FString Reason = Client ? Client->GetLastErrorMessage() : FString();
            if (!Reason.IsEmpty())
            {
                UE_LOG(LogLiveKitBridge, Verbose, TEXT("PublishPCM failed (%d frames/ch): %s"), FramesPerChannel, *Reason);
            }
            else
            {
                UE_LOG(LogLiveKitBridge, Verbose, TEXT("PublishPCM failed (%d frames/ch)"), FramesPerChannel);
            }
        } else {
            if (!bLoggedAudioInit)
            {
                bLoggedAudioInit = true;
                UE_LOG(LogLiveKitBridge, Log, TEXT("Audio publish pipeline active (first frame pushed: %d fpc, sr=%d ch=%d)"), FramesPerChannel, SampleRate, Channels);
                AsyncTask(ENamedThreads::GameThread, [this]()
                {
                    if (IsValid(this)) { OnAudioPublishReady(SampleRate, Channels); }
                });
            }
            else
            {
                UE_LOG(LogLiveKitBridge, VeryVerbose, TEXT("PublishPCM succeeded (%d frames/ch)"), FramesPerChannel);
            }
        }
    }
}

bool ULiveKitPublisherComponent::CreateAudioTrack(FName TrackName, int32 TrackSampleRate, int32 TrackChannels, int32 BufferMs)
{
    if (!Client)
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("CreateAudioTrack called before LiveKit client is ready"));
        return false;
    }
    if (TrackName.IsNone() || TrackSampleRate <= 0 || TrackChannels <= 0)
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("CreateAudioTrack invalid parameters (name=%s sr=%d ch=%d)"), *TrackName.ToString(), TrackSampleRate, TrackChannels);
        return false;
    }
    if (AudioTracks.Contains(TrackName))
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("CreateAudioTrack skipped: '%s' already exists"), *TrackName.ToString());
        return false;
    }
    TUniquePtr<LiveKitAudioTrack> Track = Client->CreateAudioTrack(TrackName.ToString(), TrackSampleRate, TrackChannels, BufferMs);
    if (!Track.IsValid() || !Track->IsValid())
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("CreateAudioTrack failed for '%s'"), *TrackName.ToString());
        return false;
    }
    AudioTracks.Add(TrackName, MoveTemp(Track));
    UE_LOG(LogLiveKitBridge, Log, TEXT("Created LiveKit audio track '%s' (sr=%d, ch=%d, buffer=%dms)"), *TrackName.ToString(), TrackSampleRate, TrackChannels, BufferMs);
    return true;
}

bool ULiveKitPublisherComponent::DestroyAudioTrack(FName TrackName)
{
    if (AudioTracks.Remove(TrackName) > 0)
    {
        UE_LOG(LogLiveKitBridge, Log, TEXT("Destroyed LiveKit audio track '%s'"), *TrackName.ToString());
        return true;
    }
    UE_LOG(LogLiveKitBridge, Warning, TEXT("DestroyAudioTrack: track '%s' not found"), *TrackName.ToString());
    return false;
}

void ULiveKitPublisherComponent::PushAudioPCMOnTrack(FName TrackName, const TArray<int16>& InterleavedFrames, int32 FramesPerChannel)
{
    if (InterleavedFrames.Num() == 0)
    {
        return;
    }
    TUniquePtr<LiveKitAudioTrack>* TrackPtr = AudioTracks.Find(TrackName);
    if (!TrackPtr || !TrackPtr->IsValid() || !(*TrackPtr)->IsValid())
    {
        UE_LOG(LogLiveKitBridge, Verbose, TEXT("PushAudioPCMOnTrack: track '%s' not available"), *TrackName.ToString());
        return;
    }
    if (!(*TrackPtr)->PublishPCM(InterleavedFrames.GetData(), (size_t)FramesPerChannel))
    {
        const FString Reason = Client ? Client->GetLastErrorMessage() : FString();
        UE_LOG(LogLiveKitBridge, Verbose, TEXT("PushAudioPCMOnTrack '%s' failed (%s)"), *TrackName.ToString(), Reason.IsEmpty()?TEXT("no reason"): *Reason);
    }
}

void ULiveKitPublisherComponent::SendMocap(const TArray<uint8>& Payload, bool bReliable)
{
    if (Client && Payload.Num() > 0)
    {
        const bool bOk = Client->SendData(Payload.GetData(), (size_t)Payload.Num(), bReliable);
        if (!bOk)
        {
            const FString Reason = Client ? Client->GetLastErrorMessage() : FString();
            if (!Reason.IsEmpty())
            {
                UE_LOG(LogLiveKitBridge, Verbose, TEXT("SendMocap failed (%d bytes, reliable=%s): %s"), Payload.Num(), bReliable ? TEXT("true") : TEXT("false"), *Reason);
                AsyncTask(ENamedThreads::GameThread, [this, n=Payload.Num(), b=bReliable, Reason]()
                {
                    if (IsValid(this)) { OnMocapSendFailed(n, b, Reason); }
                });
            }
            else
            {
                UE_LOG(LogLiveKitBridge, Verbose, TEXT("SendMocap failed (%d bytes, reliable=%s)"), Payload.Num(), bReliable ? TEXT("true") : TEXT("false"));
                const FString Fallback = TEXT("unknown");
                AsyncTask(ENamedThreads::GameThread, [this, n=Payload.Num(), b=bReliable, Fallback]()
                {
                    if (IsValid(this)) { OnMocapSendFailed(n, b, Fallback); }
                });
            }
        } else {
            UE_LOG(LogLiveKitBridge, Log, TEXT("SendMocap succeeded (%d bytes, reliable=%s)"), Payload.Num(), bReliable ? TEXT("true") : TEXT("false"));
            AsyncTask(ENamedThreads::GameThread, [this, n=Payload.Num(), b=bReliable]()
            {
                if (IsValid(this)) { OnMocapSent(n, b); }
            });
        }
    }
}

bool ULiveKitPublisherComponent::RegisterMocapChannel(FName ChannelName, const FString& Label, bool bReliable, bool bOrdered)
{
    if (!Client)
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("RegisterMocapChannel called before LiveKit client is ready"));
        return false;
    }
    if (ChannelName.IsNone() || Label.IsEmpty())
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("RegisterMocapChannel invalid args (channel=%s label empty=%s)"), *ChannelName.ToString(), Label.IsEmpty()?TEXT("true"):TEXT("false"));
        return false;
    }
    if (DataChannels.Contains(ChannelName))
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("RegisterMocapChannel skipped: '%s' already exists"), *ChannelName.ToString());
        return false;
    }
    TUniquePtr<LiveKitDataChannel> Channel = Client->CreateDataChannel(Label, bReliable, bOrdered);
    if (!Channel.IsValid() || !Channel->IsValid())
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("RegisterMocapChannel failed to create '%s'"), *ChannelName.ToString());
        return false;
    }
    DataChannels.Add(ChannelName, MoveTemp(Channel));
    UE_LOG(LogLiveKitBridge, Log, TEXT("Registered mocap channel '%s' (label='%s', reliable=%s, ordered=%s)"),
        *ChannelName.ToString(), *Label, bReliable?TEXT("true"):TEXT("false"), bOrdered?TEXT("true"):TEXT("false"));
    return true;
}

bool ULiveKitPublisherComponent::UnregisterMocapChannel(FName ChannelName)
{
    if (DataChannels.Remove(ChannelName) > 0)
    {
        UE_LOG(LogLiveKitBridge, Log, TEXT("Unregistered mocap channel '%s'"), *ChannelName.ToString());
        return true;
    }
    UE_LOG(LogLiveKitBridge, Warning, TEXT("UnregisterMocapChannel: channel '%s' not found"), *ChannelName.ToString());
    return false;
}

bool ULiveKitPublisherComponent::SendMocapOnChannel(FName ChannelName, const TArray<uint8>& Payload)
{
    if (!Client || Payload.Num() == 0)
    {
        return false;
    }
    TUniquePtr<LiveKitDataChannel>* ChannelPtr = DataChannels.Find(ChannelName);
    if (!ChannelPtr || !ChannelPtr->IsValid() || !(*ChannelPtr)->IsValid())
    {
        UE_LOG(LogLiveKitBridge, Warning, TEXT("SendMocapOnChannel: channel '%s' unavailable"), *ChannelName.ToString());
        return false;
    }
    const bool bReliable = (*ChannelPtr)->IsReliable();
    const bool bOk = (*ChannelPtr)->Send(Payload);
    if (!bOk)
    {
        const FString Reason = Client ? Client->GetLastErrorMessage() : FString();
        if (!Reason.IsEmpty())
        {
            UE_LOG(LogLiveKitBridge, Verbose, TEXT("SendMocapOnChannel '%s' failed (%d bytes, reliable=%s): %s"), *ChannelName.ToString(), Payload.Num(), bReliable?TEXT("true"):TEXT("false"), *Reason);
            AsyncTask(ENamedThreads::GameThread, [this, n = Payload.Num(), b = bReliable, Reason]()
            {
                if (IsValid(this)) { OnMocapSendFailed(n, b, Reason); }
            });
        }
        else
        {
            UE_LOG(LogLiveKitBridge, Verbose, TEXT("SendMocapOnChannel '%s' failed (%d bytes, reliable=%s)"), *ChannelName.ToString(), Payload.Num(), bReliable?TEXT("true"):TEXT("false"));
            AsyncTask(ENamedThreads::GameThread, [this, n = Payload.Num(), b = bReliable]()
            {
                if (IsValid(this)) { OnMocapSendFailed(n, b, TEXT("Send failed")); }
            });
        }
    }
    else
    {
        UE_LOG(LogLiveKitBridge, Log, TEXT("SendMocapOnChannel '%s' succeeded (%d bytes, reliable=%s)"), *ChannelName.ToString(), Payload.Num(), bReliable?TEXT("true"):TEXT("false"));
        AsyncTask(ENamedThreads::GameThread, [this, n = Payload.Num(), b = bReliable]()
        {
            if (IsValid(this)) { OnMocapSent(n, b); }
        });
    }
    return bOk;
}

/* static */ void ULiveKitPublisherComponent::DataThunk(void* User, const uint8_t* bytes, size_t len)
{
    if (!User || !bytes || len == 0) return;
    ULiveKitPublisherComponent* Self = reinterpret_cast<ULiveKitPublisherComponent*>(User);
    if (!IsValid(Self)) return;
    // Optional debug decode: [u64 time_us][u64 seq]
    if (len >= 16)
    {
        uint64 TimeUs = 0; uint64 Seq = 0;
        FMemory::Memcpy(&TimeUs, bytes + 0, sizeof(uint64));
        FMemory::Memcpy(&Seq, bytes + 8, sizeof(uint64));
        const double NowUs = FPlatformTime::Seconds() * 1e6;
        const double LatencyMs = (NowUs - (double)TimeUs) / 1000.0;
        UE_LOG(LogLiveKitBridge, Log, TEXT("Mocap recv: seq=%llu size=%d latency=%.2fms"), (unsigned long long)Seq, (int)len, LatencyMs);
    }
    else
    {
        UE_LOG(LogLiveKitBridge, Log, TEXT("Mocap recv: size=%d"), (int)len);
    }
    TArray<uint8> Payload;
    Payload.Append(bytes, (int32)len);
    AsyncTask(ENamedThreads::GameThread, [Self, Payload = MoveTemp(Payload)]() mutable {
        if (IsValid(Self))
        {
            Self->OnMocapReceived(Payload);
        }
    });
}

/* static */ void ULiveKitPublisherComponent::AudioThunk(void* User, const int16_t* pcm, size_t frames_per_channel, int32_t channels, int32_t sample_rate)
{
    if (!User || !pcm || frames_per_channel == 0 || channels <= 0 || sample_rate <= 0) return;
    ULiveKitPublisherComponent* Self = reinterpret_cast<ULiveKitPublisherComponent*>(User);
    if (!IsValid(Self)) return;

    // Log first frame and then every ~100 frames to avoid spam
    Self->AudioFrameCount++;
    if (!Self->bLoggedFirstAudioFrame)
    {
        Self->bLoggedFirstAudioFrame = true;
        UE_LOG(LogLiveKitBridge, Log, TEXT("Remote audio frame: sr=%d ch=%d fpc=%d"), sample_rate, channels, (int32)frames_per_channel);
        AsyncTask(ENamedThreads::GameThread, [Self, sample_rate, channels, frames_per_channel]()
        {
            if (IsValid(Self)) { Self->OnFirstAudioReceived(sample_rate, channels, (int32)frames_per_channel); }
        });
    }
    else if ((Self->AudioFrameCount % 100) == 0)
    {
        UE_LOG(LogLiveKitBridge, VeryVerbose, TEXT("Remote audio frame #%lld: sr=%d ch=%d fpc=%d"), (long long)Self->AudioFrameCount, sample_rate, channels, (int32)frames_per_channel);
    }
}

void ULiveKitPublisherComponent::StartDebugTone()
{
    if (!GetWorld()) return;
    if (!Client || !Client->IsReady())
    {
        // Defer until the client signals readiness
        UE_LOG(LogLiveKitBridge, VeryVerbose, TEXT("Deferring debug tone: client not ready yet"));
        GetWorld()->GetTimerManager().SetTimer(ToneReadyHandle, [this]() { StartDebugTone(); }, 0.25f, false);
        return;
    }
    const float TickSec = 0.01f; // 10ms
    UE_LOG(LogLiveKitBridge, Log, TEXT("Starting debug tone: %.1f Hz amp=%.2f (sr=%d ch=%d)"), ToneFrequencyHz, ToneAmplitude, SampleRate, Channels);
    GetWorld()->GetTimerManager().SetTimer(ToneTimerHandle, this, &ULiveKitPublisherComponent::StopDebugTone, 0.0f, false); // ensure handle exists
    const float InitialDelay = 0.5f; // give room/data channels time to come up
    GetWorld()->GetTimerManager().SetTimer(ToneTimerHandle, [this]()
    {
        if (!Client) return;
        const int32 FramesPerChannel = FMath::Max(1, SampleRate / 100);
        const int32 TotalSamples = FramesPerChannel * Channels;
        TArray<int16> Buffer;
        Buffer.SetNumUninitialized(TotalSamples);

        const double TwoPi = 6.283185307179586;
        const double PhaseInc = TwoPi * ToneFrequencyHz / double(SampleRate);
        const double Amp = FMath::Clamp(ToneAmplitude, 0.0f, 1.0f) * 32767.0;

        for (int32 i = 0; i < FramesPerChannel; ++i)
        {
            const int16 s = (int16)FMath::Clamp<int32>((int32)std::lrint(std::sin(TonePhase) * Amp), -32767, 32767);
            for (int32 ch = 0; ch < Channels; ++ch)
            {
                Buffer[i * Channels + ch] = s;
            }
            TonePhase += PhaseInc;
            if (TonePhase > TwoPi) TonePhase -= TwoPi;
        }
        PushAudioPCM(Buffer, FramesPerChannel);
    }, TickSec, true, InitialDelay);
}

void ULiveKitPublisherComponent::StopDebugTone()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ToneTimerHandle);
        UE_LOG(LogLiveKitBridge, Log, TEXT("Stopped debug tone"));
    }
}

void ULiveKitPublisherComponent::StartTestData()
{
    if (!GetWorld()) return;
    if (!Client || !Client->IsReady())
    {
        // Defer until the client signals readiness
        UE_LOG(LogLiveKitBridge, VeryVerbose, TEXT("Deferring test data: client not ready yet"));
        GetWorld()->GetTimerManager().SetTimer(DataReadyHandle, [this]() { StartTestData(); }, 0.25f, false);
        return;
    }
    const float Period = (TestDataRateHz > 0.f) ? (1.0f / TestDataRateHz) : 0.5f;
    UE_LOG(LogLiveKitBridge, Log, TEXT("Starting test data: rate=%.2f Hz bytes=%d reliable=%s"), TestDataRateHz, TestDataPayloadBytes, bTestDataReliable?TEXT("true"):TEXT("false"));
    GetWorld()->GetTimerManager().SetTimer(DataTimerHandle, [this]()
    {
        if (!Client) return;
        const int32 N = FMath::Max(1, TestDataPayloadBytes);
        TArray<uint8> Payload;
        Payload.SetNumUninitialized(N);

        // Simple structure: [u64 time_us][u64 seq][padding pattern]
        const uint64 NowUs = (uint64)(FPlatformTime::Seconds() * 1e6);
        if (N >= 16)
        {
            FMemory::Memcpy(Payload.GetData() + 0, &NowUs, sizeof(uint64));
            FMemory::Memcpy(Payload.GetData() + 8, &DataSeq, sizeof(uint64));
            for (int32 i = 16; i < N; ++i) { Payload[i] = (uint8)(i & 0xFF); }
        }
        else
        {
            for (int32 i = 0; i < N; ++i) { Payload[i] = (uint8)(i ^ 0x5A); }
        }
        ++DataSeq;
        UE_LOG(LogLiveKitBridge, Log, TEXT("SendMocap tick: seq=%llu size=%d reliable=%s"), (unsigned long long)(DataSeq-1), N, bTestDataReliable?TEXT("true"):TEXT("false"));
        SendMocap(Payload, bTestDataReliable);
    }, Period, true, 0.5f);
}

void ULiveKitPublisherComponent::StopTestData()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DataTimerHandle);
        UE_LOG(LogLiveKitBridge, Log, TEXT("Stopped test data"));
    }
}

/* static */ void ULiveKitPublisherComponent::ConnectionThunk(void* User, LkConnectionState state, int32_t reason_code, const char* message)
{
    ULiveKitPublisherComponent* Self = reinterpret_cast<ULiveKitPublisherComponent*>(User);
    if (!Self || !IsValid(Self)) return;
    const FString Msg = message ? FString(UTF8_TO_TCHAR(message)) : FString();
    AsyncTask(ENamedThreads::GameThread, [Self, state, reason_code, Msg]()
    {
        if (!IsValid(Self)) return;
        switch (state)
        {
            case LkConnConnecting:
                UE_LOG(LogLiveKitBridge, Log, TEXT("LiveKit: connecting..."));
                break;
            case LkConnConnected:
            {
                const TCHAR* RoleStr = TEXT("Both");
                switch (Self->Role)
                {
                    case ELiveKitClientRole::Publisher: RoleStr = TEXT("Publisher"); break;
                    case ELiveKitClientRole::Subscriber: RoleStr = TEXT("Subscriber"); break;
                    case ELiveKitClientRole::Auto: RoleStr = TEXT("Auto"); break;
                    case ELiveKitClientRole::Both: default: RoleStr = TEXT("Both"); break;
                }
                UE_LOG(LogLiveKitBridge, Log, TEXT("LiveKit connected to %s (Role=%s, Recv: mocap=%s audio=%s)"), *Self->RoomUrl, RoleStr, Self->bReceiveMocap?TEXT("on"):TEXT("off"), Self->bReceiveAudio?TEXT("on"):TEXT("off"));
                Self->OnConnected(Self->RoomUrl, Self->Role, Self->bReceiveMocap, Self->bReceiveAudio);
                break;
            }
            case LkConnReconnecting:
                UE_LOG(LogLiveKitBridge, Warning, TEXT("LiveKit: reconnecting..."));
                break;
            case LkConnDisconnected:
                UE_LOG(LogLiveKitBridge, Warning, TEXT("LiveKit: disconnected (%d) %s"), reason_code, *Msg);
                Self->OnDisconnected();
                break;
            case LkConnFailed:
                UE_LOG(LogLiveKitBridge, Error, TEXT("LiveKit: connection failed (%d) %s"), reason_code, *Msg);
                break;
        }
    });
}
