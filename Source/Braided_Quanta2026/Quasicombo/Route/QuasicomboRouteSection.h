#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboRouteSection.generated.h"

class AQuasicomboRouteLeg;
class AStaticMeshActor;
class UBoxComponent;

UCLASS()
class AQuasicomboRouteSection : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboRouteSection();
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route", meta=(ClampMin="1", ClampMax="3")) int32 SectionNumber = 1;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route") TArray<TObjectPtr<AQuasicomboRouteLeg>> Legs;
	/** One authored gate spanning all three route entrances. Its placed transform is the landing position. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route") TObjectPtr<AStaticMeshActor> RearGate;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route|Rear Gate", meta=(ClampMin="0")) float RearGateFallHeight = 250.0f;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route|Rear Gate", meta=(ClampMin="0.01")) float RearGateFallDuration = 0.35f;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route|Rear Gate", meta=(ClampMin="0")) float RearGateClearance = 20.0f;
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Route") bool CommitLane(EQuasicomboLane Lane);
	UFUNCTION(BlueprintPure, Category="Quasicombo|Route") bool IsRearGateClosed() const { return bRearGateClosed; }
protected:
	/** Fixed gameplay collision while the authored gate mesh falls into place. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UBoxComponent> RearGateBlocker;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	FTransform RearGateLandingTransform;
	FBox RearGateClosedBounds = FBox(ForceInit);
	float RearGateFallElapsed = 0.0f;
	bool bRearGateReady = false;
	bool bRearGateClosed = false;
	bool bRearClosurePending = false;
	bool bRearGateFalling = false;
	void TryCloseRearGate();
};
