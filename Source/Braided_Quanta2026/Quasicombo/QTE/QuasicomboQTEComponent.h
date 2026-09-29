#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuasicomboQTESequence.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboQTEComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQuasicomboPromptChanged, EQuasicomboQTEInput, Input, int32, PromptNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQuasicomboQTEFinished, bool, bSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQuasicomboFinishingBeat);

UCLASS(ClassGroup=(Quasicombo), meta=(BlueprintSpawnableComponent))
class UQuasicomboQTEComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UQuasicomboQTEComponent();
	/** Real-time seconds per prompt. Kept generous while tuning the playable blockout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QTE", meta=(ClampMin="0.1")) float PromptWindowSeconds = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QTE", meta=(ClampMin="0.0")) float SuccessBeatSeconds = 0.75f;
	UPROPERTY(BlueprintAssignable, Category="QTE") FQuasicomboPromptChanged OnPromptChanged;
	UPROPERTY(BlueprintAssignable, Category="QTE") FQuasicomboFinishingBeat OnFinishingBeat;
	UPROPERTY(BlueprintAssignable, Category="QTE") FQuasicomboQTEFinished OnQTEFinished;
	UFUNCTION(BlueprintCallable, Category="QTE") void BeginQTE(bool bTauOutcome, int32 RoundCount = 1);
	UFUNCTION(BlueprintCallable, Category="QTE") void CancelQTE();
	UFUNCTION(BlueprintPure, Category="QTE") int32 GetRoundNumber() const { return Sequence.GetRoundNumber(); }
	UFUNCTION(BlueprintPure, Category="QTE") int32 GetRoundCount() const { return Sequence.GetRoundCount(); }
	/** Returns true whenever the QTE owns gameplay input, including a failed prompt. */
	UFUNCTION(BlueprintCallable, Category="QTE") bool SubmitPrompt(EQuasicomboQTEInput Input);
	UFUNCTION(BlueprintPure, Category="QTE") bool IsQTEActive() const { return Sequence.IsActive() || bFinishingBeat; }
	UFUNCTION(BlueprintPure, Category="QTE") bool IsFinishingBeat() const { return bFinishingBeat; }
	UFUNCTION(BlueprintPure, Category="QTE") EQuasicomboQTEInput GetExpectedInput() const;
	UFUNCTION(BlueprintPure, Category="QTE") int32 GetPromptNumber() const { return Sequence.GetPromptNumber(); }
	UFUNCTION(BlueprintPure, Category="QTE") float GetSecondsRemaining() const;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	friend class FQuasicomboQTEInterruptionTest;
	UFUNCTION() void HandleRunEnded(EQuasicomboRunOutcome Outcome);
	FQuasicomboQTESequence Sequence;
	bool bFinishingBeat = false;
	bool bResultDelivered = false;
	double SuccessBeatDeadline = 0.0;
	double LastSuccessBeatClockSample = 0.0;
	void Finish(bool bSucceeded);
};
