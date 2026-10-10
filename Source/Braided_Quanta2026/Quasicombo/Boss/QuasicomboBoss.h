#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SideScrollingCombatEnemy.h"
#include "QuantumBossComponent.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboBoss.generated.h"

class AQuasicomboQTEDirector;
class UPhysicsAsset;
class UQuasicomboEvolutionComponent;
class UQuasicomboEncounterLightingComponent;
class ACameraActor;
class ASideScrollingCharacter;
class UAudioComponent;
class USoundBase;
class UMaterialInstanceDynamic;
class UAnimSequence;
class UPointLightComponent;
class AQuasicomboVictoryPickup;
struct FMinimalViewInfo;

UENUM(BlueprintType)
enum class EQuasicomboBossPhase : uint8
{
	Waiting,
	Combat,
	Evolution,
	QTE,
	Retry,
	AwaitPickup,
	Complete
};

UCLASS(Blueprintable)
class AQuasicomboBoss : public ASideScrollingCombatEnemy
{
	GENERATED_BODY()
    friend class FQuasicomboQTEDirectorTest;
    friend class FQuasicomboQTEJumpUppercutTest;
    friend class FQuasicomboQTEPromptPoseHoldTest;
    friend class FQuasicomboQTEAirborneChainTest;
    friend class FQuasicomboQTEFailureFeedbackTest;
public:
	AQuasicomboBoss();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quasicombo|Evolution")
	TObjectPtr<UQuasicomboEvolutionComponent> Evolution;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quasicombo|Lighting")
	TObjectPtr<UQuasicomboEncounterLightingComponent> EncounterLighting;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Appearance")
	TArray<TObjectPtr<UMaterialInterface>> EvolutionEyeMaterials;
	void ResolveEvolutionPresentation();
	void RevealEvolutionEyes(float Charge = 1.0f);
	void PreviewEvolutionEyes(bool bAwarded, float Charge = 1.0f);
	bool GetEvolutionGameplayView(FMinimalViewInfo& View) const;
	void SetEvolutionPlayerVisible(bool bVisible);
	void CompleteEvolutionPresentation();
	void AbortEvolutionPresentation();
	TArray<USkeletalMeshComponent*> GetEvolutionArmor() const;
	bool ArmorInGroup(int32 Piece, int32 Group) const;
	float GetReactionAnimationWeight() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	FString GetEvolutionBuffs() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	int32 GetEvolutionLevel() const { return AwardedArmorBars; }
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	bool IsOpeningChargeActive() const { return bOpeningCharge; }
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quasicombo|Boss")
	TObjectPtr<UQuantumBossComponent> Quantum;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quasicombo|Boss")
	TObjectPtr<UQuasicomboQTEComponent> QTE;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Quasicombo|Camera")
	TObjectPtr<ACameraActor> CloseupCamera;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Quasicombo|Camera")
	TObjectPtr<ACameraActor> ImpactCamera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quasicombo|Camera", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float FinisherTimeDilation = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quasicombo|Boss", meta = (ClampMin = "4"))
	int32 BaseHealthHits = 10;
	/** Remaining base HP that triggers evolution; default is one hit before death. Shields are separate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quasicombo|Boss", meta = (ClampMin = "1"))
	int32 EvolutionTriggerHealth = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quasicombo|Boss", meta = (ClampMin = "1"))
	int32 HitsPerArmorBar = 4;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Boss")
	double FirstArmorThreshold = 0.05;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Boss")
	double SecondArmorThreshold = 0.15;
	/** Legacy serialized setting. Stage timing is now on EvolutionPresentation. */
	UPROPERTY(meta = (DeprecatedProperty))
	float EvolutionMinimumSeconds = 1.25f;
	/** Legacy serialized setting. Request deadline is on QuantumState. */
	UPROPERTY(meta = (DeprecatedProperty))
	float EvolutionTimeoutSeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quasicombo|Boss", meta = (ClampMin = "0.0"))
	float TailDamage = 4.0f;
	/** Legacy serialized delay. The retry announcement now lasts one real-time second. */
	UPROPERTY(meta = (DeprecatedProperty))
	float RetryDelaySeconds = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Quasicombo|Animation")
	TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Quasicombo|Animation")
	TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Quasicombo|Animation")
	TObjectPtr<UAnimSequence> HitReactionAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Quasicombo|Animation")
	TObjectPtr<UAnimSequence> DazedAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Quasicombo|Animation")
	TObjectPtr<UAnimSequence> TailAttackAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Quasicombo|Animation")
	TObjectPtr<UAnimSequence> DefeatAnimation;
	UPROPERTY(EditDefaultsOnly, Category = "Quasicombo|Appearance")
	TArray<TObjectPtr<UMaterialInterface>> SkinMaterials;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Music")
	TObjectPtr<USoundBase> OriginalMusic;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Music")
	TObjectPtr<USoundBase> LowQuantumJazz;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Music")
	TObjectPtr<USoundBase> MediumQuantumJazz;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Music")
	TObjectPtr<USoundBase> HighQuantumJazz;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|Music")
	TObjectPtr<USoundBase> QTEMusic;
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere,Category="Quasicombo|QTE Preview") bool bEnableQTEPreview=true;
    UPROPERTY(EditAnywhere,Category="Quasicombo|QTE Preview",meta=(ClampMin="2",ClampMax="6")) int32 PreviewQTEPrompts=2;
	/** -1 uses the real quantum result. Overrides apply only in PIE. */
	UPROPERTY(EditAnywhere, Category = "Quasicombo|PIE Testing", meta = (ClampMin = "-1", ClampMax = "2"))
	int32 PreviewArmorBars = -1;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|PIE Testing", meta = (ClampMin = "-1", ClampMax = "1"))
	double PreviewTau = -1.0;
	/** Optional starting Tau for a visible before/after evolution in PIE. */
	UPROPERTY(EditAnywhere, Category = "Quasicombo|PIE Testing", meta = (ClampMin = "-1", ClampMax = "1"))
	double PreviewPreEvolutionTau = -1.0;
	/** -1 samples once, 0 is Vacuum, 1 is Tau. */
	UPROPERTY(EditAnywhere, Category = "Quasicombo|PIE Testing", meta = (ClampMin = "-1", ClampMax = "1"))
	int32 PreviewQTEOutcome = -1;
	UPROPERTY(EditAnywhere, Category = "Quasicombo|PIE Testing", meta = (ClampMin = "1", ClampMax = "3"))
	int32 PreviewQTERounds = 1;
