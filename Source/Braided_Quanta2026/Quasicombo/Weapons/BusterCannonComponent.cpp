#include "BusterCannonComponent.h"
#include "BusterProjectile.h"
#include "QuasicomboLoadoutSubsystem.h"
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboBoss.h"
#include "SideScrollingCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraParameterStore.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

UBusterCannonComponent::UBusterCannonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	MuzzleLocalTransform = FTransform(FRotator::ZeroRotator, FVector(0, 42.8f, 0));
	MuzzleEffect = TSoftObjectPtr<UParticleSystem>(FSoftObjectPath(TEXT("/Game/Quasicombo/Buster/VFX/P_BusterThunderMuzzle.P_BusterThunderMuzzle")));
	OpeningAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_IdleBreaker.Anim_ZMIKE_IdleBreaker")));
	EquipAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_BusterEquip_R.Anim_ZMIKE_BusterEquip_R")));
	CalibrationAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_CalibrationStretches.Anim_ZMIKE_CalibrationStretches")));
	TsunamiEquipAura = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Quasicombo/Buster/VFX/N_BusterEquipTsunami.N_BusterEquipTsunami")));
	GalacticEquipAura = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Quasicombo/Buster/VFX/N_BusterEquipGalacticField.N_BusterEquipGalacticField")));
	TsunamiEquipBurst = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Quasicombo/Buster/VFX/N_BusterEquipTsunamiBurst.N_BusterEquipTsunamiBurst")));
	FireAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_FireR.Anim_ZMIKE_FireR")));
	FireIdleAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_BusterIdle_R.Anim_ZMIKE_BusterIdle_R")));
}

void UBusterCannonComponent::InitializePlayer()
{
	if (bInitialized) return;
	Player = Cast<ASideScrollingCharacter>(GetOwner());
	if (!Player.IsValid()) return;
	bInitialized = true;
#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::PIE)
	{
		bOriginalTravel = false;
		TravelDiameterCm = 114.f;
	}
#endif
	CannonMesh = NewObject<UStaticMeshComponent>(Player.Get(), TEXT("BusterCannonMesh"));
	Player->AddInstanceComponent(CannonMesh);
	CannonMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/RadicalMike/Mesh/SM_RadicalBuster.SM_RadicalBuster")));
	CannonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CannonMesh->SetupAttachment(Player->GetMesh(), TEXT("Buster_RSocket"));
	// The asset hides its built-in Buster_R bone by scaling it to ~0.135 during
	// locomotion. The separate cannon mesh must follow the socket without scale.
	CannonMesh->SetAbsolute(false, false, true);
	CannonMesh->RegisterComponent();
	CannonMesh->SetVisibility(false);
	for (UMeshComponent* Mesh : {static_cast<UMeshComponent*>(Player->GetMesh()), static_cast<UMeshComponent*>(CannonMesh.Get())})
	{
		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			// The imported master hue-shifts its emissive texture after the color picker.
			// Use project-owned instances whose emissive output is grayscale before tinting.
			if (Index == 0)
			{
				const TCHAR* Path = Mesh == Player->GetMesh()
					? TEXT("/Game/Quasicombo/Buster/Materials/MI_RadicalMikePlayer.MI_RadicalMikePlayer")
					: TEXT("/Game/Quasicombo/Buster/Materials/MI_RadicalMikeCannon.MI_RadicalMikeCannon");
				if (UMaterialInterface* Corrected = LoadObject<UMaterialInterface>(nullptr, Path)) Mesh->SetMaterial(Index, Corrected);
			}
			UMaterialInterface* Base = Mesh->GetMaterial(Index);
			if (!Base) continue;
			FLinearColor Existing;
			const bool bHasDirectLight = Base->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BusterLightColor")), Existing);
			const bool bHasLight = Base->GetVectorParameterValue(FMaterialParameterInfo(TEXT("EmissiveColorPicker")), Existing);
			const bool bHasLensLight = Base->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorPicker Emiss")), Existing);
			const bool bBodySlot = Mesh == Player->GetMesh() ? Index == 0 : true;
			const bool bHasBody = bBodySlot && Base->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color G")), Existing);
			if (!bHasDirectLight && !bHasLight && !bHasLensLight && !bHasBody) continue;
			if (auto* MID = Mesh->CreateDynamicMaterialInstance(Index))
			{
				if (bHasDirectLight) DirectLightMaterials.Add(MID);
				else if (bHasLight) LightMaterials.Add(MID);
				if (bHasLensLight) LensLightMaterials.Add(MID);
				if (bHasBody) BodyMaterials.Add(MID);
			}
		}
	}
	if (UWorld* World = GetWorld()) if (auto* GI = World->GetGameInstance())
		if (auto* State = GI->GetSubsystem<UQuasicomboLoadoutSubsystem>())
		{
			State->OnMikeLightColorChanged.AddDynamic(this, &UBusterCannonComponent::ApplyLightColor);
			State->OnMikeBodyPaletteChanged.AddDynamic(this, &UBusterCannonComponent::ApplyBodyPalette);
			ApplyLightColor(State->GetMikeLightColor());
			ApplyBodyPalette();
			if (State->HasReplayCannon()) Equip();
			State->NotifyPlayerReady();
		}
	ApplyLightColor(LightColor);
}

void UBusterCannonComponent::Equip()
{
	InitializePlayer();
	if (!CannonMesh || !Player.IsValid()) return;
	bEquipped = true;
	CannonMesh->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Buster_RSocket"));
	CannonMesh->SetAbsolute(false, false, true);
	CannonMesh->SetRelativeScale3D(FVector::OneVector);
	CannonMesh->SetVisibility(true);
}

