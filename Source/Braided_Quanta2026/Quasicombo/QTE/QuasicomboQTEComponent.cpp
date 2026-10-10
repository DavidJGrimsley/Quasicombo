#include "QuasicomboQTEComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboBoss.h"
#include "QuasicomboQTEDirector.h"
#include "Engine/World.h"

UQuasicomboQTEComponent::UQuasicomboQTEComponent()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bTickEvenWhenPaused=true;
}
void UQuasicomboQTEComponent::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
        Run->OnRunEnded.AddDynamic(this,&UQuasicomboQTEComponent::HandleRunEnded);
}
void UQuasicomboQTEComponent::BeginQTE(bool bTau,int32 Rounds) { BeginRound(bTau,1,Rounds,3); }
void UQuasicomboQTEComponent::BeginRound(bool bTau,int32 Round,int32 Rounds,int32 Count)
{
    if(bOwningInput||!GetWorld()) return;
    const auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
    if(!Run||Run->GetOutcome()!=EQuasicomboRunOutcome::Playing) return;
    bResultDelivered=bActionInProgress=bFinishingBeat=false; bOwningInput=true;
    FailedInput=EQuasicomboQTEInput::Invalid;
    Sequence.Begin(bTau,FPlatformTime::Seconds(),PromptWindowSeconds,Rounds,Count,Round);
    OnPromptChanged.Broadcast(GetExpectedInput(),1);
}
float UQuasicomboQTEComponent::GetSecondsRemaining() const
{
    return float(Sequence.GetSecondsRemaining(FPlatformTime::Seconds(),GetWorld()&&GetWorld()->IsPaused()));
}
float UQuasicomboQTEComponent::GetChargeProgress() const
{
    return float(Sequence.GetChargeSeconds(FPlatformTime::Seconds(),GetWorld()&&GetWorld()->IsPaused()));
}
bool UQuasicomboQTEComponent::SubmitPrompt(EQuasicomboQTEInput Input) { return SubmitEdge(Input,EQuasicomboQTEInputEdge::Press); }
bool UQuasicomboQTEComponent::SubmitEdge(EQuasicomboQTEInput Input,EQuasicomboQTEInputEdge Edge)
{
    if(!bOwningInput) return false;
    if(bResultDelivered||bActionInProgress||bFinishingBeat) return true;
    const int32 Beat=Sequence.GetPromptNumber();
    const auto Expected=Sequence.GetExpectedInput();
    const auto Result=Sequence.Submit(Input,FPlatformTime::Seconds(),GetWorld()->IsPaused(),Edge);
    if(Result==EQuasicomboQTEProgress::Failed) { FailedInput=Expected; Finish(false); }
    else if(Result==EQuasicomboQTEProgress::Charging) OnPromptAccepted.Broadcast(Beat,true);
    else if(Result==EQuasicomboQTEProgress::Advanced||Result==EQuasicomboQTEProgress::Succeeded)
    {
        bActionInProgress=true;
        bFinishingBeat=Result==EQuasicomboQTEProgress::Succeeded;
        Sequence.Hold(FPlatformTime::Seconds());
        if(Edge==EQuasicomboQTEInputEdge::Release) OnChargeReleased.Broadcast();
        else OnPromptAccepted.Broadcast(Beat,false);
        if(bFinishingBeat) OnFinishingBeat.Broadcast();
        if(!Cast<AQuasicomboQTEDirector>(GetOwner())) CompleteAction();
    }
    return true;
}
void UQuasicomboQTEComponent::CompleteAction()
{
    if(bResultDelivered||!bActionInProgress) return;
    bActionInProgress=false;
    if(bFinishingBeat) { Finish(true); return; }
    Sequence.OpenNextPrompt(FPlatformTime::Seconds());
    OnPromptChanged.Broadcast(GetExpectedInput(),GetPromptNumber());
}
void UQuasicomboQTEComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DeltaTime,TickType,Tick);
    const auto Expected=Sequence.GetExpectedInput();
    if(Sequence.Advance(FPlatformTime::Seconds(),GetWorld()&&GetWorld()->IsPaused())==EQuasicomboQTEProgress::Failed)
    { FailedInput=Expected; Finish(false); }
}
void UQuasicomboQTEComponent::Finish(bool bSucceeded)
{
    if(bResultDelivered) return;
    bResultDelivered=true; Sequence.Cancel(); bActionInProgress=bFinishingBeat=false;
    OnQTEFinished.Broadcast(bSucceeded);
    if(Cast<AQuasicomboQTEDirector>(GetOwner())) return; // Director keeps input through the outro.
    bOwningInput=false;
    if(auto* Boss=Cast<AQuasicomboBoss>(GetOwner())) { Boss->RestoreCameraAndTime(); Boss->HandleQTEResult(bSucceeded); }
}
void UQuasicomboQTEComponent::CancelQTE()
{
    Sequence.Cancel(); bOwningInput=bActionInProgress=bFinishingBeat=false; bResultDelivered=true;
    FailedInput=EQuasicomboQTEInput::Invalid;
}
void UQuasicomboQTEComponent::HandleRunEnded(EQuasicomboRunOutcome)
{
    const bool bNotify=bOwningInput&&!bResultDelivered;
    CancelQTE(); if(bNotify) OnQTEFinished.Broadcast(false);
}
void UQuasicomboQTEComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(GetWorld()) if(auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
        Run->OnRunEnded.RemoveDynamic(this,&UQuasicomboQTEComponent::HandleRunEnded);
    CancelQTE(); Super::EndPlay(Reason);
}
