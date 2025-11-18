#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Components/ActorComponent.h"
#include "livekit_ffi.h"
#include "LiveKitClient.hpp"

UENUM(BlueprintType)
enum class ELiveKitClientRole : uint8
{
    Auto       UMETA(DisplayName="Auto"),
    Publisher  UMETA(DisplayName="Publisher"),
    Subscriber UMETA(DisplayName="Subscriber"),
    Both       UMETA(DisplayName="Both")
};
#include "LiveKitPublisherComponent.generated.h"

UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class ULiveKitPublisherComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="LiveKit") FString RoomUrl;
    UPROPERTY(EditAnywhere, Category="LiveKit") FString Token;
    UPROPERTY(EditAnywhere, Category="LiveKit") ELiveKitClientRole Role = ELiveKitClientRole::Both;
    UPROPERTY(EditAnywhere, Category="LiveKit") bool bReceiveMocap = true;
    UPROPERTY(EditAnywhere, Category="LiveKit") bool bReceiveAudio = false; // not exposed to BP (audio frames are native only)
    UPROPERTY(EditAnywhere, Category="LiveKit|Audio") int32 SampleRate = 48000;
    UPROPERTY(EditAnywhere, Category="LiveKit|Audio") int32 Channels = 1;

    // Test utilities
    UPROPERTY(EditAnywhere, Category="LiveKit|Test") bool bStartDebugTone = false;
    UPROPERTY(EditAnywhere, Category="LiveKit|Test") float ToneFrequencyHz = 440.0f;
    UPROPERTY(EditAnywhere, Category="LiveKit|Test") float ToneAmplitude = 0.2f; // 0..1

    UPROPERTY(EditAnywhere, Category="LiveKit|Test") bool bStartTestData = false;
    UPROPERTY(EditAnywhere, Category="LiveKit|Test") float TestDataRateHz = 2.0f; // sends per second
    UPROPERTY(EditAnywhere, Category="LiveKit|Test") int32 TestDataPayloadBytes = 64;
    UPROPERTY(EditAnywhere, Category="LiveKit|Test") bool bTestDataReliable = true;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // Native-only entry (int16 is not a Blueprint-supported element type)
    void PushAudioPCM(const TArray<int16>& InterleavedFrames, int32 FramesPerChannel);

    UFUNCTION(BlueprintCallable, Category="LiveKit")
    void SendMocap(const TArray<uint8>& Payload, bool bReliable);
    UFUNCTION(BlueprintCallable, Category="LiveKit|Data")
    bool RegisterMocapChannel(FName ChannelName, const FString& Label, bool bReliable, bool bOrdered = true);
    UFUNCTION(BlueprintCallable, Category="LiveKit|Data")
    bool UnregisterMocapChannel(FName ChannelName);
    UFUNCTION(BlueprintCallable, Category="LiveKit|Data")
    bool SendMocapOnChannel(FName ChannelName, const TArray<uint8>& Payload);

    // Test controls
    UFUNCTION(BlueprintCallable, Category="LiveKit|Test") void StartDebugTone();
    UFUNCTION(BlueprintCallable, Category="LiveKit|Test") void StopDebugTone();
    UFUNCTION(BlueprintCallable, Category="LiveKit|Test") void StartTestData();
    UFUNCTION(BlueprintCallable, Category="LiveKit|Test") void StopTestData();

    UFUNCTION(BlueprintImplementableEvent, Category="LiveKit")
    void OnMocapReceived(const TArray<uint8>& Payload);

    // Blueprint-friendly feedback events
    UFUNCTION(BlueprintImplementableEvent, Category="LiveKit")
    void OnConnected(const FString& InUrl, ELiveKitClientRole InRole, bool bRecvMocapFlag, bool bRecvAudioFlag);

    UFUNCTION(BlueprintImplementableEvent, Category="LiveKit")
    void OnDisconnected();

    UFUNCTION(BlueprintImplementableEvent, Category="LiveKit|Audio")
    void OnAudioPublishReady(int32 InSampleRate, int32 InChannels);

    UFUNCTION(BlueprintImplementableEvent, Category="LiveKit|Audio")
    void OnFirstAudioReceived(int32 InSampleRate, int32 InChannels, int32 InFramesPerChannel);

    UFUNCTION(BlueprintImplementableEvent, Category="LiveKit|Data")
    void OnMocapSent(int32 Bytes, bool bReliable);

    UFUNCTION(BlueprintImplementableEvent, Category="LiveKit|Data")
    void OnMocapSendFailed(int32 Bytes, bool bReliable, const FString& Reason);

    UFUNCTION(BlueprintCallable, Category="LiveKit|Audio")
    bool CreateAudioTrack(FName TrackName, int32 TrackSampleRate, int32 TrackChannels, int32 BufferMs = 1000);
    UFUNCTION(BlueprintCallable, Category="LiveKit|Audio")
    bool DestroyAudioTrack(FName TrackName);
    // Native-only helper for routing PCM to a named track
    void PushAudioPCMOnTrack(FName TrackName, const TArray<int16>& InterleavedFrames, int32 FramesPerChannel);

private:
    class LiveKitClient* Client = nullptr;
    TMap<FName, TUniquePtr<LiveKitDataChannel>> DataChannels;
    TMap<FName, TUniquePtr<LiveKitAudioTrack>> AudioTracks;

    // C callback thunks
    static void DataThunk(void* User, const uint8_t* bytes, size_t len);
    static void AudioThunk(void* User, const int16_t* pcm, size_t frames_per_channel, int32_t channels, int32_t sample_rate);

    // Test state
    FTimerHandle ToneTimerHandle;
    FTimerHandle DataTimerHandle;
    FTimerHandle ToneReadyHandle;
    FTimerHandle DataReadyHandle;
    double TonePhase = 0.0;
    int64 DataSeq = 0;

    // Audio receive diagnostics
    bool bLoggedFirstAudioFrame = false;
    int64 AudioFrameCount = 0;

    // Publisher-side: log once when audio pipeline first succeeds
    bool bLoggedAudioInit = false;
};