void UBusterCannonComponent::SetMikeLightColor(FLinearColor Color)
{
	if (UWorld* World = GetWorld()) if (auto* GI = World->GetGameInstance())
		if (auto* State = GI->GetSubsystem<UQuasicomboLoadoutSubsystem>()) { State->SetMikeLightColor(Color); return; }
	ApplyLightColor(Color);
}
void UBusterCannonComponent::ApplyLightColor(FLinearColor Color)
{
	LightColor = Color;
	for (UMaterialInstanceDynamic* Material : DirectLightMaterials) Material->SetVectorParameterValue(TEXT("BusterLightColor"), Color);
	for (UMaterialInstanceDynamic* Material : LightMaterials) Material->SetVectorParameterValue(TEXT("EmissiveColorPicker"), Color);
	for (UMaterialInstanceDynamic* Material : LensLightMaterials) Material->SetVectorParameterValue(TEXT("ColorPicker Emiss"), Color);
	if (Player.IsValid()) Player->ApplyMikeTrailColor(Color);
	if (OrbitTrail) OrbitTrail->SetVariableLinearColor(TEXT("User.Color"), Color);
	UpdateEquipAuraColors();
}

void UBusterCannonComponent::ApplyBodyPalette()
{
	const auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* State = GI ? GI->GetSubsystem<UQuasicomboLoadoutSubsystem>() : nullptr;
	if (!State) return;
	UpdateEquipAuraColors();
	if (!State->HasCustomMikeBodyPalette()) return;
	for (UMaterialInstanceDynamic* Material : BodyMaterials)
	{
		Material->SetVectorParameterValue(TEXT("Color R"), State->GetMikeBodyRegionColor(EMikeBodyRegion::ColorR));
		Material->SetVectorParameterValue(TEXT("Color G"), State->GetMikeBodyRegionColor(EMikeBodyRegion::ColorG));
		Material->SetVectorParameterValue(TEXT("Color B"), State->GetMikeBodyRegionColor(EMikeBodyRegion::ColorB));
	}
}

bool UBusterCannonComponent::TryFire()
{
	if (!Player.IsValid() || !bEquipped || bAcquiring || !Player->CanFireBuster()) return false;
	const auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	if (Run && (Run->GetOutcome() != EQuasicomboRunOutcome::Playing || Run->IsBossCombatLocked())) return false;
	const float Now = GetWorld()->GetTimeSeconds();
	if (PendingShotAt > 0) return false;
	if (Now < NextShotAt)
	{
		if (HoldEndsAt <= Now || bQueuedFollowup) return false;
		bQueuedFollowup = true;
		return true;
	}
	const bool bRapidFollowup = HoldEndsAt > Now;
	if (!bRapidFollowup) CancelFire();
	else { FireEndsAt = 0; PendingShotAt = 0; RecoilStartedAt = -1; CannonMesh->SetRelativeLocation(FVector::ZeroVector); }
	bQueuedFollowup = false;
	if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
		if (UAnimSequence* Fire = FireAnimation.LoadSynchronous())
		{
			FireMontage = Anim->PlaySlotAnimationAsDynamicMontage(Fire, TEXT("BusterArm"),
				bRapidFollowup ? 0.06f : 0.12f, 0.12f);
			HoldMontage = nullptr;
		}
	const float Windup = FMath::Max(0.f, FireWindupSeconds);
	const UAnimSequence* Fire = FireAnimation.Get();
	FireEndsAt = Now + FMath::Max(Windup + CannonRecoilSeconds,
		Fire ? Fire->GetPlayLength() * FMath::Clamp(FireAnimationHoldFraction, 0.1f, 1.f) : Windup + CannonRecoilSeconds);
	PendingShotAt = Now + Windup;
	auto* Movement = Player->GetCharacterMovement();
	if (Movement->IsFalling())
	{
		bAirFire = true;
		SavedGravity = Movement->GravityScale;
		SavedVerticalVelocity = Movement->Velocity.Z;
		const float FallFactor = FMath::Clamp(AirFireFallFactor, 0.f, 1.f);
		Movement->GravityScale = SavedGravity * FallFactor;
		Movement->Velocity.Z = SavedVerticalVelocity * FallFactor;
	}
	if (PendingShotAt <= Now) { PendingShotAt = 0; ReleaseShot(); }
	return true;
}

FTransform UBusterCannonComponent::GetMuzzleWorldTransform() const
{
	return CannonMesh ? MuzzleLocalTransform * CannonMesh->GetComponentTransform() : FTransform::Identity;
}

FVector UBusterCannonComponent::GetShotOriginWorldLocation() const
{
	const FVector MuzzleLocation = GetMuzzleWorldTransform().GetLocation();
	if (!Player.IsValid()) return MuzzleLocation;
	// An unevaluated or interrupted montage leaves the barrel down. In that case,
	// shifting toward the normal FireR idle position would put the shot under Mike.
	const FTransform PlayerTransform = Player->GetActorTransform();
	const FVector LocalMuzzle = PlayerTransform.InverseTransformPosition(MuzzleLocation);
	return LocalMuzzle.Z > 0.0f
		? MuzzleLocation + PlayerTransform.TransformVectorNoScale(ShotOriginCorrectionLocal)
		: MuzzleLocation;
}

bool UBusterCannonComponent::PreviewTravel(bool bOriginal)
{
#if WITH_EDITOR
	if (!GetWorld() || !GetWorld()->IsGameWorld()) return false;
	bOriginalTravel = bOriginal;
	DescribeTravelPreview();
	return true;
#else
	return false;
#endif
}

