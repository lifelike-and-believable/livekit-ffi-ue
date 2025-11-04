#include "LiveKitPublisherComponent.h"
#include "LiveKitClient.hpp"
DEFINE_LOG_CATEGORY_STATIC(LogLiveKitBridge, Log, All);

void ULiveKitPublisherComponent::BeginPlay()
{
    Super::BeginPlay();
    Client = new LiveKitClient();
    const bool bOk = Client->Connect(TCHAR_TO_UTF8(*RoomUrl), TCHAR_TO_UTF8(*Token));
    if (!bOk)
    {
        UE_LOG(LogLiveKitBridge, Error, TEXT("LiveKit connect failed for %s"), *RoomUrl);
    }
}

void ULiveKitPublisherComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Client) { Client->Disconnect(); delete Client; Client = nullptr; }
    Super::EndPlay(Reason);
}

void ULiveKitPublisherComponent::PushAudioPCM(const TArray<int16>& InterleavedFrames, int32 FramesPerChannel)
{
    if (Client && InterleavedFrames.Num() > 0)
    {
        const bool bOk = Client->PublishPCM(InterleavedFrames.GetData(), (size_t)FramesPerChannel, Channels, SampleRate);
        if (!bOk)
        {
            UE_LOG(LogLiveKitBridge, Verbose, TEXT("PublishPCM failed (%d frames/ch)"), FramesPerChannel);
        }
    }
}

void ULiveKitPublisherComponent::SendMocap(const TArray<uint8>& Payload, bool bReliable)
{
    if (Client && Payload.Num() > 0)
    {
        const bool bOk = Client->SendData(Payload.GetData(), (size_t)Payload.Num(), bReliable);
        if (!bOk)
        {
            UE_LOG(LogLiveKitBridge, Verbose, TEXT("SendMocap failed (%d bytes, reliable=%s)"), Payload.Num(), bReliable ? TEXT("true") : TEXT("false"));
        }
    }
}
