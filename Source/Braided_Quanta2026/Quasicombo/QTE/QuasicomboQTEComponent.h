#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuasicomboQTESequence.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboQTEComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQuasicomboPromptChanged,EQuasicomboQTEInput,Input,int32,PromptNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQuasicomboQTEFinished,bool,bSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQuasicomboPromptAccepted,int32,Beat,bool,bChargeStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQuasicomboFinishingBeat);

UCLASS(ClassGroup=(Quasicombo),meta=(BlueprintSpawnableComponent))
class UQuasicomboQTEComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UQuasicomboQTEComponent();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="QTE",meta=(ClampMin="0.1")) float PromptWindowSeconds=1.5f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="QTE") float SuccessBeatSeconds=0.75f;
    UPROPERTY(BlueprintAssignable,Category="QTE") FQuasicomboPromptChanged OnPromptChanged;
    UPROPERTY(BlueprintAssignable,Category="QTE") FQuasicomboPromptAccepted OnPromptAccepted;
    UPROPERTY(BlueprintAssignable,Category="QTE") FQuasicomboFinishingBeat OnChargeReleased;
    UPROPERTY(BlueprintAssignable,Category="QTE") FQuasicomboFinishingBeat OnFinishingBeat;
    UPROPERTY(BlueprintAssignable,Category="QTE") FQuasicomboQTEFinished OnQTEFinished;
    UFUNCTION(BlueprintCallable,Category="QTE") void BeginQTE(bool bTauOutcome,int32 RoundCount=1);
    void BeginRound(bool bTau,int32 Round,int32 Rounds,int32 Count);
    UFUNCTION(BlueprintCallable,Category="QTE") void CancelQTE();
    UFUNCTION(BlueprintCallable,Category="QTE") bool SubmitPrompt(EQuasicomboQTEInput Input);
    bool SubmitEdge(EQuasicomboQTEInput Input,EQuasicomboQTEInputEdge Edge);
    void CompleteAction();
    void HoldPresentation() { Sequence.Hold(FPlatformTime::Seconds()); bActionInProgress=true; }
    void OpenPresentation() { bActionInProgress=false; Sequence.OpenNextPrompt(FPlatformTime::Seconds()); }
    UFUNCTION(BlueprintPure,Category="QTE") bool IsQTEActive() const { return bOwningInput; }
    UFUNCTION(BlueprintPure,Category="QTE") bool IsPromptOpen() const { return Sequence.IsActive()&&!bActionInProgress; }
    UFUNCTION(BlueprintPure,Category="QTE") bool IsCharging() const { return Sequence.IsCharging(); }
    UFUNCTION(BlueprintPure,Category="QTE") float GetChargeProgress() const;
    UFUNCTION(BlueprintPure,Category="QTE") bool IsReleaseWindowOpen() const { return IsCharging()&&GetChargeProgress()>=1.f; }
    UFUNCTION(BlueprintPure,Category="QTE") bool IsFinishingBeat() const { return bFinishingBeat; }
    UFUNCTION(BlueprintPure,Category="QTE") int32 GetPromptNumber() const { return Sequence.GetPromptNumber(); }
    UFUNCTION(BlueprintPure,Category="QTE") int32 GetPromptCount() const { return Sequence.GetPromptCount(); }
    UFUNCTION(BlueprintPure,Category="QTE") int32 GetRoundNumber() const { return Sequence.GetRoundNumber(); }
    UFUNCTION(BlueprintPure,Category="QTE") int32 GetRoundCount() const { return Sequence.GetRoundCount(); }
    UFUNCTION(BlueprintPure,Category="QTE") EQuasicomboQTEInput GetExpectedInput() const { return Sequence.GetExpectedInput(); }
    UFUNCTION(BlueprintPure,Category="QTE") EQuasicomboQTEInput GetFailedInput() const { return FailedInput; }
    UFUNCTION(BlueprintPure,Category="QTE") float GetSecondsRemaining() const;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class FQuasicomboQTEInterruptionTest;
    friend class FQuasicomboQTEDirectorTest;
    friend class FQuasicomboQTEAirborneChainTest;
    friend class FQuasicomboQTEFailureFeedbackTest;
    UFUNCTION() void HandleRunEnded(EQuasicomboRunOutcome Outcome);
    FQuasicomboQTESequence Sequence;
    EQuasicomboQTEInput FailedInput=EQuasicomboQTEInput::Invalid;
    bool bOwningInput=false,bActionInProgress=false,bFinishingBeat=false,bResultDelivered=false;
    void Finish(bool bSucceeded);
};