bool UBusterCannonComponent::PreviewTravelSize(float DiameterCm)
{
#if WITH_EDITOR
	if (!GetWorld() || !GetWorld()->IsGameWorld() || (!FMath::IsFinite(DiameterCm) || DiameterCm < 1.f || DiameterCm > 1000.f)) return false;
	TravelDiameterCm = DiameterCm;
	DescribeTravelPreview();
	return true;
#else
	return false;
#endif
}

void UBusterCannonComponent::DescribeTravelPreview() const
{
#if WITH_EDITOR
	const FString Message = FString::Printf(TEXT("Thunder travel: %s / %.0f cm (new shots)"),
		bOriginalTravel ? TEXT("original source colors") : TEXT("custom Mike colors"), TravelDiameterCm);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	if (GEngine) GEngine->AddOnScreenDebugMessage(9586, 3.f, FColor::Yellow, Message);
#endif
}

void UBusterCannonComponent::ReleaseShot()
{
	if (!Player.IsValid() || !bEquipped || bAcquiring || !Player->CanFireBuster()) return;
	const auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	if (Run && (Run->GetOutcome() != EQuasicomboRunOutcome::Playing || Run->IsBossCombatLocked())) return;
	const FVector Direction(Player->GetActorForwardVector().X >= 0 ? 1 : -1, 0, 0);
	// Clear any previous mechanical recoil. FireR has already lifted the barrel
	// by the release frame, so fire from its measured cannon-idle position.
	RecoilStartedAt = -1;
	CannonMesh->SetRelativeLocation(FVector::ZeroVector);
	const FTransform Muzzle = GetMuzzleWorldTransform();
	const FVector VisibleOrigin = GetShotOriginWorldLocation();
	FVector CollisionOrigin = VisibleOrigin;
	// Collision remains in the side-scroller lane, while VFX stays at the barrel.
	CollisionOrigin.Y = Player->GetActorLocation().Y;
	const FVector VisualOffset = VisibleOrigin - CollisionOrigin;
	const float MuzzleDiameter = 37.642f * CannonMesh->GetComponentScale().GetAbsMax();
	// Retain the approved explosion sizes; only travel follows the comparison diameter.
	const float EffectScale = MuzzleDiameter * BusterProjectileDiameterMultiplier / FMath::Max(1.0f, ThunderVisualDiameterCm);
	const float TravelScale = TravelDiameterCm / FMath::Max(1.0f, ThunderVisualDiameterCm);
	FActorSpawnParameters Params;
	Params.Owner = Player.Get();
	Params.Instigator = Player.Get();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABusterProjectile* Bolt = GetWorld()->SpawnActor<ABusterProjectile>(CollisionOrigin, Direction.Rotation(), Params);
	if (!Bolt) return;
	Bolt->Initialize(Player.Get(), Direction, Player->GetBusterDamage(), ProjectileSpeed, ProjectileLifetime,
		VisualOffset, TravelScale, EffectScale * ImpactEffectScaleMultiplier,
		bOriginalTravel, TravelCoreIntensity, TravelSparkIntensity);
	if (FireSound) UGameplayStatics::PlaySoundAtLocation(this, FireSound, VisibleOrigin);
	if (UParticleSystem* Effect = MuzzleEffect.LoadSynchronous())
		if (UParticleSystemComponent* Flash = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Effect,
			FTransform(Direction.Rotation(), VisibleOrigin - Muzzle.GetUnitAxis(EAxis::Y) * MuzzleEffectInsetCm,
				FVector(EffectScale * MuzzleEffectScaleMultiplier)),
			true, EPSCPoolMethod::None, false))
		{
			Flash->SetColorParameter(TEXT("BusterCoreColor"), LightColor);
			if (auto* GI = GetWorld()->GetGameInstance())
				if (auto* State = GI->GetSubsystem<UQuasicomboLoadoutSubsystem>())
					Flash->SetColorParameter(TEXT("BusterSparkColor"), State->GetMikeSparkColor());
			Flash->Activate(true);
		}
	RecoilStartedAt = GetWorld()->GetTimeSeconds();
	NextShotAt = RecoilStartedAt + FMath::Max(0.08f, ShotInterval);
	if (FireIdleHoldSeconds > 0 && Player->GetCharacterMovement()->IsMovingOnGround() &&
		Player->GetCharacterMovement()->Velocity.Size2D() <= 5.0f)
		HoldEndsAt = RecoilStartedAt + FireIdleHoldSeconds;
	// Check the entire character-to-muzzle segment: a muzzle through a wall cannot shoot through it.
	FHitResult Block;
	if (ABusterProjectile::SweepForImpact(GetWorld(), Player->GetActorLocation(), CollisionOrigin,
		Bolt->CollisionRadius, Player.Get(), Bolt, Block))
		Bolt->ResolveImpact(Block);
}

void UBusterCannonComponent::CancelFire()
{
	CancelHeldPose();
	if (Player.IsValid())
	{
		if (bAirFire)
		{
			auto* Movement = Player->GetCharacterMovement();
			Movement->GravityScale = SavedGravity;
			if (Movement->IsFalling()) Movement->Velocity.Z = SavedVerticalVelocity;
		}
	}
	bAirFire = false;
	bQueuedFollowup = false;
	RecoilStartedAt = -1;
	if (CannonMesh) CannonMesh->SetRelativeLocation(FVector::ZeroVector);
	if (FireMontage && Player.IsValid())
		if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.08f, FireMontage);
	FireMontage = nullptr;
	FireEndsAt = 0;
	PendingShotAt = 0;
}

void UBusterCannonComponent::CancelHeldPose()
{
	HoldEndsAt = 0;
	bQueuedFollowup = false;
	if (HoldMontage && Player.IsValid())
		if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.08f, HoldMontage);
	HoldMontage = nullptr;
}

