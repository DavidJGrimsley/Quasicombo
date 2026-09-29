#include "QuasicomboRunSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "QuasicomboBoss.h"

void UQuasicomboRunSubsystem::RegisterBossEncounter(AQuasicomboBoss* Boss) { ActiveBoss = Boss; }
bool UQuasicomboRunSubsystem::TryHandleBossFailure()
{
	return RunOutcome == EQuasicomboRunOutcome::Playing && ActiveBoss.IsValid() && ActiveBoss->HandleEncounterFailure();
}
bool UQuasicomboRunSubsystem::IsBossCombatLocked() const
{
	return ActiveBoss.IsValid() && ActiveBoss->IsPlayerCombatLocked();
}

namespace
{
struct FRouteTransitionDefinition
{
	int32 FirstGenerator;
	int32 FirstPower;
	int32 SecondGenerator;
	int32 SecondPower;
};

// Rows are the previous A/B/C lane; columns are the chosen A/B/C lane.
// Junction two reverses the crossing orientation when this word is appended.
constexpr FRouteTransitionDefinition Transitions[3][3] =
{
	{{0, 0, 0, 0}, {1, 1, 0, 0}, {1, 1, 2, 1}},
	{{1, -1, 0, 0}, {0, 0, 0, 0}, {2, 1, 0, 0}},
	{{2, -1, 1, -1}, {2, -1, 0, 0}, {0, 0, 0, 0}}
};
}

void UQuasicomboRunSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	StartTime = InWorld.GetTimeSeconds();
}

bool UQuasicomboRunSubsystem::TryCommitRoute(int32 SectionNumber, EQuasicomboLane Lane)
{
	if (RunOutcome != EQuasicomboRunOutcome::Playing || SectionNumber < 1 || SectionNumber > 3 ||
		SectionNumber != RouteHistory.Num() + 1 ||
		Lane < EQuasicomboLane::A || Lane > EQuasicomboLane::C)
	{
		return false;
	}
	const int32 Previous = RouteHistory.IsEmpty() ? 1 : static_cast<int32>(RouteHistory.Last());
	const int32 Next = static_cast<int32>(Lane);
	const FRouteTransitionDefinition& Transition = Transitions[Previous][Next];
	const int32 Orientation = SectionNumber == 2 ? -1 : 1;
	if (Transition.FirstGenerator)
	{
		FQuasicomboBraidOperation& Operation = BraidOperations.AddDefaulted_GetRef();
		Operation.Generator = Transition.FirstGenerator;
		Operation.Power = Transition.FirstPower * Orientation;
	}
	if (Transition.SecondGenerator)
	{
		FQuasicomboBraidOperation& Operation = BraidOperations.AddDefaulted_GetRef();
		Operation.Generator = Transition.SecondGenerator;
		Operation.Power = Transition.SecondPower * Orientation;
	}
	RouteHistory.Add(Lane);
	OnRouteCommitted.Broadcast(SectionNumber, Lane);
	return true;
}

void UQuasicomboRunSubsystem::EndRun(EQuasicomboRunOutcome Outcome)
{
	if (RunOutcome != EQuasicomboRunOutcome::Playing || Outcome == EQuasicomboRunOutcome::Playing || !GetWorld())
	{
		return;
	}
	RunOutcome = Outcome;
	EndTime = GetWorld()->GetTimeSeconds();
	if (Outcome == EQuasicomboRunOutcome::Victory)
	{
		VictoryScore = CalculateScore(GetElapsedSeconds(), HighestCombo, BraidOperations, MeasuredOutcome);
	}
	OnRunEnded.Broadcast(Outcome);
	if (Outcome == EQuasicomboRunOutcome::Defeat)
	{
		GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UQuasicomboRunSubsystem::ReloadRun, 1.5f, false);
	}
}

