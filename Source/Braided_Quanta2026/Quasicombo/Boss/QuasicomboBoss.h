#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SideScrollingCombatEnemy.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboBoss.generated.h"

class UQuantumBossComponent;
class UQuasicomboQTEComponent;
class ACameraActor;
class ASideScrollingCharacter;

UCLASS(Blueprintable)
class AQuasicomboBoss : public ASideScrollingCombatEnemy
{
	GENERATED_BODY()
public:
	AQuasicomboBoss();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Boss") TObjectPtr<UQuantumBossComponent> Quantum;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Boss") TObjectPtr<UQuasicomboQTEComponent> QTE;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Camera") TObjectPtr<ACameraActor> CloseupCamera;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Camera") TObjectPtr<ACameraActor> ImpactCamera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Camera", meta=(ClampMin="0.1", ClampMax="1.0")) float FinisherTimeDilation = 0.4f;
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") bool SubmitQTEInput(EQuasicomboQTEInput Input);
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") void ShowImpactCamera();
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") void RestoreCameraAndTime();
	UFUNCTION(BlueprintImplementableEvent, Category="Quasicombo|Boss") void OnFinisherStarted(bool bTauPattern);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnStrikeAccepted(int32 StrikeCount) override;
	virtual bool ShouldUseChargedSideAttack() const override;
	virtual void HandleRequiredHitsReached(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	UFUNCTION() void ApplyQuantumTuning(double Vacuum, double Tau, bool bFallback);
private:
	bool bFinisherStarted = false;
	bool bCameraOverride = false;
	bool bMovementFrozen = false;
	float PreviousTimeDilation = 1.0f;
	EMovementMode PreviousMovementMode = MOVE_Walking;
	TWeakObjectPtr<ASideScrollingCharacter> FrozenPlayer;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	void BeginFinisher();
};
