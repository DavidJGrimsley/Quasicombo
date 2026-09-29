#include "QuasicomboQTEComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboBoss.h"
#include "Engine/World.h"

UQuasicomboQTEComponent::UQuasicomboQTEComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void UQuasicomboQTEComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		Run->OnRunEnded.AddDynamic(this, &UQuasicomboQTEComponent::HandleRunEnded);
	}
}

void UQuasicomboQTEComponent::BeginQTE(bool bTauOutcome, int32 RoundCount)
{
	if (IsQTEActive() || !GetWorld()) return;
	const UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	if (!Run || Run->GetOutcome() != EQuasicomboRunOutcome::Playing) return;
	bResultDelivered = false;
	Sequence.Begin(bTauOutcome, FPlatformTime::Seconds(), PromptWindowSeconds, RoundCount);
	OnPromptChanged.Broadcast(GetExpectedInput(), 1);
}

EQuasicomboQTEInput UQuasicomboQTEComponent::GetExpectedInput() const
{
	return Sequence.GetExpectedInput();
}

float UQuasicomboQTEComponent::GetSecondsRemaining() const
{
	return static_cast<float>(Sequence.GetSecondsRemaining(FPlatformTime::Seconds(), GetWorld() && GetWorld()->IsPaused()));
}

bool UQuasicomboQTEComponent::SubmitPrompt(EQuasicomboQTEInput Input)
{
	if (bFinishingBeat) return true;
	if (!Sequence.IsActive()) return false;
	const EQuasicomboQTEProgress Result = Sequence.Submit(Input, FPlatformTime::Seconds(), GetWorld() && GetWorld()->IsPaused());
	if (Result == EQuasicomboQTEProgress::Failed) Finish(false);
	else if (Result == EQuasicomboQTEProgress::Succeeded)
	{
		bFinishingBeat = true;
		LastSuccessBeatClockSample = FPlatformTime::Seconds();
		SuccessBeatDeadline = LastSuccessBeatClockSample + FMath::Max(0.0f, SuccessBeatSeconds);
		if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(GetOwner())) Boss->ShowImpactCamera();
		OnFinishingBeat.Broadcast();
		if (SuccessBeatSeconds <= 0.0f) Finish(true);
	}
	else if (Result == EQuasicomboQTEProgress::Advanced)
	{
		OnPromptChanged.Broadcast(GetExpectedInput(), GetPromptNumber());
		if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(GetOwner())) Boss->ShowImpactCamera();
	}
	return true;
}

void UQuasicomboQTEComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double Now = FPlatformTime::Seconds();
	const bool bPaused = GetWorld() && GetWorld()->IsPaused();
	if (Sequence.IsActive() && Sequence.Advance(Now, bPaused) == EQuasicomboQTEProgress::Failed)
	{
		Finish(false);
	}
	if (bFinishingBeat)
	{
		if (bPaused) SuccessBeatDeadline += FMath::Max(0.0, Now - LastSuccessBeatClockSample);
		LastSuccessBeatClockSample = Now;
		if (!bPaused && Now >= SuccessBeatDeadline) Finish(true);
	}
}

void UQuasicomboQTEComponent::Finish(bool bSucceeded)
{
	if (bResultDelivered) return;
	bResultDelivered = true;
	Sequence.Cancel();
	bFinishingBeat = false;
	UQuasicomboRunSubsystem* Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(GetOwner())) Boss->RestoreCameraAndTime();
	OnQTEFinished.Broadcast(bSucceeded);
	if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(GetOwner()))
	{
		Boss->HandleQTEResult(bSucceeded);
		return;
	}
	if (Run) Run->EndRun(bSucceeded ? EQuasicomboRunOutcome::Victory : EQuasicomboRunOutcome::Defeat);
}

void UQuasicomboQTEComponent::CancelQTE()
{
	Sequence.Cancel();
	bFinishingBeat = false;
	bResultDelivered = true;
}

void UQuasicomboQTEComponent::HandleRunEnded(EQuasicomboRunOutcome)
{
	if (!IsQTEActive() || bResultDelivered) return;
	bResultDelivered = true;
	Sequence.Cancel();
	bFinishingBeat = false;
	if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(GetOwner())) Boss->RestoreCameraAndTime();
	OnQTEFinished.Broadcast(false);
}

void UQuasicomboQTEComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>())
		{
			Run->OnRunEnded.RemoveDynamic(this, &UQuasicomboQTEComponent::HandleRunEnded);
		}
	}
	Sequence.Cancel();
	bFinishingBeat = false;
	bResultDelivered = true;
	if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(GetOwner())) Boss->RestoreCameraAndTime();
	Super::EndPlay(EndPlayReason);
}
