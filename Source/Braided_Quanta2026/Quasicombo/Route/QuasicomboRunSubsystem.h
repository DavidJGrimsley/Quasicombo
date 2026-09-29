#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboRunSubsystem.generated.h"

class AQuasicomboBoss;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQuasicomboRouteCommitted, int32, Section, EQuasicomboLane, Lane);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQuasicomboRunEnded, EQuasicomboRunOutcome, Outcome);

UENUM(BlueprintType)
enum class EQuasicomboMeasuredOutcome : uint8
{
	Unavailable,
	Vacuum,
	Tau
};

USTRUCT(BlueprintType)
struct FQuasicomboScoreBreakdown
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Quasicombo|Score") int32 TimePoints = 0;
	UPROPERTY(BlueprintReadOnly, Category="Quasicombo|Score") int32 ComboPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category="Quasicombo|Score") int32 BraidPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category="Quasicombo|Score") int32 QuantumPoints = 0;
	UPROPERTY(BlueprintReadOnly, Category="Quasicombo|Score") int32 NetCrossings = 0;
	UPROPERTY(BlueprintReadOnly, Category="Quasicombo|Score") int32 Total = 0;
};

/** Run state belongs to the current world; reloading the map naturally clears it. */
UCLASS()
class UQuasicomboRunSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void RegisterBossEncounter(AQuasicomboBoss* Boss);
	bool TryHandleBossFailure();
	bool IsBossCombatLocked() const;
	UPROPERTY(BlueprintAssignable, Category="Quasicombo|Run") FQuasicomboRouteCommitted OnRouteCommitted;
	UPROPERTY(BlueprintAssignable, Category="Quasicombo|Run") FQuasicomboRunEnded OnRunEnded;

	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run")
	bool TryCommitRoute(int32 SectionNumber, EQuasicomboLane Lane);

	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run")
	void EndRun(EQuasicomboRunOutcome Outcome);
	/** Let a player death animation and ragdoll finish before the level reloads. */
	void ExtendDefeatReload(float MinimumSeconds);

	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") int32 GetCurrentSection() const { return RouteHistory.Num(); }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") TArray<EQuasicomboLane> GetRouteHistory() const { return RouteHistory; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") TArray<FQuasicomboBraidOperation> GetBraidOperations() const { return BraidOperations; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") EQuasicomboRunOutcome GetOutcome() const { return RunOutcome; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") float GetElapsedSeconds() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") int32 GetHighestCombo() const { return HighestCombo; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") int32 GetVictoryScore() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") FQuasicomboScoreBreakdown GetVictoryScoreBreakdown() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") EQuasicomboMeasuredOutcome GetMeasuredOutcome() const { return MeasuredOutcome; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") bool HasFinalQuantumState() const { return bHasFinalQuantumState; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") double GetFinalVacuumProbability() const { return FinalVacuumProbability; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") double GetFinalTauProbability() const { return FinalTauProbability; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") bool UsedQuantumFallback() const { return bUsedQuantumFallback; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Run") float GetDamageTaken() const { return DamageTaken; }
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run") void RecordCombo(int32 Combo);
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Run") void RecordDamageTaken(float Amount);
	void RecordFinisherMeasurement(bool bTauOutcome, bool bHasQuantumState, double VacuumProbability,
		double TauProbability, bool bFallback);
	static TArray<FQuasicomboBraidOperation> ReduceBraid(const TArray<FQuasicomboBraidOperation>& Operations);
	static FQuasicomboScoreBreakdown CalculateScore(float ElapsedSeconds, int32 BestCombo,
		const TArray<FQuasicomboBraidOperation>& Operations, EQuasicomboMeasuredOutcome Outcome);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	TWeakObjectPtr<AQuasicomboBoss> ActiveBoss;
	UPROPERTY() TArray<EQuasicomboLane> RouteHistory;
	UPROPERTY() TArray<FQuasicomboBraidOperation> BraidOperations;
	EQuasicomboRunOutcome RunOutcome = EQuasicomboRunOutcome::Playing;
	float StartTime = 0.0f;
	float EndTime = 0.0f;
	int32 HighestCombo = 0;
	float DamageTaken = 0.0f;
	EQuasicomboMeasuredOutcome MeasuredOutcome = EQuasicomboMeasuredOutcome::Unavailable;
	bool bHasFinalQuantumState = false;
	double FinalVacuumProbability = 0.0;
	double FinalTauProbability = 0.0;
	bool bUsedQuantumFallback = false;
	FQuasicomboScoreBreakdown VictoryScore;
	FTimerHandle ReloadTimer;
	void ReloadRun();
};
