#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboBarrier.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Solid route gate with an optional falling visual. Place behind or beyond an entry trigger. */
UCLASS()
class AQuasicomboBarrier : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboBarrier();
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Route") void SetClosed(bool bClosed);
	UFUNCTION(BlueprintPure, Category="Quasicombo|Route") bool IsClosed() const { return bIsClosed; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Route") bool bInitiallyClosed = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Route") float FallHeight = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Route") float FallDuration = 0.35f;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UBoxComponent> Blocker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UStaticMeshComponent> Visual;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	bool bIsClosed = false;
	bool bConfigured = false;
	float FallElapsed = 0.0f;
	FVector LandingLocation = FVector::ZeroVector;
};
