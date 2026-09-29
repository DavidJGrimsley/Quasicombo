#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SideScrollingCombatEnemy.h"
#include "QuantumBossComponent.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboBoss.generated.h"

class ACameraActor;
class ASideScrollingCharacter;
class UAudioComponent;
class USoundBase;
class UMaterialInstanceDynamic;
class UAnimSequence;
class UPointLightComponent;
class AQuasicomboVictoryPickup;

UENUM(BlueprintType)
enum class EQuasicomboBossPhase : uint8 { Waiting, Combat, Evolution, QTE, Retry, AwaitPickup, Complete };

UCLASS(Blueprintable)
class AQuasicomboBoss : public ASideScrollingCombatEnemy
{
	GENERATED_BODY()
public:
	AQuasicomboBoss();
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Boss") TObjectPtr<UQuantumBossComponent> Quantum;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Boss") TObjectPtr<UQuasicomboQTEComponent> QTE;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Camera") TObjectPtr<ACameraActor> CloseupCamera;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Camera") TObjectPtr<ACameraActor> ImpactCamera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Camera", meta=(ClampMin="0.1", ClampMax="1.0")) float FinisherTimeDilation = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Boss", meta=(ClampMin="4")) int32 BaseHealthHits = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Boss", meta=(ClampMin="1")) int32 HitsPerArmorBar = 4;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Boss") double FirstArmorThreshold = 0.05;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Boss") double SecondArmorThreshold = 0.15;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Boss") float EvolutionMinimumSeconds = 1.25f;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Boss") float EvolutionTimeoutSeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Boss", meta=(ClampMin="0.0")) float TailDamage = 4.0f;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Boss") float RetryDelaySeconds = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Animation") TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Animation") TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Animation") TObjectPtr<UAnimSequence> HitReactionAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Animation") TObjectPtr<UAnimSequence> DazedAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Animation") TObjectPtr<UAnimSequence> TailAttackAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Animation") TObjectPtr<UAnimSequence> DefeatAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Appearance") TArray<TObjectPtr<UMaterialInterface>> SkinMaterials;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Music") TObjectPtr<USoundBase> OriginalMusic;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Music") TObjectPtr<USoundBase> LowQuantumJazz;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Music") TObjectPtr<USoundBase> MediumQuantumJazz;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Music") TObjectPtr<USoundBase> HighQuantumJazz;
	UPROPERTY(EditAnywhere, Category="Quasicombo|Music") TObjectPtr<USoundBase> QTEMusic;
#if WITH_EDITORONLY_DATA
	/** -1 uses the real quantum result. Overrides apply only in PIE. */
	UPROPERTY(EditAnywhere, Category="Quasicombo|PIE Testing", meta=(ClampMin="-1", ClampMax="2")) int32 PreviewArmorBars = -1;
	UPROPERTY(EditAnywhere, Category="Quasicombo|PIE Testing", meta=(ClampMin="-1", ClampMax="1")) double PreviewTau = -1.0;
	/** Optional starting Tau for a visible before/after evolution in PIE. */
	UPROPERTY(EditAnywhere, Category="Quasicombo|PIE Testing", meta=(ClampMin="-1", ClampMax="1")) double PreviewPreEvolutionTau = -1.0;
	/** -1 samples once, 0 is Vacuum, 1 is Tau. */
	UPROPERTY(EditAnywhere, Category="Quasicombo|PIE Testing", meta=(ClampMin="-1", ClampMax="1")) int32 PreviewQTEOutcome = -1;
	UPROPERTY(EditAnywhere, Category="Quasicombo|PIE Testing", meta=(ClampMin="-1", ClampMax="3")) int32 PreviewQTERounds = -1;