#endif
	UFUNCTION(BlueprintCallable, Category = "Quasicombo|Boss")
	void StartEncounter(ASideScrollingCharacter* Player);
	UFUNCTION(BlueprintCallable, Category = "Quasicombo|Boss")
	bool HandleEncounterFailure();
	UFUNCTION(BlueprintCallable, Category = "Quasicombo|Boss")
	bool SubmitQTEInput(EQuasicomboQTEInput Input);
	UFUNCTION(BlueprintCallable, Category = "Quasicombo|Boss")
	void ShowImpactCamera();
	UFUNCTION(BlueprintCallable, Category = "Quasicombo|Boss")
	void RestoreCameraAndTime(float CameraBlendSeconds = 0.25f);
	UFUNCTION(BlueprintImplementableEvent, Category = "Quasicombo|Boss")
	void OnFinisherStarted(bool bTauPattern);
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	EQuasicomboBossPhase GetBossPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	int32 GetArmorBars() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	float GetBaseHealthFraction() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	float GetArmorFraction(int32 BarIndex) const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	int32 GetRetriesRemaining() const { return FMath::Max(0, 1 - Failures); }
	/** Extra QTE rounds after the current round; zero until the finisher outcome is fixed. */
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	int32 GetAdditionalQTERoundsRemaining() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	double GetEffectiveTau() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	FString GetEncounterStatus() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	FString GetEvolutionCue() const;
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	float GetTailDamage() const { return TailDamage; }
	UFUNCTION(BlueprintPure, Category = "Quasicombo|Boss")
	float GetMeleeDamage() const { return MeleeDamage; }
	bool IsPlayerCombatLocked() const
	{
		return Phase == EQuasicomboBossPhase::Evolution || Phase == EQuasicomboBossPhase::Retry || Phase == EQuasicomboBossPhase::Complete;
	}
    void ResetAfterQTERound(bool bSucceeded);
    void FinishCinematicDefeat();
    void BeginCinematicRagdoll(bool bControlledFlight=false);
    void RestoreCinematicRagdoll();
    bool IsCinematicDefeatComplete() const { return bCinematicDefeated; }
    void SelectQTEPreview(int32 Count);
	void HandleQTEResult(bool bSucceeded);
	bool ClaimVictory();
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void DoAttackTrace(FName DamageSourceBone) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnStrikeAccepted(int32 StrikeCount) override;
	virtual bool ShouldUseChargedSideAttack() const override;
	virtual bool CanRunSideAI() const override { return !bOpeningCharge; }
	virtual float GetStrikeReactionScale() const override { return 1.0f - GetResistance(); }
	virtual bool CanProcessSideCombat() const override { return Phase == EQuasicomboBossPhase::Combat; }
	virtual bool StartCustomSideAttack(bool bCharged) override;
	virtual void ResolveAcceptedStrike(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleRequiredHitsReached(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	UFUNCTION()
	void ApplyQuantumTuning(double Vacuum, double Tau, bool bFallback);
	UFUNCTION()
	void HandleQuantumUpdate(EQuasicomboQuantumUpdate Reason, double PreviousTau, double NewTau, bool bFallback);
	UFUNCTION()
	void HandleRunEnded(EQuasicomboRunOutcome Outcome);

private:
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> ArmorPieces;
	UPROPERTY()
	TObjectPtr<UAnimMontage> HitReactionMontage;
	int32 StartingSkin = 0;
	bool bSkinCaptured = false;
	bool bEyesRevealed = false;
	bool bOpeningCharge = false;
	bool bOpeningPunch = false;
	double ChargeStartedAt = 0.0;
	float ReactionAnimationWeight = 1.0f;
	float InitialAttackCooldown = 2.0f;
	float InitialMoveSpeed = 145.0f;
	void TickOpeningCharge(float DeltaSeconds);
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> EyeMaterial;
	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicA;
	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicB;
	UPROPERTY()
	TObjectPtr<USoundBase> CurrentMusic;
	UPROPERTY()
	TObjectPtr<UAnimMontage> TailMontage;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> EvolutionGlow;
	UPROPERTY()
	TObjectPtr<AQuasicomboVictoryPickup> VictoryPickup;
	EQuasicomboBossPhase Phase = EQuasicomboBossPhase::Waiting;
	int32 BaseHitsRemaining = 10;
	int32 ArmorHitsRemaining = 0;
	int32 AwardedArmorBars = 0;
	int32 Failures = 0;
	bool bHasEvolved = false;
	bool bOutcomeSampled = false;
	bool bTauOutcome = false;
    int32 FinisherRound=1,FinalPromptCount=2;
    bool bCinematicDefeated=false;
    UPROPERTY(Transient) TObjectPtr<UPhysicsAsset> OriginalPhysicsAsset;
    UPROPERTY(Transient) TObjectPtr<UPhysicsAsset> CinematicPhysicsAsset;
    FTransform RagdollMeshRelative;
	int32 FinisherRounds = 1;
	bool bTailActive = false;
	bool bTailHit = false;
	float TailRate = 1.0f;
	TArray<FVector> PreviousTailPoints;
	bool bCameraOverride = false;
	bool bMovementFrozen = false;
	bool bEvolutionPlayerHidden = false;
	bool bPlayerWasHidden = false;
	float PreviousTimeDilation = 1.0f;
	EMovementMode PreviousMovementMode = MOVE_Walking;
	EMovementMode PreviousBossMovementMode = MOVE_Walking;
	TWeakObjectPtr<ASideScrollingCharacter> EncounterPlayer;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	FTransform PlayerStartTransform;
	FTransform BossStartTransform;
	double PhaseStartedAt = 0.0;
	float MusicElapsed = 0.0f;
	bool bUseMusicA = true;
	double RetryResumeAt = 0.0;
	bool bOwnsRetryPause = false;
	FTimerHandle VictoryPickupTimer;
	double EvolutionBeforeTau = 0.0;
	void BeginFinisher();
	void SpawnVictoryPickup();
	void RestartEncounter();
	void RestoreRetryPause();
	void UpdateAppearance();
	void UpdateHealthBar();
	void UpdateMusic();
	void FreezePlayerAndCamera(bool bFinisher);
	void FrameCamera(ACameraActor* Camera, bool bImpact);
	void UpdateTailAttack();
	void EndTailAttack(UAnimMontage* Montage, bool bInterrupted);
};
