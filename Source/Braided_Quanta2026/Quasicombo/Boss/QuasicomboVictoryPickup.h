#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboVictoryPickup.generated.h"

class AQuasicomboBoss;
class ASideScrollingCharacter;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

/** The boss is defeated when the QTE ends; the run is won only when this is collected. */
UCLASS()
class AQuasicomboVictoryPickup : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboVictoryPickup();
	void Initialize(AQuasicomboBoss* InBoss, ASideScrollingCharacter* InPlayer);
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Orb;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
	TWeakObjectPtr<AQuasicomboBoss> Boss;
	TWeakObjectPtr<ASideScrollingCharacter> Player;
	float ArmAt = 0.0f;
	float Age = 0.0f;
	bool bCollected = false;
	void TryCollect(AActor* OtherActor);
};
