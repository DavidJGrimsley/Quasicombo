#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "BusterCannonComponent.generated.h"

class ASideScrollingCharacter;
class UStaticMeshComponent;
class UAnimSequence;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class UParticleSystem;
class ACameraActor;
class UNiagaraComponent;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EBusterAcquisitionPhase : uint8 { None, Lift, Orbit, Equip, Land, VictoryWait, Complete };

UENUM(BlueprintType)
enum class EBusterEquipAura : uint8 { Tsunami, GalacticField, Both };

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UBusterCannonComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBusterCannonComponent();
	void InitializePlayer();
	UFUNCTION(BlueprintCallable, Category="Buster Cannon") void Equip();
	UFUNCTION(BlueprintPure, Category="Buster Cannon") bool IsEquipped() const { return bEquipped; }
	UFUNCTION(BlueprintPure, Category="Buster Cannon") bool IsFiring() const { return FireEndsAt > 0; }
	UFUNCTION(BlueprintPure, Category="Buster Cannon") bool IsHoldingFirePose() const { return FireEndsAt > 0 || HoldEndsAt > 0; }
	UFUNCTION(BlueprintCallable, Category="Buster Cannon") bool TryFire();
	void CancelFire();
	void CancelHeldPose();
	FTransform GetMuzzleWorldTransform() const;
	FVector GetShotOriginWorldLocation() const;
	bool BeginAcquisition(const FTransform& PickupTransform);
	/** Editor preview of the full pickup presentation, including when already equipped. */
	bool PreviewAcquisition(const FTransform& PickupTransform, EBusterEquipAura Aura);
	/** PIE comparisons affect new shots only; live bolts keep their selected template and scale. */
	bool PreviewTravel(bool bOriginal);
	bool PreviewTravelSize(float DiameterCm);
	bool UsesOriginalTravel() const { return bOriginalTravel; }
	void CompleteAcquisition();
	UFUNCTION(BlueprintCallable, Category="Mike|Appearance") void SetMikeLightColor(FLinearColor Color);
	UFUNCTION() void ApplyLightColor(FLinearColor Color);
	UFUNCTION() void ApplyBodyPalette();
	UPROPERTY(BlueprintReadOnly, Category="Buster Cannon") EBusterAcquisitionPhase AcquisitionPhase = EBusterAcquisitionPhase::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Buster Cannon") TObjectPtr<UStaticMeshComponent> CannonMesh;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX") FTransform MuzzleLocalTransform;
	/** Align FireR's raised barrel at release with the settled cannon-idle firing position. Actor-local centimeters. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX") FVector ShotOriginCorrectionLocal = FVector(17.0f, -15.0f, -31.0f);
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX", meta=(ClampMin="0.1")) float BusterProjectileDiameterMultiplier = 1.5f;
	/** Approximate unscaled diameter of the Thunder Ball's visible core. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX", meta=(ClampMin="1.0")) float ThunderVisualDiameterCm = 230.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Travel", meta=(ClampMin="1.0")) float TravelDiameterCm = 114.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Travel", meta=(ClampMin="0.0")) float TravelCoreIntensity = 1.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Travel", meta=(ClampMin="0.0")) float TravelSparkIntensity = 1.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX", meta=(ClampMin="0.01")) float MuzzleEffectScaleMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX") float MuzzleEffectInsetCm = 4.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX", meta=(ClampMin="0.01")) float ImpactEffectScaleMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX") TSoftObjectPtr<UParticleSystem> MuzzleEffect;
	/** Minimum time between projectile releases. Each button press still fires at most once. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Fire Rate", meta=(ClampMin="0.08", UIMin="0.08", UIMax="0.4")) float ShotInterval = 0.16f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon", meta=(ClampMin="0.0", ClampMax="0.4")) float FireWindupSeconds = 0.22f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon") float ProjectileSpeed = 1500;
	UPROPERTY(EditAnywhere, Category="Buster Cannon") float ProjectileLifetime = 2;
	UPROPERTY(EditAnywhere, Category="Buster Cannon", meta=(ClampMin="0.0", ClampMax="1.0")) float AirFireFallFactor = 0.5f;
	/** Assign the supplied firing cue here later; a valid shot invokes it even on a miss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Buster Cannon|Audio") TObjectPtr<USoundBase> FireSound = nullptr;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float LiftHeight = 165;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float LiftSeconds = 0.85f;
	/** Total shared lift-and-orbit time; Mike and the cannon reach their apex together. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float OrbitSeconds = 3.6f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition", meta=(ClampMin="1")) int32 OrbitTurns = 2;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float OrbitEndVerticalOffset = 20;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float OrbitRadius = 140;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float OrbitMeetRadius = 65;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float LandSeconds = 0.6f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float AttachmentFraction = 0.55f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") FVector CameraOffset = FVector(180, 380, 120);
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition") float CameraFOV = 42;
	/** Keep the opening wide view long enough to see the ground before the camera moves in. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition", meta=(ClampMin="0.0")) float CameraZoomDelaySeconds = 0.65f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation") TSoftObjectPtr<UAnimSequence> OpeningAnimation;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation") TSoftObjectPtr<UAnimSequence> EquipAnimation;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation") TSoftObjectPtr<UAnimSequence> CalibrationAnimation;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX") EBusterEquipAura EquipAuraVariant = EBusterEquipAura::Tsunami;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX") TSoftObjectPtr<class UNiagaraSystem> TsunamiEquipAura;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX") TSoftObjectPtr<class UNiagaraSystem> GalacticEquipAura;
	/** One-shot water burst when the cannon actually attaches; Galactic Field has no matching burst asset. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX") TSoftObjectPtr<UNiagaraSystem> TsunamiEquipBurst;
	/** Offset above the bottom of Mike's capsule for the aura and equip burst. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX") float EquipVFXFootOffsetCm = 5.0f;
	/** Approximate starting height of the aura used to stretch it between ground and Mike during lift. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX", meta=(ClampMin="1.0")) float EquipAuraBaseHeightCm = 65.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX", meta=(ClampMin="0.01")) float TsunamiEquipAuraScale = 1.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX", meta=(ClampMin="0.01")) float GalacticEquipAuraScale = 1.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Acquisition|VFX", meta=(ClampMin="0.01")) float TsunamiEquipBurstScale = 1.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation") TSoftObjectPtr<UAnimSequence> FireAnimation;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation") TSoftObjectPtr<UAnimSequence> FireIdleAnimation;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation", meta=(ClampMin="0.0")) float FireIdleHoldSeconds = 0.5f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation", meta=(ClampMin="0.0")) float CannonRecoilCm = 4.0f;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation", meta=(ClampMin="0.01")) float CannonRecoilSeconds = 0.18f;
	/** Transition into cannon idle before the supplied fire clip lowers Mike's arm. */
	UPROPERTY(EditAnywhere, Category="Buster Cannon|Animation", meta=(ClampMin="0.1", ClampMax="1.0")) float FireAnimationHoldFraction = 0.75f;
	float GetRecoilAlpha() const;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
