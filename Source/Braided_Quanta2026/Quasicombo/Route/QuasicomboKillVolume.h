#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboKillVolume.generated.h"

class UBoxComponent;

UCLASS()
class AQuasicomboKillVolume : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboKillVolume();
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UBoxComponent> Trigger;
	virtual void BeginPlay() override;
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
