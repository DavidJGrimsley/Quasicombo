#include "QuasicomboRunSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
	OnRunEnded.Broadcast(Outcome);
	if (Outcome == EQuasicomboRunOutcome::Defeat)
	{
		GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UQuasicomboRunSubsystem::ReloadRun, 1.5f, false);
	}
}

float UQuasicomboRunSubsystem::GetElapsedSeconds() const
{
	return FMath::Max(0.0f, (RunOutcome == EQuasicomboRunOutcome::Playing && GetWorld() ? GetWorld()->GetTimeSeconds() : EndTime) - StartTime);
}

void UQuasicomboRunSubsystem::RecordCombo(int32 Combo)
{
	HighestCombo = FMath::Max(HighestCombo, Combo);
}

void UQuasicomboRunSubsystem::RecordDamageTaken(float Amount)
{
	DamageTaken += FMath::Max(0.0f, Amount);
}

void UQuasicomboRunSubsystem::ReloadRun()
{
	if (GetWorld())
	{
		UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("Lvl_Main")));
	}
}
