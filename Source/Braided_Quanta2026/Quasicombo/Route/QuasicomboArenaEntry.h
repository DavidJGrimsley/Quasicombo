#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboArenaEntry.generated.h"

class UBoxComponent;
class AQuasicomboRouteLeg;

/** Safe final-lane connection into the common blockout boss arena. */
UCLASS()
class AQuasicomboArenaEntry : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboArenaEntry();
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Arena") EQuasicomboLane Lane = EQuasicomboLane::B;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Arena") TObjectPtr<AQuasicomboRouteLeg> FinalLeg;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Arena") FVector ArrivalLocation = FVector(28000, 1000, 1100);
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UBoxComponent> Trigger;
	virtual void BeginPlay() override;
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
