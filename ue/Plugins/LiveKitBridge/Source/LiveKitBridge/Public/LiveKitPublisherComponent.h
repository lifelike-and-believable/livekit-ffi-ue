#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LiveKitPublisherComponent.generated.h"

UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class ULiveKitPublisherComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="LiveKit") FString RoomUrl;
    UPROPERTY(EditAnywhere, Category="LiveKit") FString Token;
    UPROPERTY(EditAnywhere, Category="LiveKit|Audio") int32 SampleRate = 48000;
    UPROPERTY(EditAnywhere, Category="LiveKit|Audio") int32 Channels = 1;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UFUNCTION(BlueprintCallable, Category="LiveKit")
    void PushAudioPCM(const TArray<int16>& InterleavedFrames, int32 FramesPerChannel);

    UFUNCTION(BlueprintCallable, Category="LiveKit")
    void SendMocap(const TArray<uint8>& Payload, bool bReliable);

private:
    class LiveKitClient* Client = nullptr;
};