float UBusterCannonComponent::GetRecoilAlpha() const
{
	if (RecoilStartedAt < 0 || !GetWorld()) return 0.f;
	const float Progress = FMath::Clamp((GetWorld()->GetTimeSeconds() - RecoilStartedAt) /
		FMath::Max(0.01f, CannonRecoilSeconds), 0.f, 1.f);
	return FMath::Sin(PI * Progress);
}

void UBusterCannonComponent::FinishFire()
{
	FireEndsAt = 0;
	if (bAirFire && Player.IsValid())
	{
		auto* Movement = Player->GetCharacterMovement();
		Movement->GravityScale = SavedGravity;
		if (Movement->IsFalling()) Movement->Velocity.Z = SavedVerticalVelocity;
		bAirFire = false;
	}
	if (HoldEndsAt > GetWorld()->GetTimeSeconds() && Player.IsValid())
		if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
			if (UAnimSequence* Idle = FireIdleAnimation.LoadSynchronous())
				HoldMontage = Anim->PlaySlotAnimationAsDynamicMontage(Idle, TEXT("BusterArm"), 0.12f, 0.08f, 1.0f, 10);
	FireMontage = nullptr;
	if (HoldEndsAt <= GetWorld()->GetTimeSeconds()) CancelHeldPose();
}

void UBusterCannonComponent::Pulse(FVector Location, float Radius)
{
	if (auto* Effect = GetWorld()->SpawnActor<ABusterEnergyPulse>(Location, FRotator::ZeroRotator)) Effect->Initialize(LightColor, Radius);
}

bool UBusterCannonComponent::BeginAcquisition(const FTransform& PickupTransform)
{
	InitializePlayer();
	if (!Player.IsValid() || bAcquiring || AcquisitionPhase != EBusterAcquisitionPhase::None) return false;
	CancelFire();
	Player->PrepareForFinisher();
	bAcquiring = true;
	TotalAge = 0;
	AcquisitionTimeout = 15.0f;
	EquipDuration = 1.0f;
	VictoryWaitDuration = 1.0f;
	GroundPosition = Player->GetActorLocation();
	FHitResult Floor;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BusterLanding), false, Player.Get());
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (GetWorld()->LineTraceSingleByChannel(Floor, GroundPosition, GroundPosition - FVector(0,0,1000), ECC_Visibility, Query)
		&& Floor.ImpactNormal.Z > 0.6f) GroundPosition.Z = Floor.ImpactPoint.Z + HalfHeight + 2;
	Player->GetCharacterMovement()->DisableMovement();
	Player->SetActorLocation(GroundPosition, true);
	GroundPosition = Player->GetActorLocation();
	LiftPosition = GroundPosition + FVector(0,0,LiftHeight);
	// Clamp lift against the ceiling using the player's capsule.
	FHitResult Ceiling;
	if (GetWorld()->SweepSingleByChannel(Ceiling, GroundPosition, LiftPosition, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeCapsule(Player->GetCapsuleComponent()->GetScaledCapsuleRadius(), HalfHeight), Query))
		LiftPosition = FMath::Lerp(GroundPosition, LiftPosition, FMath::Max(0.f, Ceiling.Time - 0.02f));
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
	for (auto* Widget : Widgets) { HiddenWidgets.Add(Widget); WidgetVisibility.Add(Widget->GetVisibility()); Widget->SetVisibility(ESlateVisibility::Hidden); }
	TArray<UWidgetComponent*> WorldWidgets;
	Player->GetComponents(WorldWidgets);
	for (auto* Widget : WorldWidgets) if (!Widget->bHiddenInGame) { HiddenWorldWidgets.Add(Widget); Widget->SetHiddenInGame(true); }
	if (bEquipped) { SetPhase(EBusterAcquisitionPhase::VictoryWait); return true; }
	PickupPosition = PickupTransform.GetLocation();
	CannonMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	CannonMesh->SetWorldTransform(PickupTransform);
	CannonMesh->SetVisibility(true);
	OrbitStartAngle = FMath::Atan2(PickupPosition.Y - GroundPosition.Y, PickupPosition.X - GroundPosition.X);
	if (auto* PC = Cast<APlayerController>(Player->GetController()))
	{
		PreviousViewTarget = PC->GetViewTarget();
		CinematicCamera = GetWorld()->SpawnActor<ACameraActor>();
		if (CinematicCamera)
		{
			CameraStartPosition = PC->PlayerCameraManager->GetCameraLocation();
			CameraStartRotation = PC->PlayerCameraManager->GetCameraRotation();
			CameraStartFOV = PC->PlayerCameraManager->GetFOVAngle();
			CinematicCamera->SetActorLocation(CameraStartPosition);
			CinematicCamera->SetActorRotation(CameraStartRotation);
			CinematicCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
			CinematicCamera->GetCameraComponent()->SetFieldOfView(CameraStartFOV);
			PC->SetViewTargetWithBlend(CinematicCamera, 0.2f);
		}
	}
	SetPhase(EBusterAcquisitionPhase::Lift);
	return true;
}

bool UBusterCannonComponent::PreviewAcquisition(const FTransform& PickupTransform, EBusterEquipAura Aura)
{
	if (bAcquiring || !GetWorld() || !GetWorld()->IsGameWorld()) return false;
	InitializePlayer();
	const bool bWasEquipped = bEquipped;
	const EBusterAcquisitionPhase PreviousPhase = AcquisitionPhase;
	const EBusterEquipAura PreviousAura = EquipAuraVariant;
	bEquipped = false;
	AcquisitionPhase = EBusterAcquisitionPhase::None;
	EquipAuraVariant = Aura;
	if (BeginAcquisition(PickupTransform)) return true;
	bEquipped = bWasEquipped;
	AcquisitionPhase = PreviousPhase;
	EquipAuraVariant = PreviousAura;
	return false;
}

