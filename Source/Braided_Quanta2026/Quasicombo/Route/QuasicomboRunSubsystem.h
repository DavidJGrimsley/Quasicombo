#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboRunSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQuasicomboRouteCommitted, int32, Section, EQuasicomboLane, Lane);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQuasicomboRunEnded, EQuasicomboRunOutcome, Outcome);

/** Run state belongs to the current world; reloading the map naturally clears it. */
UCLASS()
class UQuasicomboRunSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable, Category="Quasicombo|Run") FQuasicomboRouteCommitted OnRouteCommitted;
	UPROPERTY(BlueprintAssignable, Category="Quasicombo|Run") FQuasicomboRunEnded OnRunEnded;

	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run")
	bool TryCommitRoute(int32 SectionNumber, EQuasicomboLane Lane);

	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run")
	void EndRun(EQuasicomboRunOutcome Outcome);

	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") int32 GetCurrentSection() const { return RouteHistory.Num(); }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") TArray<EQuasicomboLane> GetRouteHistory() const { return RouteHistory; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") TArray<FQuasicomboBraidOperation> GetBraidOperations() const { return BraidOperations; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") EQuasicomboRunOutcome GetOutcome() const { return RunOutcome; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") float GetElapsedSeconds() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") int32 GetHighestCombo() const { return HighestCombo; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") float GetDamageTaken() const { return DamageTaken; }
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run") void RecordCombo(int32 Combo);
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run") void RecordDamageTaken(float Amount);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	UPROPERTY() TArray<EQuasicomboLane> RouteHistory;
	UPROPERTY() TArray<FQuasicomboBraidOperation> BraidOperations;
	EQuasicomboRunOutcome RunOutcome = EQuasicomboRunOutcome::Playing;
	float StartTime = 0.0f;
	float EndTime = 0.0f;
	int32 HighestCombo = 0;
	float DamageTaken = 0.0f;
	FTimerHandle ReloadTimer;
	void ReloadRun();
};
