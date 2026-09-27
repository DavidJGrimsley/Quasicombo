#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboRouteLeg.generated.h"

class UBoxComponent;
class ACombatEnemy;
class AQuasicomboBarrier;
class AQuasicomboRouteSection;

/** One placed A1/B1/C1-style route. Its trigger contains no level logic. */
UCLASS()
class AQuasicomboRouteLeg : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboRouteLeg();
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route") EQuasicomboLane Lane = EQuasicomboLane::B;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route") TObjectPtr<AQuasicomboRouteSection> Section;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Route") TObjectPtr<AQuasicomboBarrier> ForwardBarrier;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Encounter") TArray<TObjectPtr<ACombatEnemy>> Enemies;
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Route") void Prepare();
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Route") void Select();
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Route") void Reject();
	UFUNCTION(BlueprintPure, Category="Quasicombo|Encounter") bool IsCleared() const;
	UFUNCTION(BlueprintImplementableEvent, Category="Quasicombo|Encounter") void OnEncounterCleared();
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UBoxComponent> EntryTrigger;
	virtual void BeginPlay() override;
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
	UFUNCTION() void CheckEncounter();
	UFUNCTION() void HandleEnemyDestroyed(AActor* DestroyedActor);
private:
	bool bSelected = false;
	bool bClearNotified = false;
	TSet<TWeakObjectPtr<AActor>> RemovedEnemies;
	void SetEnemiesActive(bool bActive);
};