void UBusterCannonComponent::UpdateEquipAuraColors()
{
	const auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* State = GI ? GI->GetSubsystem<UQuasicomboLoadoutSubsystem>() : nullptr;
	const FLinearColor SparkColor = State ? State->GetMikeSparkColor() : FLinearColor(0.175f, 0.367f, 1.0f);
	const auto ApplyColors = [this, &SparkColor](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, const TArray<bool>& MaterialIsCore)
	{
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			if (!Materials[Index]) continue;
			const bool bCore = MaterialIsCore.IsValidIndex(Index) && MaterialIsCore[Index];
			Materials[Index]->SetVectorParameterValue(TEXT("BusterAuraTint"), bCore ? LightColor : SparkColor);
		}
	};
	ApplyColors(EquipAuraMaterials, EquipAuraMaterialIsCore);
	ApplyColors(SecondaryEquipAuraMaterials, SecondaryEquipAuraMaterialIsCore);
	ApplyColors(EquipBurstMaterials, EquipBurstMaterialIsCore);
}

void UBusterCannonComponent::BindEquipVFXMaterials(UNiagaraSystem* System, UNiagaraComponent* Component,
	TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, TArray<bool>& MaterialIsCore)
{
	TArray<FNiagaraVariable> AuraParameters;
	System->GetExposedParameters().GetParameters(AuraParameters);
	for (const FNiagaraVariable& Parameter : AuraParameters)
	{
		const FString Name = Parameter.GetName().ToString();
		const bool bCore = Name.StartsWith(TEXT("User.BusterAuraCoreMat_"));
		if (!bCore && !Name.StartsWith(TEXT("User.BusterAuraSparkMat_"))) continue;
		UMaterialInterface* Base = Cast<UMaterialInterface>(System->GetExposedParameters().GetUObject(Parameter));
		if (!Base) { UE_LOG(LogTemp, Error, TEXT("Buster equip aura parameter lacks a material: %s"), *Name); continue; }
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		Materials.Add(MID);
		MaterialIsCore.Add(bCore);
		Component->SetVariableMaterial(Parameter.GetName(), MID);
	}
	if (Materials.IsEmpty()) UE_LOG(LogTemp, Error, TEXT("Buster equip effect has no recolorable material bindings: %s"), *System->GetPathName());
}

void UBusterCannonComponent::StartEquipAura()
{
	StopEquipAura();
	if (!Player.IsValid()) return;
	const FVector GroundFoot = GroundPosition - FVector(0, 0,
		Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - EquipVFXFootOffsetCm);
	const auto SpawnAura = [this, &GroundFoot](TSoftObjectPtr<UNiagaraSystem>& SystemAsset, const TCHAR* Name,
		TObjectPtr<UNiagaraComponent>& Component, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials,
		TArray<bool>& MaterialIsCore)
	{
		UNiagaraSystem* System = SystemAsset.LoadSynchronous();
		if (!System) { UE_LOG(LogTemp, Error, TEXT("Buster equip aura failed to load: %s"), Name); return; }
		Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, GroundFoot,
			FRotator::ZeroRotator, FVector::OneVector, false, false, ENCPoolMethod::None, false);
		if (Component) BindEquipVFXMaterials(System, Component, Materials, MaterialIsCore);
	};
	if (EquipAuraVariant != EBusterEquipAura::GalacticField)
		SpawnAura(TsunamiEquipAura, TEXT("Tsunami"), ActiveEquipAura, EquipAuraMaterials, EquipAuraMaterialIsCore);
	if (EquipAuraVariant != EBusterEquipAura::Tsunami)
		SpawnAura(GalacticEquipAura, TEXT("Galactic Field"), SecondaryEquipAura,
			SecondaryEquipAuraMaterials, SecondaryEquipAuraMaterialIsCore);
	UpdateEquipAuraColors();
	UpdateEquipAuraTransform();
	if (ActiveEquipAura) ActiveEquipAura->Activate(true);
	if (SecondaryEquipAura) SecondaryEquipAura->Activate(true);
}

void UBusterCannonComponent::UpdateEquipAuraTransform()
{
	if ((!ActiveEquipAura && !SecondaryEquipAura) || !Player.IsValid()) return;
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float GroundFootZ = GroundPosition.Z - HalfHeight + EquipVFXFootOffsetCm;
	const float Gap = FMath::Max(0.0f, Player->GetActorLocation().Z - GroundPosition.Z);
	// Keep the effect centered in the growing gap, with its lower edge at the floor.
	const auto PositionAura = [this, GroundFootZ, Gap](UNiagaraComponent* Aura, float BaseScale)
	{
		if (!Aura) return;
		Aura->SetWorldLocation(FVector(Player->GetActorLocation().X, Player->GetActorLocation().Y,
			GroundFootZ + Gap * 0.5f));
		Aura->SetWorldScale3D(FVector(BaseScale, BaseScale,
			BaseScale * (1.0f + Gap / FMath::Max(1.0f, EquipAuraBaseHeightCm))));
	};
	PositionAura(ActiveEquipAura, TsunamiEquipAuraScale);
	PositionAura(SecondaryEquipAura, GalacticEquipAuraScale);
}