void UQuasicomboRunSubsystem::ExtendDefeatReload(float MinimumSeconds)
{
	if (RunOutcome != EQuasicomboRunOutcome::Defeat || !GetWorld()) return;
	if (GetWorld()->GetTimerManager().GetTimerRemaining(ReloadTimer) < MinimumSeconds)
	{
		GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UQuasicomboRunSubsystem::ReloadRun,
			MinimumSeconds, false);
	}
}

float UQuasicomboRunSubsystem::GetElapsedSeconds() const
{
	return FMath::Max(0.0f, (RunOutcome == EQuasicomboRunOutcome::Playing && GetWorld() ? GetWorld()->GetTimeSeconds() : EndTime) - StartTime);
}

int32 UQuasicomboRunSubsystem::GetVictoryScore() const
{
	return GetVictoryScoreBreakdown().Total;
}

FQuasicomboScoreBreakdown UQuasicomboRunSubsystem::GetVictoryScoreBreakdown() const
{
	return RunOutcome == EQuasicomboRunOutcome::Victory ? VictoryScore : FQuasicomboScoreBreakdown{};
}

void UQuasicomboRunSubsystem::RecordFinisherMeasurement(bool bTauOutcome, bool bHasQuantumState,
	double VacuumProbability, double TauProbability, bool bFallback)
{
	if (RunOutcome != EQuasicomboRunOutcome::Playing || MeasuredOutcome != EQuasicomboMeasuredOutcome::Unavailable) return;
	MeasuredOutcome = bTauOutcome ? EQuasicomboMeasuredOutcome::Tau : EQuasicomboMeasuredOutcome::Vacuum;
	bHasFinalQuantumState = bHasQuantumState;
	if (bHasFinalQuantumState)
	{
		FinalVacuumProbability = VacuumProbability;
		FinalTauProbability = TauProbability;
		bUsedQuantumFallback = bFallback;
	}
}

TArray<FQuasicomboBraidOperation> UQuasicomboRunSubsystem::ReduceBraid(const TArray<FQuasicomboBraidOperation>& Operations)
{
	TArray<FQuasicomboBraidOperation> Reduced;
	for (const FQuasicomboBraidOperation& Operation : Operations)
	{
		if (!Reduced.IsEmpty() && Reduced.Last().Generator == Operation.Generator &&
			Reduced.Last().Power == -Operation.Power)
		{
			Reduced.Pop();
		}
		else
		{
			Reduced.Add(Operation);
		}
	}
	return Reduced;
}

FQuasicomboScoreBreakdown UQuasicomboRunSubsystem::CalculateScore(float ElapsedSeconds, int32 BestCombo,
	const TArray<FQuasicomboBraidOperation>& Operations, EQuasicomboMeasuredOutcome Outcome)
{
	FQuasicomboScoreBreakdown Score;
	Score.TimePoints = FMath::RoundToInt(10000.0f * 90.0f / (90.0f + FMath::Max(0.0f, ElapsedSeconds)));
	Score.ComboPoints = FMath::Max(0, BestCombo) * 200;
	Score.NetCrossings = ReduceBraid(Operations).Num();
	Score.BraidPoints = Score.NetCrossings * 350;
	Score.QuantumPoints = Outcome == EQuasicomboMeasuredOutcome::Tau ? 2000 : 0;
	Score.Total = Score.TimePoints + Score.ComboPoints + Score.BraidPoints + Score.QuantumPoints;
	return Score;
}

void UQuasicomboRunSubsystem::RecordCombo(int32 Combo)
{
	if (RunOutcome != EQuasicomboRunOutcome::Playing) return;
	HighestCombo = FMath::Max(HighestCombo, Combo);
}

void UQuasicomboRunSubsystem::RecordDamageTaken(float Amount)
{
	if (RunOutcome != EQuasicomboRunOutcome::Playing) return;
	DamageTaken += FMath::Max(0.0f, Amount);
}

void UQuasicomboRunSubsystem::ReloadRun()
{
	if (GetWorld())
	{
		UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("Lvl_Main")));
	}
}
