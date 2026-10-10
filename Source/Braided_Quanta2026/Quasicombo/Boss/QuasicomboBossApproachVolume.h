#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboBossApproachVolume.generated.h"

class AQuasicomboBoss;
class UBoxComponent;
class APawn;

UCLASS(Blueprintable)
class AQuasicomboBossApproachVolume : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboBossApproachVolume();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Approach") TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approach", meta=(MakeEditWidget=true)) FVector StartPoint = FVector(-1500, 0, -850);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approach", meta=(MakeEditWidget=true)) FVector EndPoint = FVector(1500, 0, 850);
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Approach") TObjectPtr<AQuasicomboBoss> Boss;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approach", meta=(ClampMin="0.0")) float StartIntensity = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Approach", meta=(ClampMin="0.0")) float EndIntensity = 4.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Approach") float Progress = 0.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Approach") bool bPlayerInside = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Approach") bool bEncounterOwnsEffects = false;
	static bool ProjectProgress(const FVector& Position, const FVector& Start, const FVector& End, float& RawProgress);
	bool ApplyPlayerLocation(const FVector& Position);
	void FinishOverlap(const FVector& Position);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	TWeakObjectPtr<APawn> TrackedPlayer;
	bool bAppliedApproach = false;
	bool bCheckedInitialOverlap = false;
	bool bLoggedInvalidPoints = false;
	UFUNCTION() void HandleBeginOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent,
		int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
	bool CanControlEffects();
	void ApplyProgress(float NewProgress);
};
