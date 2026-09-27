// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "SideScrollingCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UAnimMontage;
class UCombatLifeBar;
class UWidgetComponent;
struct FInputActionValue;

/**
 *  A player-controllable side scrolling character with platforming dash
 *  and the Combat variant's melee/damage functionality.
 */
UCLASS(abstract)
class ASideScrollingCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Player camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	UCameraComponent* Camera;

	/** Combat life bar */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat", meta=(AllowPrivateAccess="true"))
	UWidgetComponent* LifeBar;

protected:

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Drop from Platform Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DropAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	/** Dash Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DashAction;

	/** Combo Attack Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ComboAttackAction;

	/** Charged Attack Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ChargedAttackAction;

	/** Impulse to manually push physics objects while we're in midair */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Jump")
	float JumpPushImpulse = 600.0f;

	/** Max distance that interactive objects can be triggered */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Interaction")
	float InteractionRadius = 200.0f;

	/** Time to disable input after a wall jump to preserve momentum */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float DelayBetweenWallJumps = 0.3f;

	/** Distance to trace ahead of the character for wall jumps */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpTraceDistance = 50.0f;

	/** Horizontal impulse to apply to the character during wall jumps */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpHorizontalImpulse = 500.0f;

	/** Multiplies the jump Z velocity for wall jumps. */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpVerticalMultiplier = 1.4f;

	/** Collision object type to use for soft collision traces (dropping down floors) */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Soft Platforms")
	TEnumAsByte<ECollisionChannel> SoftCollisionObjectType;

	/** Distance to trace down during soft collision checks */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Soft Platforms")
	float SoftCollisionTraceDistance = 1000.0f;

	/** Last recorded time when this character started falling */
	float LastFallTime = 0.0f;

	/** Max amount of time that can pass since we started falling when we allow a regular jump */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Coyote Time", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float MaxCoyoteTime = 0.16f;

	/** Wall jump lockout timer */
	FTimerHandle WallJumpTimer;

	/** Last captured horizontal movement input value */
	float ActionValueY = 0.0f;

	/** Last captured platform drop axis value */
	float DropValue = 0.0f;

	/** If true, this character has already wall jumped */
	bool bHasWallJumped = false;

	/** If true, this character has already double jumped */
	bool bHasDoubleJumped = false;

	/** If true, this character is moving along the side scrolling axis */
	bool bMovingHorizontally = false;

	/** If true, this character has already dashed since it last touched the ground */
	bool bHasDashed = false;

	/** If true, a dash is currently in progress */
	bool bIsDashing = false;

	/** Gravity scale to restore when a dash ends */
	float PreDashGravityScale = 1.75f;

	/** Velocity saved when a dash begins so an air dash does not erase jump momentum. */
	FVector PreDashVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Side Scrolling|Dash", meta=(ClampMin="0", Units="cm/s"))
	float DashHorizontalSpeed = 950.0f;

	UPROPERTY(EditAnywhere, Category="Side Scrolling|Dash", meta=(ClampMin="0.01", Units="s"))
	float ChargedDashStrikeDelay = 0.15f;

	UPROPERTY(EditAnywhere, Category="Side Scrolling|Dash", meta=(ClampMin="1.0"))
	float ChargedDashKnockbackMultiplier = 1.8f;

	bool bChargedDashAttack = false;
	FTimerHandle ChargedDashStrikeTimer;

	/** Dash montage ended delegate */
	FOnMontageEnded OnDashMontageEnded;

	/** AnimMontage used for the platforming dash */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Dash")
	UAnimMontage* DashMontage;

	/** Max HP */
	UPROPERTY(EditAnywhere, Category="Combat|Damage", meta=(ClampMin=0, ClampMax=100))
	float MaxHP = 5.0f;

	/** Current HP */
	UPROPERTY(VisibleAnywhere, Category="Combat|Damage")
	float CurrentHP = 0.0f;

	/** Time without taking damage before health starts recovering. */
	UPROPERTY(EditAnywhere, Category="Combat|Damage|Regeneration", meta=(ClampMin="0.0", Units="s"))
	float HealthRegenDelay = 3.0f;

	/** Fraction of maximum health recovered per second after the delay. */
	UPROPERTY(EditAnywhere, Category="Combat|Damage|Regeneration", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HealthRegenFractionPerSecond = 0.1f;

	float LastDamageTakenTime = 0.0f;
	bool bHasTakenDamage = false;

	/** Life bar fill color */
	UPROPERTY(EditAnywhere, Category="Combat|Damage")
	FLinearColor LifeBarColor = FLinearColor(0.1f, 0.8f, 0.2f, 1.0f);

	/** Pelvis bone used when blending hit reactions/ragdoll physics */
	UPROPERTY(EditAnywhere, Category="Combat|Damage")
	FName PelvisBoneName = TEXT("pelvis");

	/** Runtime life bar widget */
	UPROPERTY(Transient)
	TObjectPtr<UCombatLifeBar> LifeBarWidget;

	/** Max age of an attack input that may be carried into the next attack */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float AttackInputCacheTimeTolerance = 1.0f;

	/** Time at which an attack button was last pressed */
	float CachedAttackInputTime = -1000.0f;

	/** A fresh attack press can advance the montage by one section. */
	bool bComboAdvanceQueued = false;

	/** If true, an attack montage is currently playing */
	bool bIsAttacking = false;

	/** Distance ahead of the character that melee traces extend */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Trace", meta=(ClampMin=0, ClampMax=500, Units="cm"))
	float MeleeTraceDistance = 75.0f;

	/** Radius of melee attack sphere traces */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Trace", meta=(ClampMin=0, ClampMax=200, Units="cm"))
	float MeleeTraceRadius = 75.0f;

	/** Distance ahead of the character used to warn enemies */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Trace", meta=(ClampMin=0, ClampMax=500, Units="cm"))
	float DangerTraceDistance = 300.0f;

	/** Radius of the incoming-attack warning trace */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Trace", meta=(ClampMin=0, ClampMax=200, Units="cm"))
	float DangerTraceRadius = 100.0f;

	/** Damage dealt by a melee hit */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Damage", meta=(ClampMin=0, ClampMax=100))
	float MeleeDamage = 1.0f;

	/** Horizontal knockback impulse applied by a melee hit */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Damage", meta=(ClampMin=0, ClampMax=1000, Units="cm/s"))
	float MeleeKnockbackImpulse = 250.0f;

	/** Upward launch impulse applied by a melee hit */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Damage", meta=(ClampMin=0, ClampMax=1000, Units="cm/s"))
	float MeleeLaunchImpulse = 300.0f;

	/** Combo attack montage */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Combo")
	UAnimMontage* ComboAttackMontage;

	/** Montage section names used by the combo string. Auto-populated from the montage if empty. */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Combo")
	TArray<FName> ComboSectionNames;

	/** Max age of an input that can advance the combo */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Combo", meta=(ClampMin=0, ClampMax=5, Units="s"))
	float ComboInputCacheTimeTolerance = 0.45f;

	/** Current combo stage */
	int32 ComboCount = 0;

	/** Charged attack montage */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Charged")
	UAnimMontage* ChargedAttackMontage;

	/** Montage section used to loop charging */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Charged")
	FName ChargeLoopSection;

	/** Montage section used to release the charged attack */
	UPROPERTY(EditAnywhere, Category="Combat|Melee Attack|Charged")
	FName ChargeAttackSection;

	/** Whether the charged attack input is currently held */
	bool bIsChargingAttack = false;

	/** Whether the charged attack has reached its loop point */
	bool bHasLoopedChargedAttack = false;

	/** Whether the charged attack should resolve */
	bool bHasReleasedChargedAttack = false;

	/** Delay before destroying a dead character so the side-scroller controller can respawn it */
	UPROPERTY(EditAnywhere, Category="Combat|Respawn", meta=(ClampMin=0, ClampMax=10, Units="s"))
	float RespawnTime = 3.0f;

	/** Attack montage ended delegate */
	FOnMontageEnded OnAttackMontageEnded;

	/** Character respawn timer */
	FTimerHandle RespawnTimer;

	/** Number of consecutive damaging hits in the current gameplay combo */
	UPROPERTY(BlueprintReadOnly, Category="Combat|Hit Combo", meta=(AllowPrivateAccess="true"))
	int32 HitComboCount = 0;

	/**
	 * Seconds after the most recent damaging hit before the gameplay combo resets.
	 * Set to 0 to disable automatic timeout resets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Hit Combo", meta=(ClampMin=0, Units="s", AllowPrivateAccess="true"))
	float HitComboResetDelay = 5.0f;

	/** Timer used to expire the current gameplay combo */
	FTimerHandle HitComboResetTimer;
	TSet<AActor*> HitActorsThisStrike;
	int64 CurrentStrikeSerial = 0;

public:

	/** Constructor */
	ASideScrollingCharacter();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Recovers health after the damage-free delay. */
	virtual void Tick(float DeltaSeconds) override;

	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Collision handling */
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** Landing handling */
	virtual void Landed(const FHitResult& Hit) override;

	/** Handle movement mode changes to keep track of coyote time jumps */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for dash input */
	void Dash();

	/** Called for drop from platform input */
	void Drop(const FInputActionValue& Value);

	/** Called for drop from platform input release */
	void DropReleased(const FInputActionValue& Value);

	/** Called for combo attack input */
	void ComboAttackPressed();

	/** Called when charged attack input starts */
	void ChargedAttackPressed();

	/** Called when charged attack input ends */
	void ChargedAttackReleased();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Forward);

	/** Handles drop inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoDrop(float Value);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump released inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Handles dash inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoDash();

	/** Handles interact inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoInteract();

	/** Handles combo attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackStart();

	/** Handles combo attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackEnd();

	/** Handles charged attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackStart();

	/** Handles charged attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackEnd();

protected:

	/** Handles advanced jump logic */
	void MultiJump();

	/** Checks for soft collision with platforms */
	void CheckForSoftCollision();

	/** Resets wall jump lockout. Called from timer after a wall jump */
	void ResetWallJump();

	/** Called when the dash montage ends or is interrupted */
	void DashMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void PerformChargedDashStrike();

	/** Resets HP and UI */
	void ResetHP();

	/** Performs a combo attack */
	void ComboAttack();

	/** Performs a charged attack */
	void ChargedAttack();

	/** Called when an attack montage ends */
	void AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Updates the optional life bar widget */
	void UpdateLifeBar();

	/** Blueprint hook for optional dash/jump trails */
	UFUNCTION(BlueprintImplementableEvent, Category="Side Scrolling|Dash")
	void SetJumpTrailState(bool bEnabled);

	UFUNCTION(BlueprintImplementableEvent, Category="Side Scrolling|Dash")
	void OnChargedDashStarted();