void UBusterCannonComponent::StartEquipBurst()
{
	if (EquipAuraVariant == EBusterEquipAura::GalacticField || !Player.IsValid()) return;
	StopEquipBurst();
	UNiagaraSystem* System = TsunamiEquipBurst.LoadSynchronous();
	if (!System) { UE_LOG(LogTemp, Error, TEXT("Buster Tsunami equip burst failed to load")); return; }
	const float FootZ = -Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + EquipVFXFootOffsetCm;
	ActiveEquipBurst = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Player->GetRootComponent(), NAME_None,
		FVector(0, 0, FootZ), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false, false);
	if (!ActiveEquipBurst) return;
	ActiveEquipBurst->SetRelativeScale3D(FVector(TsunamiEquipBurstScale));
	BindEquipVFXMaterials(System, ActiveEquipBurst, EquipBurstMaterials, EquipBurstMaterialIsCore);
	UpdateEquipAuraColors();
	ActiveEquipBurst->Activate(true);
}

void UBusterCannonComponent::StopEquipAura()
{
	if (ActiveEquipAura)
	{
		ActiveEquipAura->DeactivateImmediate();
		ActiveEquipAura->DestroyComponent();
		ActiveEquipAura = nullptr;
	}
	EquipAuraMaterials.Reset();
	EquipAuraMaterialIsCore.Reset();
	if (SecondaryEquipAura)
	{
		SecondaryEquipAura->DeactivateImmediate();
		SecondaryEquipAura->DestroyComponent();
		SecondaryEquipAura = nullptr;
	}
	SecondaryEquipAuraMaterials.Reset();
	SecondaryEquipAuraMaterialIsCore.Reset();
}

void UBusterCannonComponent::StopEquipBurst()
{
	if (ActiveEquipBurst)
	{
		ActiveEquipBurst->DeactivateImmediate();
		ActiveEquipBurst->DestroyComponent();
		ActiveEquipBurst = nullptr;
	}
	EquipBurstMaterials.Reset();
	EquipBurstMaterialIsCore.Reset();
}

bool UBusterCannonComponent::IsAnimationCompatibleWithMike(const UAnimSequence* Sequence, const TCHAR* AnimationName) const
{
	if (!Sequence || !Player.IsValid()) return false;
	const USkeletalMesh* MeshAsset = Player->GetMesh()->GetSkeletalMeshAsset();
	if (MeshAsset && Sequence->GetSkeleton() != MeshAsset->GetSkeleton())
	{
		UE_LOG(LogTemp, Error, TEXT("Buster %s animation skeleton differs from Mike: %s"),
			AnimationName, *Sequence->GetPathName());
		return false;
	}
	return true;
}

void UBusterCannonComponent::StopAcquisitionMontages(float BlendOutTime)
{
	if (Player.IsValid())
		if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
		{
			if (OpeningMontage) Anim->Montage_Stop(BlendOutTime, OpeningMontage);
			if (EquipMontage) Anim->Montage_Stop(BlendOutTime, EquipMontage);
			if (CalibrationMontage) Anim->Montage_Stop(BlendOutTime, CalibrationMontage);
		}
	OpeningMontage = nullptr;
	EquipMontage = nullptr;
	CalibrationMontage = nullptr;
}

void UBusterCannonComponent::ExtendAcquisitionTimeout(float ExpectedRemainingDuration)
{
	if (!bAcquiring) return;
	constexpr float SafetyMargin = 2.0f;
	AcquisitionTimeout = FMath::Max(AcquisitionTimeout,
		TotalAge + FMath::Max(0.0f, ExpectedRemainingDuration) + SafetyMargin);
}

