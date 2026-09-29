#pragma once

#include "CoreMinimal.h"
#include "CombatEnemy.h"
#include "SideScrollingCombatEnemy.generated.h"

UENUM(BlueprintType)
enum class ESideCombatState : uint8 { Idle, Approach, Attack, Recovery, HitReaction, Dead };

/** An independent 2.5D melee enemy; its simple state loop does not use 3D flank EQS. */
UCLASS(Blueprintable)
class ASideScrollingCombatEnemy : public ACombatEnemy
{
	GENERATED_BODY()
public:
	ASideScrollingCombatEnemy();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="1")) int32 RequiredHits = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AggroRange = 550.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackRange = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float PatrolHalfWidth = 450.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackCooldown = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.01", Units="s")) float AttackWindup = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.01", Units="s")) float ChargedAttackWindup = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.0", Units="s")) float RecoveryDuration = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.0", Units="s")) float HitReactionDuration = 0.3f;
	/** If false, species/tier or boss armor supplies Toughness automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	bool bOverrideToughness = false;

	/**
	 * Chance to shrug off the stagger from a non-lethal hit.
	 * 0 = always stagger, 0.8 = 80% chance to ignore stagger, 1 = never stagger.
	 * Toughness does not reduce damage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat",
		meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bOverrideToughness"))
	float Toughness = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.1", Units="s")) float AttackTimeout = 2.5f;
	UPROPERTY(BlueprintReadOnly, Category="Combat") ESideCombatState CombatState = ESideCombatState::Idle;
	UFUNCTION(BlueprintPure, Category="Combat") int32 GetAcceptedStrikes() const { return AcceptedStrikes; }
	UFUNCTION(BlueprintPure, Category="Combat") int32 GetRemainingStrikes() const { return FMath::Max(0, RequiredHits - AcceptedStrikes); }
	UFUNCTION(BlueprintPure, Category="Combat") bool IsCombatDefeated() const { return AcceptedStrikes >= RequiredHits; }
	UFUNCTION(BlueprintPure, Category="Combat") float GetToughness() const { return FMath::Clamp(Toughness, 0.0f, 1.0f); }
	UFUNCTION(BlueprintCallable, Category="Combat") void SetToughness(float NewToughness);
	UFUNCTION(BlueprintPure, Category="Combat") bool DidLastStrikeStagger() const { return bLastStrikeTriggeredHitReaction; }
	UFUNCTION(BlueprintPure, Category="Combat") bool IsChargedAttackActive() const { return bChargedAttackActive; }
	UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnAcceptedStrike(int32 StrikeCount, int32 StrikesRequired);
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void DoAttackTrace(FName DamageSourceBone) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnStrikeAccepted(int32 StrikeCount);
	/** Boss variants can choose a charged montage while retaining one-hit timing guards. */
	virtual bool ShouldUseChargedSideAttack() const;
	virtual bool CanProcessSideCombat() const { return true; }
	virtual bool StartCustomSideAttack(bool bCharged) { return false; }
	virtual void ResolveAcceptedStrike(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse);
	/** Applies HP/effects while respecting whether the current accepted strike actually staggered this enemy. */
	void ApplyAcceptedStrikeDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse);
	bool DidLastStrikeTriggerHitReaction() const { return bLastStrikeTriggeredHitReaction; }
	void SetAutomaticToughness(float NewToughness);
	void SuspendSideCombat();
	void ResetSideCombat(int32 Hits);
	void FinishSideAttack();
	void SuppressFallbackAttackTrace() { bAttackTraceFired = true; }
	virtual void HandleRequiredHitsReached(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse);
	UPROPERTY(BlueprintReadOnly, Category="Combat") int32 AcceptedStrikes = 0;
private:
	TWeakObjectPtr<AActor> LastStrikeCauser;
	int64 LastAcceptedStrikeSerial = -1;
	float SpawnX = 0.0f;
	float NextAttackTime = 0.0f;
	float StateEndsAt = 0.0f;
	bool bSideAttackActive = false;
	bool bStartingSideAttack = false;
	bool bAttackTraceFired = false;
	bool bChargedAttackActive = false;
	float ChargedStrikeReadyAt = 0.0f;
	bool bLastStrikeTriggeredHitReaction = true;
	FTimerHandle AttackTraceFallbackTimer;
	FTimerHandle AttackTimeoutTimer;
	FTimerHandle HitReactionPhysicsTimer;
	bool HasFloorAhead(float Direction) const;
	void StartSideAttack();
	void FallbackAttackTrace();
	void TimeoutSideAttack();
	bool ShouldTriggerHitReaction();
	float GetEffectiveHitReactionDuration() const;
	void ResetHitReactionPhysics();
};
