#include "LiveKitPublisherComponent.h"
#include "LiveKitClient.hpp"

void ULiveKitPublisherComponent::BeginPlay()
{
    Super::BeginPlay();
    Client = new LiveKitClient();
    Client->Connect(TCHAR_TO_ANSI(*RoomUrl), TCHAR_TO_ANSI(*Token));
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
        Client->PublishPCM(InterleavedFrames.GetData(), (size_t)FramesPerChannel, Channels, SampleRate);
    }
}

void ULiveKitPublisherComponent::SendMocap(const TArray<uint8>& Payload, bool bReliable)
{
    if (Client && Payload.Num() > 0)
    {
        Client->SendData(Payload.GetData(), (size_t)Payload.Num(), bReliable);
    }
}