void UBusterCannonComponent::SetPhase(EBusterAcquisitionPhase Phase)
{
	if (Phase == EBusterAcquisitionPhase::Equip) StopEquipAura();
	if (Phase == EBusterAcquisitionPhase::Lift || Phase == EBusterAcquisitionPhase::Equip ||
		Phase == EBusterAcquisitionPhase::VictoryWait)
		StopAcquisitionMontages(0.1f);
	AcquisitionPhase = Phase;
	PhaseAge = 0;
	if (Phase == EBusterAcquisitionPhase::Lift)
	{
		const float SharedDuration = FMath::Max(OrbitSeconds, LiftSeconds + 0.01f);
		ExtendAcquisitionTimeout(SharedDuration + EquipDuration + LandSeconds + 3.0f);
		if (UAnimSequence* Sequence = OpeningAnimation.LoadSynchronous())
		{
			if (IsAnimationCompatibleWithMike(Sequence, TEXT("opening")))
			{
				const float SequenceLength = Sequence->GetPlayLength();
				if (SequenceLength > KINDA_SMALL_NUMBER)
				{
					const int32 LoopCount = FMath::Max(1, FMath::CeilToInt(SharedDuration / SequenceLength));
					if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
					{
						OpeningMontage = Anim->PlaySlotAnimationAsDynamicMontage(
							Sequence, TEXT("DefaultSlot"), 0.1f, 0.12f, 1.0f, LoopCount);
						if (!OpeningMontage)
							UE_LOG(LogTemp, Error, TEXT("Buster opening animation did not play in DefaultSlot: %s"), *Sequence->GetPathName());
					}
					else UE_LOG(LogTemp, Error, TEXT("Buster opening animation has no Mike AnimInstance"));
				}
				else UE_LOG(LogTemp, Error, TEXT("Buster opening animation has no playback length: %s"), *Sequence->GetPathName());
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("Buster opening animation asset missing: %s"), *OpeningAnimation.ToSoftObjectPath().ToString());
		StartEquipAura();
		if (auto* Trail = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Variant_Platforming/VFX/NS_Jump_Trail.NS_Jump_Trail")))
		{
			OrbitTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(Trail, CannonMesh, NAME_None, FVector::ZeroVector,
				FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
			if (OrbitTrail) OrbitTrail->SetVariableLinearColor(TEXT("User.Color"), LightColor);
		}
	}
	if (Phase == EBusterAcquisitionPhase::Equip)
	{
		OrbitEndPosition = CannonMesh->GetComponentLocation();
		if (auto* Sequence = EquipAnimation.LoadSynchronous())
		{
			EquipDuration = Sequence->GetPlayLength();
			if (const auto* MeshAsset = Player->GetMesh()->GetSkeletalMeshAsset();
				MeshAsset && Sequence->GetSkeleton() != MeshAsset->GetSkeleton())
				UE_LOG(LogTemp, Error, TEXT("Buster equip animation skeleton differs from Mike: %s"), *Sequence->GetPathName());
			if (auto* Anim = Player->GetMesh()->GetAnimInstance())
			{
				EquipMontage = Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.1f, 0.12f);
				if (!EquipMontage) UE_LOG(LogTemp, Error, TEXT("Buster equip animation did not play in DefaultSlot: %s"), *Sequence->GetPathName());
			}
			else UE_LOG(LogTemp, Error, TEXT("Buster equip animation has no Mike AnimInstance"));
		}
		else UE_LOG(LogTemp, Error, TEXT("Buster equip animation asset missing: %s"), *EquipAnimation.ToSoftObjectPath().ToString());
		ExtendAcquisitionTimeout(EquipDuration + LandSeconds + 3.0f);
	}
	if (Phase == EBusterAcquisitionPhase::Land)
	{
		ExtendAcquisitionTimeout(LandSeconds + 3.0f);
	}
	if (Phase == EBusterAcquisitionPhase::VictoryWait)
	{
		VictoryWaitDuration = 1.0f;
		Player->SetActorLocation(GroundPosition, true);
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Pulse(GroundPosition - FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 75);
		if (UAnimSequence* Sequence = CalibrationAnimation.LoadSynchronous())
		{
			if (IsAnimationCompatibleWithMike(Sequence, TEXT("calibration")))
			{
				if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
				{
					constexpr float IdleBlendSeconds = 0.3f;
					constexpr float IdleSettleSeconds = 0.25f;
					// Finish the stretch before blending back to the locomotion graph's idle.
					CalibrationMontage = Anim->PlaySlotAnimationAsDynamicMontage(
						Sequence, TEXT("DefaultSlot"), 0.1f, IdleBlendSeconds, 1.0f, 1, 0.0f);
					if (CalibrationMontage)
						VictoryWaitDuration = FMath::Max(1.0f,
							Sequence->GetPlayLength() + IdleBlendSeconds + IdleSettleSeconds);
					else UE_LOG(LogTemp, Error, TEXT("Buster calibration animation did not play in DefaultSlot: %s"), *Sequence->GetPathName());
				}
				else UE_LOG(LogTemp, Error, TEXT("Buster calibration animation has no Mike AnimInstance"));
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("Buster calibration animation asset missing: %s"), *CalibrationAnimation.ToSoftObjectPath().ToString());
		ExtendAcquisitionTimeout(VictoryWaitDuration);
	}
}

void UBusterCannonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (PendingShotAt > 0 && GetWorld()->GetTimeSeconds() >= PendingShotAt)
	{
		PendingShotAt = 0;
		if (!Player.IsValid() || !Player->CanFireBuster()) CancelFire();
		else ReleaseShot();
	}
	if (bQueuedFollowup && GetWorld()->GetTimeSeconds() >= NextShotAt)
	{
		bQueuedFollowup = false;
		if (HoldEndsAt > GetWorld()->GetTimeSeconds()) TryFire();
	}
	if (FireEndsAt > 0)
	{
		if (!Player.IsValid() || !Player->CanFireBuster()) CancelFire();
		else if (GetWorld()->GetTimeSeconds() >= FireEndsAt) FinishFire();
	}
	if (RecoilStartedAt >= 0 && CannonMesh)
	{
		const float RecoilAlpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - RecoilStartedAt) /
			FMath::Max(0.01f, CannonRecoilSeconds), 0.f, 1.f);
		const float Offset = CannonRecoilCm * GetRecoilAlpha();
		CannonMesh->SetRelativeLocation(FVector(0, -Offset, 0));
		if (RecoilAlpha >= 1.f) RecoilStartedAt = -1;
	}
	if (HoldEndsAt > 0 && (GetWorld()->GetTimeSeconds() >= HoldEndsAt || !Player.IsValid() ||
		!Player->CanFireBuster() || !Player->GetCharacterMovement()->IsMovingOnGround() ||
		Player->GetCharacterMovement()->Velocity.Size2D() > 5.0f)) CancelHeldPose();
	if (!bAcquiring) return;
	if (!Player.IsValid()) { CompleteAcquisition(); return; }
	PhaseAge += DeltaTime;
	TotalAge += DeltaTime;
	if (TotalAge > AcquisitionTimeout) { CompleteAcquisition(); return; }
	if (CinematicCamera)
	{
		// Frame Mike's whole body, with room for the feet and antenna at the end of the move.
		const FVector Target = Player->GetActorLocation() + FVector(0,0,-35);
		const FVector FramingOffset = CameraOffset + CameraOffset.GetSafeNormal() * 40.0f;
		const float Delay = FMath::Clamp(CameraZoomDelaySeconds, 0.0f, OrbitSeconds * 0.45f);
		const float Alpha = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((TotalAge - Delay) /
			FMath::Max(0.01f, OrbitSeconds - Delay), 0.f, 1.f));
		const FVector Eye = FMath::Lerp(CameraStartPosition, Target + FramingOffset, Alpha);
		CinematicCamera->SetActorLocation(Eye);
		CinematicCamera->SetActorRotation(FQuat::Slerp(CameraStartRotation.Quaternion(),
			(Target - Eye).Rotation().Quaternion(), Alpha));
		CinematicCamera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(CameraStartFOV, CameraFOV, Alpha));
	}
	const auto Ease = [](float T, float Duration) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T / FMath::Max(0.01f, Duration), 0.f, 1.f)); };
	switch (AcquisitionPhase)
	{
	case EBusterAcquisitionPhase::Lift:
	case EBusterAcquisitionPhase::Orbit:
	{
		const float SharedDuration = FMath::Max(OrbitSeconds, LiftSeconds + 0.01f);
		const float Progress = Ease(TotalAge, SharedDuration);
		Player->SetActorLocation(FMath::Lerp(GroundPosition, LiftPosition, Progress));
		const float Angle = OrbitStartAngle + 2 * PI * FMath::Max(1, OrbitTurns) * Progress;
		const float StartRadius = FVector2D(PickupPosition - GroundPosition).Size();
		const float MidRadius = (StartRadius + OrbitMeetRadius) * 0.5f;
		const float Radius = FMath::Lerp(StartRadius, OrbitMeetRadius, Progress) +
			FMath::Max(0.f, OrbitRadius - MidRadius) * FMath::Sin(PI * Progress);
		const float Height = FMath::Lerp(PickupPosition.Z, LiftPosition.Z + OrbitEndVerticalOffset, Progress);
		CannonMesh->SetWorldLocation(FVector(GroundPosition.X + FMath::Cos(Angle) * Radius,
			GroundPosition.Y + FMath::Sin(Angle) * Radius, Height));
		CannonMesh->SetWorldRotation(FRotator(0, FMath::RadiansToDegrees(Angle), 0));
		UpdateEquipAuraTransform();
		if (TotalAge >= SharedDuration) SetPhase(EBusterAcquisitionPhase::Equip);
		else if (AcquisitionPhase == EBusterAcquisitionPhase::Lift && PhaseAge >= LiftSeconds) SetPhase(EBusterAcquisitionPhase::Orbit);
		break;
	}
	case EBusterAcquisitionPhase::Equip:
		if (!bEquipped)
		{
			const FTransform Socket = Player->GetMesh()->GetSocketTransform(TEXT("Buster_RSocket"));
			CannonMesh->SetWorldLocation(FMath::Lerp(OrbitEndPosition, Socket.GetLocation(), Ease(PhaseAge, EquipDuration * AttachmentFraction)));
			CannonMesh->SetWorldRotation(Socket.GetRotation());
			if (PhaseAge >= EquipDuration * AttachmentFraction)
			{
				Equip();
				Pulse(CannonMesh->GetComponentLocation(), 55);
				StartEquipBurst();
				if (OrbitTrail) OrbitTrail->Deactivate();
			}
		}
		if (PhaseAge >= EquipDuration) { Equip(); SetPhase(EBusterAcquisitionPhase::Land); }
		break;
	case EBusterAcquisitionPhase::Land:
		Player->SetActorLocation(FMath::Lerp(LiftPosition, GroundPosition, Ease(PhaseAge, LandSeconds)));
		if (PhaseAge >= LandSeconds) SetPhase(EBusterAcquisitionPhase::VictoryWait);
		break;
	case EBusterAcquisitionPhase::VictoryWait:
		if (PhaseAge >= VictoryWaitDuration) CompleteAcquisition();
		break;
	default: break;
	}
}