#endif
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") void StartEncounter(ASideScrollingCharacter* Player);
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") bool HandleEncounterFailure();
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") bool SubmitQTEInput(EQuasicomboQTEInput Input);
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") void ShowImpactCamera();
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Boss") void RestoreCameraAndTime();
	UFUNCTION(BlueprintImplementableEvent, Category="Quasicombo|Boss") void OnFinisherStarted(bool bTauPattern);
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") EQuasicomboBossPhase GetBossPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") int32 GetArmorBars() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") float GetBaseHealthFraction() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") float GetArmorFraction(int32 BarIndex) const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") int32 GetRetriesRemaining() const { return FMath::Max(0, 1 - Failures); }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") double GetEffectiveTau() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") FString GetEncounterStatus() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") FString GetEvolutionCue() const;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") float GetTailDamage() const { return TailDamage; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Boss") float GetMeleeDamage() const { return MeleeDamage; }
	bool IsPlayerCombatLocked() const { return Phase == EQuasicomboBossPhase::Evolution || Phase == EQuasicomboBossPhase::Retry || Phase == EQuasicomboBossPhase::Complete; }
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
	virtual bool CanProcessSideCombat() const override { return Phase == EQuasicomboBossPhase::Combat; }
	virtual bool StartCustomSideAttack(bool bCharged) override;
	virtual void ResolveAcceptedStrike(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleRequiredHitsReached(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	UFUNCTION() void ApplyQuantumTuning(double Vacuum, double Tau, bool bFallback);
	UFUNCTION() void HandleQuantumUpdate(EQuasicomboQuantumUpdate Reason, double PreviousTau, double NewTau, bool bFallback);
	UFUNCTION() void HandleRunEnded(EQuasicomboRunOutcome Outcome);
private:
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> ArmorPieces;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> EyeMaterial;
	UPROPERTY() TObjectPtr<UAudioComponent> MusicA;
	UPROPERTY() TObjectPtr<UAudioComponent> MusicB;
	UPROPERTY() TObjectPtr<USoundBase> CurrentMusic;
	UPROPERTY() TObjectPtr<UAnimMontage> TailMontage;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> EvolutionGlow;
	UPROPERTY() TObjectPtr<AQuasicomboVictoryPickup> VictoryPickup;
	EQuasicomboBossPhase Phase = EQuasicomboBossPhase::Waiting;
	int32 BaseHitsRemaining = 4;
	int32 ArmorHitsRemaining = 0;
	int32 AwardedArmorBars = 0;
	int32 Failures = 0;
	bool bHasEvolved = false;
	bool bOutcomeSampled = false;
	bool bTauOutcome = false;
	int32 FinisherRounds = 1;
	bool bTailActive = false;
	bool bTailHit = false;
	bool bCorpseSettling = false;
	float CorpseFloorZ = 0.0f;
	double CorpseSettleStartedAt = 0.0;
	float TailRate = 1.0f;
	TArray<FVector> PreviousTailPoints;
	bool bCameraOverride = false;
	bool bMovementFrozen = false;
	float PreviousTimeDilation = 1.0f;
	EMovementMode PreviousMovementMode = MOVE_Walking;
	TWeakObjectPtr<ASideScrollingCharacter> EncounterPlayer;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	FTransform PlayerStartTransform;
	FTransform BossStartTransform;
	double PhaseStartedAt = 0.0;
	float MusicElapsed = 0.0f;
	bool bUseMusicA = true;
	FTimerHandle RetryTimer;
	FTimerHandle VictoryPickupTimer;
	double EvolutionBeforeTau = 0.0;
	double EvolutionAfterTau = 0.0;
	float EvolutionResultUntil = 0.0f;
	float EvolutionDeadlineSeconds = 3.0f;
	bool bEvolutionUsedFallback = false;
	void BeginFinisher();
	void FinishEvolution();
	void SpawnVictoryPickup();
	void UpdateCorpseSettle();
	void CorrectVisualGrounding(float DeltaSeconds);
	void RestartEncounter();
	void UpdateAppearance();
	void UpdateHealthBar();
	void UpdateMusic();
	void FreezePlayerAndCamera(bool bFinisher);
	void FrameCamera(ACameraActor* Camera, bool bImpact);
	void UpdateTailAttack();
	void EndTailAttack(UAnimMontage* Montage, bool bInterrupted);
};