public:

	/** Ends the current dash and restores gravity */
	UFUNCTION(BlueprintCallable, Category="Side Scrolling|Dash")
	void EndDash();

	/** Clears buffered combat and movement state before the finisher takes input. */
	UFUNCTION(BlueprintCallable, Category="Combat|Finisher")
	void PrepareForFinisher();

	/** Sets the soft collision response. True passes, False blocks */
	void SetSoftCollision(bool bEnabled);

	/** Returns true if the character has just double jumped */
	UFUNCTION(BlueprintPure, Category="Side Scrolling")
	bool HasDoubleJumped() const;

	/** Returns true if the character has just wall jumped */
	UFUNCTION(BlueprintPure, Category="Side Scrolling")
	bool HasWallJumped() const;

	/** Returns true while the dash state is active */
	UFUNCTION(BlueprintPure, Category="Side Scrolling")
	bool IsDashing() const { return bIsDashing; }

	/** Records a successful damaging hit and restarts the combo timeout */
	UFUNCTION(BlueprintCallable, Category="Combat|Hit Combo")
	void RegisterHitCombo();

	/** Lets the Blueprint broadcast combo changes to interested HUD widgets. */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat|Hit Combo")
	void OnHitComboStateChanged();

	/** Immediately clears the current gameplay combo */
	UFUNCTION(BlueprintCallable, Category="Combat|Hit Combo")
	void ResetHitCombo();

	/** Returns the current consecutive-hit combo count */
	UFUNCTION(BlueprintPure, Category="Combat|Hit Combo")
	int32 GetHitComboCount() const { return HitComboCount; }

	UFUNCTION(BlueprintPure, Category="Combat|Damage")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category="Combat|Damage")
	float GetMaxHP() const { return MaxHP; }

	/** Identifies one physical combo stage, charged release, or charged dash. */
	int64 GetCurrentStrikeSerial() const { return CurrentStrikeSerial; }

	/** Returns seconds remaining before the current combo expires, or 0 if no timeout is active */
	UFUNCTION(BlueprintPure, Category="Combat|Hit Combo")
	float GetHitComboTimeRemaining() const;

	// ~begin CombatAttacker interface

	/** Performs the collision check for an attack */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** Performs the combo string check */
	virtual void CheckCombo() override;

	/** Performs the charged attack hold check */
	virtual void CheckChargedAttack() override;

	/** Loops or resolves the charged attack animation */
	void LoopOrResolveChargedAttack();

	// ~end CombatAttacker interface

	// ~begin CombatDamageable interface

	/** Notifies nearby enemies that an attack is coming so they can react */
	void NotifyEnemiesOfIncomingAttack();

	/** Handles damage and knockback */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death */
	virtual void HandleDeath() override;

	/** Handles healing */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Allows reaction to incoming attacks */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end CombatDamageable interface

	/** Called by the respawn timer; destruction is handled by the side-scroller player controller */
	void RespawnCharacter();

	/** Overrides the default damage entry point */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:

	/** Blueprint handler to play damage dealt effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void DealtDamage(float Damage, const FVector& ImpactPoint);

	/** Blueprint handler to play damage received effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ReceivedDamage(float Damage, const FVector& ImpactPoint, const FVector& DamageDirection);
};