void UBusterCannonComponent::RestorePresentation()
{
	StopEquipAura();
	StopEquipBurst();
	if (OrbitTrail) { OrbitTrail->DestroyComponent(); OrbitTrail = nullptr; }
	StopAcquisitionMontages(0.1f);
	if (Player.IsValid())
	{
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if (auto* PC = Cast<APlayerController>(Player->GetController()); PC && CinematicCamera)
			PC->SetViewTarget(PreviousViewTarget.IsValid() ? PreviousViewTarget.Get() : Player.Get());
	}
	if (CinematicCamera) { CinematicCamera->Destroy(); CinematicCamera = nullptr; }
	for (int32 Index = 0; Index < HiddenWidgets.Num(); ++Index)
		if (IsValid(HiddenWidgets[Index])) HiddenWidgets[Index]->SetVisibility(WidgetVisibility[Index]);
	HiddenWidgets.Reset(); WidgetVisibility.Reset();
	for (auto Widget : HiddenWorldWidgets) if (Widget.IsValid()) Widget->SetHiddenInGame(false);
	HiddenWorldWidgets.Reset();
}
void UBusterCannonComponent::CompleteAcquisition()
{
	if (!bAcquiring) return;
	bAcquiring = false;
	Equip();
	if (Player.IsValid()) Player->SetActorLocation(GroundPosition, true);
	RestorePresentation();
	AcquisitionPhase = EBusterAcquisitionPhase::Complete;
	if (auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) Run->ReleaseVictoryPresentation();
}
void UBusterCannonComponent::EndPlay(EEndPlayReason::Type Reason)
{
	CancelFire();
	if (bAcquiring)
    {
        bAcquiring = false;
        RestorePresentation();
        if (Reason == EEndPlayReason::Destroyed)
            if (auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) Run->ReleaseVictoryPresentation();
    }
	if (UWorld* World = GetWorld()) if (auto* GI = World->GetGameInstance())
		if (auto* State = GI->GetSubsystem<UQuasicomboLoadoutSubsystem>())
		{
			State->OnMikeLightColorChanged.RemoveDynamic(this, &UBusterCannonComponent::ApplyLightColor);
			State->OnMikeBodyPaletteChanged.RemoveDynamic(this, &UBusterCannonComponent::ApplyBodyPalette);
		}
	Super::EndPlay(Reason);
}