protected:
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	TWeakObjectPtr<ASideScrollingCharacter> Player;
	TWeakObjectPtr<AActor> PreviousViewTarget;
	UPROPERTY() TObjectPtr<ACameraActor> CinematicCamera;
	UPROPERTY() TObjectPtr<UNiagaraComponent> OrbitTrail;
	UPROPERTY() TObjectPtr<UNiagaraComponent> ActiveEquipAura;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> EquipAuraMaterials;
	TArray<bool> EquipAuraMaterialIsCore;
	UPROPERTY() TObjectPtr<UNiagaraComponent> SecondaryEquipAura;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SecondaryEquipAuraMaterials;
	TArray<bool> SecondaryEquipAuraMaterialIsCore;
	UPROPERTY() TObjectPtr<UNiagaraComponent> ActiveEquipBurst;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> EquipBurstMaterials;
	TArray<bool> EquipBurstMaterialIsCore;
	UPROPERTY() TObjectPtr<UAnimMontage> HoldMontage;
	UPROPERTY() TObjectPtr<UAnimMontage> FireMontage;
	UPROPERTY() TObjectPtr<UAnimMontage> OpeningMontage;
	UPROPERTY() TObjectPtr<UAnimMontage> EquipMontage;
	UPROPERTY() TObjectPtr<UAnimMontage> CalibrationMontage;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> LightMaterials;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> DirectLightMaterials;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> LensLightMaterials;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMaterials;
	UPROPERTY() TArray<TObjectPtr<UUserWidget>> HiddenWidgets;
	TArray<ESlateVisibility> WidgetVisibility;
	TArray<TWeakObjectPtr<class UWidgetComponent>> HiddenWorldWidgets;
	bool bEquipped = false;
	bool bOriginalTravel = false;
	void DescribeTravelPreview() const;
	bool bInitialized = false;
	bool bAcquiring = false;
	bool bAirFire = false;
	float SavedGravity = 1.75f;
	float SavedVerticalVelocity = 0;
	float FireEndsAt = 0;
	float HoldEndsAt = 0;
	float PendingShotAt = 0;
	float NextShotAt = 0;
	float RecoilStartedAt = -1;
	bool bQueuedFollowup = false;
	float PhaseAge = 0;
	float TotalAge = 0;
	float EquipDuration = 1;
	float VictoryWaitDuration = 1.0f;
	float AcquisitionTimeout = 15.0f;
	float OrbitStartAngle = 0;
	FVector GroundPosition;
	FVector LiftPosition;
	FVector PickupPosition;
	FVector OrbitEndPosition;
	FVector CameraStartPosition;
	FRotator CameraStartRotation;
	float CameraStartFOV = 65.0f;
	FLinearColor LightColor = FLinearColor(0.175f, 0.367f, 1);
	void SetPhase(EBusterAcquisitionPhase Phase);
	bool IsAnimationCompatibleWithMike(const UAnimSequence* Sequence, const TCHAR* AnimationName) const;
	void StopAcquisitionMontages(float BlendOutTime);
	void ExtendAcquisitionTimeout(float ExpectedRemainingDuration);
	void RestorePresentation();
	void StartEquipAura();
	void UpdateEquipAuraTransform();
	void StopEquipAura();
	void StartEquipBurst();
	void StopEquipBurst();
	void BindEquipVFXMaterials(UNiagaraSystem* System, UNiagaraComponent* Component,
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, TArray<bool>& MaterialIsCore);
	void UpdateEquipAuraColors();
	void Pulse(FVector Location, float Radius);
	void ReleaseShot();
	void FinishFire();
};
