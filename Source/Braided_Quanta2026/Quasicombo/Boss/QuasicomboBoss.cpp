#include "QuasicomboBoss.h"
#include "QuasicomboQTEDirector.h"
#include "QuasicomboQTERules.h"
#include "BusterCannonComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "QuasicomboEvolutionComponent.h"
#include "QuasicomboEncounterLightingComponent.h"
#include "QuasicomboMusicComponent.h"
#include "SideScrollingGameMode.h"
#include "HAL/PlatformTime.h"
#include "QuasicomboBossRules.h"
#include "QuasicomboBossAnimInstance.h"
#include "QuasicomboVictoryPickup.h"
#include "CombatLifeBar.h"
#include "QuasicomboRunSubsystem.h"
#include "SideScrollingCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Misc/App.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"

namespace
{
float BossResistanceForArmorBars(int32 ArmorBars)
{
	return FMath::Lerp(0.6f, 1.0f, FMath::Clamp(static_cast<float>(ArmorBars) / 2.0f, 0.0f, 1.0f));
}

const TCHAR* ArmorNames[] = {
	TEXT("SK_Cloth"), TEXT("SK_BeltWaist"), TEXT("SK_LeftArmArmor"), TEXT("SK_RightArmArmor"),
	TEXT("SK_LeftShoulderArmor_01"), TEXT("SK_BeltsBody"), TEXT("SK_RightShoulderArmor"),
	TEXT("SK_LeftShoulderArmor_02"), TEXT("SK_LeftArmArmorSpikes"), TEXT("SK_RightArmArmorSpikes"),
	TEXT("SK_LeftShoulderArmorSpikes"), TEXT("SK_Skull"), TEXT("SK_WaistBone")
};
} // namespace

AQuasicomboBoss::AQuasicomboBoss()
{
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RequiredHits = 10;
	MaxHP = 10.0f;
	AggroRange = 2200.0f;
	AggroHeightTolerance = 600.0f;
	AggroDepthTolerance = 220.0f;
	PatrolHalfWidth = 2200.0f;
	AttackRange = 160.0f;
	MeleeTraceDistance = 150.0f;
	AttackTimeout = 3.5f;
	MeleeDamage = 2.0f;
	TailDamage = 4.0f;
	SetResistance(BossResistanceForArmorBars(0));
	Evolution = CreateDefaultSubobject<UQuasicomboEvolutionComponent>(TEXT("EvolutionPresentation"));
	EncounterLighting = CreateDefaultSubobject<UQuasicomboEncounterLightingComponent>(TEXT("EncounterLighting"));
	Quantum = CreateDefaultSubobject<UQuantumBossComponent>(TEXT("QuantumState"));
	QTE = CreateDefaultSubobject<UQuasicomboQTEComponent>(TEXT("FinalQTE"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Quasicombo/Boss/Lizard/SK_Lizard_CinematicBody"));
	if (Body.Succeeded()) GetMesh()->SetSkeletalMesh(Body.Object);
	GetMesh()->SetRelativeLocation(FVector(0, 0, -87));
	GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
	GetMesh()->SetAnimInstanceClass(UQuasicomboBossAnimInstance::StaticClass());
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	for (const TCHAR* Name : ArmorNames)
	{
		USkeletalMeshComponent* Part = CreateDefaultSubobject<USkeletalMeshComponent>(FName(Name));
		Part->SetupAttachment(GetMesh());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ConstructorHelpers::FObjectFinder<USkeletalMesh> PartAsset(*(FString(TEXT("/Game/Lizardman_Berserker/Mesh/SeparatedMesh/")) + Name));
		if (PartAsset.Succeeded()) Part->SetSkeletalMesh(PartAsset.Object);
		ArmorPieces.Add(Part);
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Lizardman_Berserker/Demo/TestAnimations/ThirdPersonIdle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Lizardman_Berserker/Demo/TestAnimations/ThirdPersonWalk"));
	IdleAnimation = Idle.Object;
	WalkAnimation = Walk.Object;
	for (int32 I = 1; I <= 12; ++I)
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
			*FString::Printf(TEXT("/Game/Lizardman_Berserker/Materials/MI_Body_Skin_%02d_Inst"), I));
		SkinMaterials.Add(Material.Object);
	}
	for (const TCHAR *Name : {TEXT("M_Eyes_03"), TEXT("M_Eyes_02"), TEXT("M_GlowEyes")})
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(*(FString(TEXT("/Game/Quasicombo/Boss/Lizard/Materials/")) + Name + TEXT("_Evolution")));
		EvolutionEyeMaterials.Add(Material.Object);
	}
	MusicA = CreateDefaultSubobject<UAudioComponent>(TEXT("BossMusicA"));
	MusicB = CreateDefaultSubobject<UAudioComponent>(TEXT("BossMusicB"));
	EvolutionGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("EvolutionGlow"));
	EvolutionGlow->SetupAttachment(GetMesh(), TEXT("spine_03"));
	EvolutionGlow->SetLightColor(FLinearColor(0.12f, 0.7f, 1.0f));
	EvolutionGlow->SetAttenuationRadius(420.0f);
	EvolutionGlow->SetIntensity(0.0f);
	EvolutionGlow->SetVisibility(false);
	EvolutionGlow->SetCastShadows(false);
	for (UAudioComponent* Audio : {MusicA.Get(), MusicB.Get()})
	{
		Audio->SetupAttachment(RootComponent);
		Audio->bAutoActivate = false;
		Audio->bAllowSpatialization = false;
		Audio->bIsUISound = false;
	}
}

void AQuasicomboBoss::OnConstruction(const FTransform &Transform)
{
	Super::OnConstruction(Transform);
	if (const USkeletalMesh *Body = GetMesh()->GetSkeletalMeshAsset())
	{
		const auto Bounds = Body->GetBounds();
		FVector Offset = GetMesh()->GetRelativeLocation();
		Offset.Z = -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + 3.0f -
				   (Bounds.Origin.Z - Bounds.BoxExtent.Z) * GetMesh()->GetRelativeScale3D().Z;
		GetMesh()->SetRelativeLocation(Offset);
	}
	for (USkeletalMeshComponent *Part : ArmorPieces)
		Part->SetLeaderPoseComponent(GetMesh());
	UpdateAppearance();
}

void AQuasicomboBoss::BeginPlay()
{
	BaseHealthHits = FMath::Max(4, BaseHealthHits);
	HitsPerArmorBar = FMath::Max(1, HitsPerArmorBar);
	RequiredHits = BaseHealthHits;
	BaseHitsRemaining = BaseHealthHits;
	SetResistance(BossResistanceForArmorBars(0));
	Super::BeginPlay();
	if (SkinMaterials.Num() != 12)
	{
		SkinMaterials.Reset();
		for (int32 I = 1; I <= 12; ++I)
			SkinMaterials.Add(LoadObject<UMaterialInterface>(
				nullptr, *FString::Printf(TEXT("/Game/Lizardman_Berserker/Materials/MI_Body_Skin_%02d_Inst"), I)));
	}
	BossStartTransform = GetActorTransform();
	for (USkeletalMeshComponent *Part : ArmorPieces)
		Part->SetLeaderPoseComponent(GetMesh());
	if (UWidgetComponent *Bar = FindComponentByClass<UWidgetComponent>())
	{
		Bar->SetRelativeLocation(FVector(0, 0, 95));
		Bar->SetHiddenInGame(true);
	}
	Quantum->OnQuantumStateReady.AddDynamic(this, &AQuasicomboBoss::ApplyQuantumTuning);
	Quantum->OnQuantumUpdated.AddDynamic(this, &AQuasicomboBoss::HandleQuantumUpdate);
	if (UQuasicomboRunSubsystem *Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
		Run->OnRunEnded.AddDynamic(this, &AQuasicomboBoss::HandleRunEnded);
	ApplyQuantumTuning(Quantum->GetVacuumProbability(), Quantum->GetTauProbability(), Quantum->UsedFallback());
	UpdateMusic();
}

void AQuasicomboBoss::StartEncounter(ASideScrollingCharacter* Player)
{
	if (Phase != EQuasicomboBossPhase::Waiting)
		return;
	const UQuasicomboRunSubsystem *Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	if (!Run || !Run->HasRunStarted() || Run->GetOutcome() != EQuasicomboRunOutcome::Playing)
		return;
	Quantum->EnsureState();
	StartingSkin = QuasicomboBossRules::SkinPalette(GetEffectiveTau());
	bSkinCaptured = true;
	InitialAttackCooldown = FMath::Lerp(2.0f, 0.8f, float(GetEffectiveTau()));
	InitialMoveSpeed = FMath::Lerp(145.0f, 250.0f, float(GetEffectiveTau()));
	EncounterPlayer = Player;
	if (Player)
		PlayerStartTransform = Player->GetActorTransform();
	BossStartTransform = GetActorTransform();
	Phase = EQuasicomboBossPhase::Combat;
	ResetSideCombat(BaseHealthHits);
	EncounterLighting->BeginCombat();
	UpdateHealthBar();
	if (UQuasicomboRunSubsystem* MutableRun = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
		MutableRun->RegisterBossEncounter(this);
	// Every encounter entry path comes through here. Use the normal death path
	// so hidden route enemies also notify their gates and remove their ragdolls.
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemy::StaticClass(), Enemies);
	for (AActor* Actor : Enemies)
	{
		ACombatEnemy* Enemy = Cast<ACombatEnemy>(Actor);
		if (IsValid(Enemy) && !Enemy->IsA<AQuasicomboBoss>() && Enemy->CurrentHP > 0.0f)
			Enemy->TakeDamage(Enemy->CurrentHP, FDamageEvent(), nullptr, this);
	}
	ApplyQuantumTuning(Quantum->GetVacuumProbability(), GetEffectiveTau(), Quantum->UsedFallback());
}

int32 AQuasicomboBoss::GetAdditionalQTERoundsRemaining() const
{
	if (!bOutcomeSampled || bCinematicDefeated || Phase == EQuasicomboBossPhase::Waiting ||
		Phase == EQuasicomboBossPhase::AwaitPickup || Phase == EQuasicomboBossPhase::Complete)
		return 0;
	const UQuasicomboRunSubsystem* Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	return Run && Run->GetOutcome() == EQuasicomboRunOutcome::Playing ? FMath::Max(0, FinisherRounds - FinisherRound) : 0;
}

void AQuasicomboBoss::Tick(float DeltaSeconds)
{
	if (Phase == EQuasicomboBossPhase::Retry)
	{
		if (FPlatformTime::Seconds() >= RetryResumeAt)
			RestartEncounter();
		return;
	}
	if (UGameplayStatics::IsGamePaused(this))
		return;
	if (bOpeningCharge)
		TickOpeningCharge(DeltaSeconds);
	Super::Tick(DeltaSeconds);
	// The arena transfer is normally responsible for starting combat. If the
	// player reaches the lizard without crossing that small trigger, proximity
	// must still make the placed boss fightable.
	if (Phase == EQuasicomboBossPhase::Waiting)
	{
		ASideScrollingCharacter* Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (Player && Player->GetCurrentHP() > 0.0f)
		{
			const FVector Offset = Player->GetActorLocation() - GetActorLocation();
			if (FMath::Abs(Offset.X) <= AggroRange && FMath::Abs(Offset.Y) <= AggroDepthTolerance &&
				FMath::Abs(Offset.Z) <= AggroHeightTolerance)
				StartEncounter(Player);
		}
	}
	MusicElapsed += FApp::GetDeltaTime();
	if (Phase == EQuasicomboBossPhase::Evolution)
	{
		const double Elapsed = FPlatformTime::Seconds() - PhaseStartedAt;
		EvolutionGlow->SetIntensity(5500.0f + 2500.0f * FMath::Sin(static_cast<float>(Elapsed) * 13.0f));
	}
	if (bTailActive)
		UpdateTailAttack();
	// SoundWave slots can be looping or one-shot; both remain continuous.
	if (CurrentMusic && Phase != EQuasicomboBossPhase::AwaitPickup && Phase != EQuasicomboBossPhase::Complete)
	{
		UAudioComponent* Active = bUseMusicA ? MusicA : MusicB;
		if (!Active->IsPlaying()) Active->Play(0.0f);
	}
}

double AQuasicomboBoss::GetEffectiveTau() const
{
#if WITH_EDITORONLY_DATA
	if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE && PreviewTau >= 0.0)
		return FMath::Clamp(!bHasEvolved && PreviewPreEvolutionTau >= 0.0 ? PreviewPreEvolutionTau : PreviewTau, 0.0, 1.0);
#endif
	return Quantum ? Quantum->GetTauProbability() : 0.0;
}

void AQuasicomboBoss::ApplyQuantumTuning(double, double, bool)
{
	const float Aggression = static_cast<float>(GetEffectiveTau());
	AttackCooldown = FMath::Lerp(2.0f, 0.8f, Aggression);
	AttackWindup = FMath::Lerp(0.42f, 0.16f, Aggression);
	ChargedAttackWindup = FMath::Lerp(1.1f, 0.7f, Aggression);
	RecoveryDuration = FMath::Lerp(0.65f, 0.25f, Aggression);
	GetCharacterMovement()->MaxWalkSpeed = FMath::Lerp(145.0f, 250.0f, Aggression);
	UpdateAppearance();
	UpdateHealthBar();
	UpdateMusic();
}

void AQuasicomboBoss::HandleQuantumUpdate(EQuasicomboQuantumUpdate Reason, double, double, bool)
{
	if (Reason == EQuasicomboQuantumUpdate::Braid) UpdateAppearance();
}

void AQuasicomboBoss::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& Location, const FVector& Impulse)
{
	if (Phase == EQuasicomboBossPhase::Waiting)
	{
		if (ASideScrollingCharacter* Player = Cast<ASideScrollingCharacter>(DamageCauser))
		{
			const FVector Offset = Player->GetActorLocation() - GetActorLocation();
			if (FMath::Abs(Offset.X) <= AggroRange && FMath::Abs(Offset.Y) <= AggroDepthTolerance &&
				FMath::Abs(Offset.Z) <= AggroHeightTolerance)
				StartEncounter(Player);
		}
	}
	const int32 Before = AcceptedStrikes;
	Super::ApplyDamage(Damage, DamageCauser, Location, Impulse);
	if (AcceptedStrikes != Before && bOpeningCharge && ReactionAnimationWeight > 0.001f)
	{
		bOpeningCharge = false;
		ApplyQuantumTuning(0, GetEffectiveTau(), Quantum->UsedFallback());
	}
	if (AcceptedStrikes != Before && bTailActive && ReactionAnimationWeight > 0.001f)
	{
		bTailActive = false;
		if (UAnimInstance *Anim = GetMesh()->GetAnimInstance())
			Anim->Montage_Stop(0.08f, TailMontage);
	}
}

void AQuasicomboBoss::ResolveAcceptedStrike(AActor *Causer, const FVector &Location, const FVector &Impulse)
{
	ReactionAnimationWeight = 1.0f - GetResistance();
	const int32 BeforeBars = GetArmorBars();
	if (ArmorHitsRemaining > 0)
		--ArmorHitsRemaining;
	else
		BaseHitsRemaining = FMath::Max(0, BaseHitsRemaining - 1);
	if (BeforeBars > GetArmorBars())
		Evolution->BreakArmorGroup(BeforeBars - 1, Impulse);
	SetResistance(BossResistanceForArmorBars(GetArmorBars()));
	if (BaseHitsRemaining == 0 && ArmorHitsRemaining == 0)
	{
		HandleRequiredHitsReached(Causer, Location, Impulse);
	}
	else
	{
		// Keep total HP/effects in sync while using the authored stagger animation.
		// Apply HP without the base enemy's unconditional montage interruption.
		FDamageEvent Event;
		const float Actual = TakeDamage(1.0f, Event, nullptr, Causer);
		if (Actual > 0.0f)
			ReceivedDamage(Actual, Location, FVector::ZeroVector);
		GetMesh()->SetPhysicsBlendWeight(0.0f);
		CurrentHP = static_cast<float>(BaseHitsRemaining + ArmorHitsRemaining);
		if (ReactionAnimationWeight > 0.001f)
		{
			GetCharacterMovement()->StopMovementImmediately();
			if (UAnimInstance *Anim = GetMesh()->GetAnimInstance(); Anim && HitReactionAnimation)
				HitReactionMontage = Anim->PlaySlotAnimationAsDynamicMontage(HitReactionAnimation, TEXT("DefaultSlot"), 0.03f, 0.06f,
																			 1.0f / ReactionAnimationWeight);
		}
	}
	UpdateAppearance();
	UpdateHealthBar();
	UpdateMusic();
}

void AQuasicomboBoss::OnStrikeAccepted(int32 StrikeCount)
{
	// ResolveAcceptedStrike has already deducted this hit from base health.
	if (BaseHitsRemaining <= 0 || BaseHitsRemaining > FMath::Max(1, EvolutionTriggerHealth) ||
		bHasEvolved || Phase != EQuasicomboBossPhase::Combat) return;
	EvolutionBeforeTau = GetEffectiveTau();
	Phase = EQuasicomboBossPhase::Evolution;
	EncounterLighting->BeginEvolution();
	UpdateMusic();
	EvolutionGlow->SetVisibility(true);
	PhaseStartedAt = FPlatformTime::Seconds();
	bTailActive = false;
	SuspendSideCombat();
	PreviousBossMovementMode = GetCharacterMovement()->MovementMode;
	GetCharacterMovement()->DisableMovement();
	FreezePlayerAndCamera(false);
	Quantum->EvolveState();
	Evolution->Start();
}

void AQuasicomboBoss::ResolveEvolutionPresentation()
{
	if (bHasEvolved || Phase != EQuasicomboBossPhase::Evolution)
		return;
	bHasEvolved = true;
	AwardedArmorBars = QuasicomboBossRules::ArmorBars(Quantum->GetTauProbability() - Quantum->GetPreEvolutionTau(),
		FMath::Max(0.0, FirstArmorThreshold), FMath::Max(FirstArmorThreshold, SecondArmorThreshold));
#if WITH_EDITORONLY_DATA
	if (GetWorld()->WorldType == EWorldType::PIE && PreviewArmorBars >= 0)
		AwardedArmorBars = FMath::Clamp(PreviewArmorBars, 0, 2);
#endif
	ArmorHitsRemaining = AwardedArmorBars * HitsPerArmorBar;
	// Evolution restores the depleted base health as well as awarding shields.
	// Keep strike accounting and the active presentation intact.
	BaseHitsRemaining = BaseHealthHits;
	SetResistance(BossResistanceForArmorBars(GetArmorBars()));
	// The base enemy limits cumulative accepted strikes. Extend that budget for
	// the refilled phase without counting earlier hits against the new health.
	RequiredHits = AcceptedStrikes + BaseHealthHits + ArmorHitsRemaining;
	MaxHP = static_cast<float>(BaseHealthHits + ArmorHitsRemaining);
	CurrentHP = static_cast<float>(BaseHitsRemaining + ArmorHitsRemaining);
	UpdateHealthBar();
}
void AQuasicomboBoss::RevealEvolutionEyes(float Charge)
{
	// Continue the emissive ramp during the final reveal, even after the color locks.
	PreviewEvolutionEyes(true, Charge);
	if (bEyesRevealed)
		return;
	bEyesRevealed = true;
	UpdateHealthBar();
}
void AQuasicomboBoss::PreviewEvolutionEyes(bool bAwarded, float Charge)
{
	if (!bAwarded)
	{
		if (!bEyesRevealed)
			GetMesh()->SetMaterial(1, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Lizardman_Berserker/Materials/M_Eyes_01")));
		return;
	}
	if (EvolutionEyeMaterials.IsValidIndex(AwardedArmorBars))
	{
		if (!EyeMaterial)
		{
			EyeMaterial = GetMesh()->CreateDynamicMaterialInstance(1, EvolutionEyeMaterials[AwardedArmorBars]);
			if (EyeMaterial && AwardedArmorBars == 2)
			{
				EyeMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.04f, 0.02f));
				EyeMaterial->SetScalarParameterValue(TEXT("Emissive"), 7.0f);
			}
		}
		if (EyeMaterial)
		{
			GetMesh()->SetMaterial(1, EyeMaterial);
			EyeMaterial->SetScalarParameterValue(TEXT("EvolutionEmissiveScale"),
				FMath::Lerp(0.1f, 1.0f, FMath::Clamp(Charge, 0.0f, 1.0f)));
		}
	}
}
bool AQuasicomboBoss::GetEvolutionGameplayView(FMinimalViewInfo& View) const
{
	AActor* Target = PreviousViewTarget.Get();
	if (!IsValid(Target))
		if (auto* PC = UGameplayStatics::GetPlayerController(this, 0)) Target = PC->GetPawn();
	if (!IsValid(Target)) return false;
	Target->CalcCamera(0.0f, View);
	return true;
}
void AQuasicomboBoss::SetEvolutionPlayerVisible(bool bVisible)
{
	if (auto* Player = EncounterPlayer.Get())
	{
		if (!bVisible && !bEvolutionPlayerHidden)
		{
			bPlayerWasHidden = Player->IsHidden();
			bEvolutionPlayerHidden = true;
			Player->SetActorHiddenInGame(true);
		}
		else if (bVisible && bEvolutionPlayerHidden)
		{
			bEvolutionPlayerHidden = false;
			Player->SetActorHiddenInGame(bPlayerWasHidden);
		}
	}
}
void AQuasicomboBoss::CompleteEvolutionPresentation()
{
	if (Phase != EQuasicomboBossPhase::Evolution)
		return;
	EncounterLighting->EndEvolution();
	RevealEvolutionEyes();
	EvolutionGlow->SetVisibility(false);
	// The presentation camera has already reached this view along one diagonal.
	RestoreCameraAndTime(0.0f);
	GetCharacterMovement()->SetMovementMode(PreviousBossMovementMode);
	Phase = EQuasicomboBossPhase::Combat;
	UpdateAppearance();
	UpdateHealthBar();
	UpdateMusic();
	bOpeningCharge = GetEffectiveTau() >= 2.0 / 3.0;
	bOpeningPunch = false;
	ChargeStartedAt = GetWorld()->GetTimeSeconds();
	if (bOpeningCharge)
	{
		CombatState = ESideCombatState::Recovery;
		if (auto *Anim = GetMesh()->GetAnimInstance())
			if (auto *Clip = Evolution->RiseAnimation.LoadSynchronous())
				Anim->PlaySlotAnimationAsDynamicMontage(Clip, TEXT("DefaultSlot"), 0.08f, 0.12f,
														Clip->GetPlayLength() / (AttackWindup + 0.35f));
	}
}
void AQuasicomboBoss::AbortEvolutionPresentation()
{
	if (Phase != EQuasicomboBossPhase::Evolution)
		return;
	EncounterLighting->EndEvolution();
	Quantum->TimeoutEvolution();
	ResolveEvolutionPresentation();
	RevealEvolutionEyes();
	EvolutionGlow->SetVisibility(false);
	RestoreCameraAndTime();
	GetCharacterMovement()->SetMovementMode(PreviousBossMovementMode);
	Phase = EQuasicomboBossPhase::Combat;
	bOpeningCharge = false;
	UpdateAppearance();
	UpdateHealthBar();
	UpdateMusic();
}
TArray<USkeletalMeshComponent *> AQuasicomboBoss::GetEvolutionArmor() const
{
	TArray<USkeletalMeshComponent *> Out;
	for (const auto &P : ArmorPieces)
		Out.Add(P.Get());
	return Out;
}
bool AQuasicomboBoss::ArmorInGroup(int32 Piece, int32 Group) const
{
	if (!ArmorPieces.IsValidIndex(Piece)) return false;
	if (Group == 0) return Piece < 4 || Piece == (AwardedArmorBars == 2 ? 7 : 4);
	return Group == 1 && Piece >= 5 && Piece != 7;
}
float AQuasicomboBoss::GetReactionAnimationWeight() const
{
	const auto *Anim = GetMesh()->GetAnimInstance();
	return Anim && HitReactionMontage && Anim->Montage_IsPlaying(HitReactionMontage) ? ReactionAnimationWeight : 1.0f;
}
void AQuasicomboBoss::TickOpeningCharge(float Delta)
{
	if (Phase != EQuasicomboBossPhase::Combat || !EncounterPlayer.IsValid())
	{
		bOpeningCharge = false;
		return;
	}
	const float Age = GetWorld()->GetTimeSeconds() - ChargeStartedAt;
	if (Age < AttackWindup + 0.35f)
		return;
	const FVector Offset = EncounterPlayer->GetActorLocation() - GetActorLocation();
	if (FMath::Abs(Offset.Z) > AttackHeightTolerance || FMath::Abs(Offset.Y) > AttackDepthTolerance || Age > 2.5f)
	{
		bOpeningCharge = false;
		GetCharacterMovement()->MaxWalkSpeed = FMath::Lerp(145.0f, 250.0f, float(GetEffectiveTau()));
		return;
	}
	const float Direction = FMath::Sign(Offset.X);
	SetActorRotation(FRotator(0, Direction < 0 ? 180 : 0, 0));
	if (FMath::Abs(Offset.X) <= AttackRange)
	{
		bOpeningCharge = false;
		GetCharacterMovement()->MaxWalkSpeed = FMath::Lerp(145.0f, 250.0f, float(GetEffectiveTau()));
		bOpeningPunch = true;
		TriggerSideAttack();
		bOpeningPunch = false;
		return;
	}
	FHitResult Floor;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BossCharge), false, this);
	const FVector Ahead = GetActorLocation() + FVector(Direction * 95, 0, -70);
	if (!GetWorld()->LineTraceSingleByObjectType(Floor, Ahead, Ahead - FVector(0, 0, 180), FCollisionObjectQueryParams(ECC_WorldStatic),
												 Query))
	{
		bOpeningCharge = false;
		GetCharacterMovement()->MaxWalkSpeed = FMath::Lerp(145.0f, 250.0f, float(GetEffectiveTau()));
		return;
	}
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;
	AddMovementInput(FVector(Direction, 0, 0));
}
FString AQuasicomboBoss::GetEvolutionBuffs() const
{
	if (!bEyesRevealed || Phase == EQuasicomboBossPhase::Waiting || Phase == EQuasicomboBossPhase::Complete ||
		Phase == EQuasicomboBossPhase::AwaitPickup)
		return FString();
	const float Speed = FMath::Lerp(145.0f, 250.0f, float(GetEffectiveTau()));
	return FString::Printf(TEXT("ATTACK RATE %+.0f%%   |   SPEED %+.0f%%   |   RESISTANCE %.0f%%"),
						   (InitialAttackCooldown / AttackCooldown - 1.0f) * 100.0f, (Speed / InitialMoveSpeed - 1.0f) * 100.0f,
						   GetResistance() * 100.0f);
}

void AQuasicomboBoss::HandleRequiredHitsReached(AActor *, const FVector &, const FVector &)
{
	CurrentHP = 0.0f;
	bTailActive = false;
	SuspendSideCombat();
	CombatState = ESideCombatState::Dead;
	GetCharacterMovement()->DisableMovement();
	BeginFinisher();
}

void AQuasicomboBoss::BeginFinisher()
{
    if(Phase==EQuasicomboBossPhase::QTE||Phase==EQuasicomboBossPhase::Complete) return;
    auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
    auto* Player=EncounterPlayer.Get();
    if(!Player) Player=Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Run||!Player||Run->GetOutcome()!=EQuasicomboRunOutcome::Playing) return;
    Phase=EQuasicomboBossPhase::QTE; UpdateHealthBar(); Quantum->FreezeForFinisher();
    if(!bOutcomeSampled)
    {
        bOutcomeSampled=true; bTauOutcome=FMath::FRand()<GetEffectiveTau();
        FinisherRounds=QuasicomboBossRules::QTERounds(GetEffectiveTau());
        FinalPromptCount=QuasicomboQTERules::FinalPromptCount(GetEffectiveTau(),Player->GetBusterCannon()->IsEquipped());
#if WITH_EDITORONLY_DATA
        if(GetWorld()->WorldType==EWorldType::PIE&&bEnableQTEPreview)
        {
            if(PreviewQTEOutcome>=0) bTauOutcome=PreviewQTEOutcome==1;
            FinisherRounds=FMath::Clamp(PreviewQTERounds,1,3);
            if(PreviewQTEPrompts==6) Player->GetBusterCannon()->Equip();
            FinalPromptCount=FMath::Clamp(PreviewQTEPrompts,2,Player->GetBusterCannon()->IsEquipped()?6:5);
        }
#endif
        Run->RecordFinisherMeasurement(bTauOutcome,Quantum->HasQuantumState(),Quantum->GetVacuumProbability(),
            Quantum->GetTauProbability(),Quantum->UsedFallback());
    }
    OnFinisherStarted(bTauOutcome);
    auto* Director=GetWorld()->SpawnActor<AQuasicomboQTEDirector>();
    if(!Director||!Director->Start(this,Player,bTauOutcome,FinisherRound,FinisherRounds,
        QuasicomboQTERules::RoundPromptCount(FinalPromptCount,FinisherRound,FinisherRounds)))
    {
        if(Director) Director->Destroy();
        HandleEncounterFailure();
    }
    UpdateMusic();
}

bool AQuasicomboBoss::HandleEncounterFailure()
{
	if (Phase == EQuasicomboBossPhase::Waiting || Phase == EQuasicomboBossPhase::AwaitPickup)
		return false;
	if (Phase == EQuasicomboBossPhase::Retry || Phase == EQuasicomboBossPhase::Complete)
		return true;
	Evolution->Cancel();
	bOpeningCharge = false;
	if(auto* Run=GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>(); Run&&Run->GetQTEDirector())
        Run->GetQTEDirector()->Destroy();
    else QTE->CancelQTE();
	bTailActive = false;
	SuspendSideCombat();
	RestoreCameraAndTime();
	Quantum->FreezeForFinisher();
	++Failures;
	if (Failures >= 2)
	{
		Phase = EQuasicomboBossPhase::Complete;
		if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
			Run->EndRun(EQuasicomboRunOutcome::Defeat);
		return true;
	}
	Phase = EQuasicomboBossPhase::Retry;
	UpdateMusic();
	GetCharacterMovement()->DisableMovement();
	if (ASideScrollingCharacter* Player = EncounterPlayer.Get())
	{
		Player->PrepareForFinisher();
		Player->GetCharacterMovement()->DisableMovement();
	}
	RetryResumeAt = FPlatformTime::Seconds() + 1.0;
	bOwnsRetryPause = !UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this, true);
	return true;
}

void AQuasicomboBoss::RestoreRetryPause()
{
	RetryResumeAt = 0.0;
	if (bOwnsRetryPause)
	{
		bOwnsRetryPause = false;
		if (GetWorld()) UGameplayStatics::SetGamePaused(this, false);
	}
}

void AQuasicomboBoss::RestartEncounter()
{
	if (Phase != EQuasicomboBossPhase::Retry)
		return;
	RestoreRetryPause();
    RestoreCinematicRagdoll();
	Evolution->Cancel();
	Evolution->ClearDebris();
	bOpeningCharge = false;
	SetActorTransform(BossStartTransform, false, nullptr, ETeleportType::TeleportPhysics);
	BaseHitsRemaining = BaseHealthHits;
	ArmorHitsRemaining = AwardedArmorBars * HitsPerArmorBar;
	SetResistance(BossResistanceForArmorBars(GetArmorBars()));
	GetMesh()->bPauseAnims = false;
	ResetSideCombat(BaseHealthHits + ArmorHitsRemaining);
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetPhysicsBlendWeight(0.0f);
	Quantum->ResumeAfterRetry();
	if (bHasEvolved)
		RevealEvolutionEyes();
	if (ASideScrollingCharacter *Player = EncounterPlayer.Get())
		Player->ResetForBossRetry(PlayerStartTransform);
	Phase = EQuasicomboBossPhase::Combat;
	UpdateHealthBar();
	ApplyQuantumTuning(0.0, GetEffectiveTau(), Quantum->UsedFallback());
	EncounterLighting->BeginCombat();
}

void AQuasicomboBoss::HandleQTEResult(bool bSucceeded)
{
	if (!bSucceeded) { HandleEncounterFailure(); return; }
	Phase = EQuasicomboBossPhase::AwaitPickup;
	// A lingering attack montage would override the death sequence in the slot.
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->StopAllMontages(0.0f);
	GetCharacterMovement()->StopMovementImmediately();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UpdateHealthBar();
	EvolutionGlow->SetVisibility(false);
	MusicA->FadeOut(0.8f, 0.0f);
	MusicB->FadeOut(0.8f, 0.0f);
	if (auto* Mode = Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(this)))
		Mode->MusicController->StopMusic(0.8f);
	// Let the complete death clip play before revealing the reward and
	// handing the still-visible body to ragdoll physics.
	const float FallSeconds = DefeatAnimation ? DefeatAnimation->GetPlayLength() : 1.5f;
	GetWorldTimerManager().SetTimer(VictoryPickupTimer, this, &AQuasicomboBoss::SpawnVictoryPickup,
		FMath::Max(0.2f, FallSeconds + 0.1f), false);
}

void AQuasicomboBoss::SpawnVictoryPickup()
{
	if (Phase != EQuasicomboBossPhase::AwaitPickup || !GetWorld() || IsValid(VictoryPickup)) return;
	// The completed death animation always hands off to ordinary gravity-driven
	// physics, including when no equipment reward is needed.
	if (!GetMesh()->IsSimulatingPhysics()) BeginCinematicRagdoll();
	if (auto* Player = EncounterPlayer.Get(); Player && Player->GetBusterCannon()->IsEquipped())
	{
		if (auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
			Run->EndRun(EQuasicomboRunOutcome::Victory);
		return;
	}
	FVector Position = GetActorLocation();
	if (ASideScrollingCharacter* Player = EncounterPlayer.Get())
		Position.X += Player->GetActorLocation().X < Position.X ? 110.0f : -110.0f;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Query;
	Query.AddIgnoredActor(this);
	FHitResult Ground;
	if (GetWorld()->LineTraceSingleByObjectType(Ground, Position + FVector(0, 0, 120),
		Position - FVector(0, 0, 450), Objects, Query)) Position.Z = Ground.ImpactPoint.Z + 75.0f;
	else Position.Z -= GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 75.0f;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	VictoryPickup = GetWorld()->SpawnActor<AQuasicomboVictoryPickup>(Position, FRotator::ZeroRotator, Spawn);
	if (VictoryPickup)
	{
		VictoryPickup->Initialize(this, EncounterPlayer.Get());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Quasicombo: victory pickup failed to spawn"));
		GetWorldTimerManager().SetTimer(VictoryPickupTimer, this, &AQuasicomboBoss::SpawnVictoryPickup, 0.5f, false);
	}
}

bool AQuasicomboBoss::ClaimVictory()
{
	if (Phase != EQuasicomboBossPhase::AwaitPickup || !IsValid(VictoryPickup)) return false;
	UQuasicomboRunSubsystem* Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (!Run || Run->GetOutcome() != EQuasicomboRunOutcome::Playing) return false;
	Run->EndRun(EQuasicomboRunOutcome::Victory);
	return Run->GetOutcome() == EQuasicomboRunOutcome::Victory;
}

void AQuasicomboBoss::HandleRunEnded(EQuasicomboRunOutcome Outcome)
{
	// Victory is recorded before Mike's pickup cinematic starts. Keep its dark
	// lighting through the reward presentation; EndPlay restores it on level exit.
	if (Outcome != EQuasicomboRunOutcome::Victory) EncounterLighting->Restore();
	Evolution->Cancel();
	Evolution->ClearDebris();
	bOpeningCharge = false;
	Phase = EQuasicomboBossPhase::Complete;
	RestoreRetryPause();
	GetWorldTimerManager().ClearTimer(VictoryPickupTimer);
	if (Outcome == EQuasicomboRunOutcome::Defeat && IsValid(VictoryPickup)) VictoryPickup->Destroy();
	bTailActive = false;
	SuspendSideCombat();
	QTE->CancelQTE();
	UpdateHealthBar();
	Quantum->FreezeForFinisher();
	RestoreCameraAndTime();
	MusicA->FadeOut(1.0f, 0.0f);
	MusicB->FadeOut(1.0f, 0.0f);
}

void AQuasicomboBoss::FreezePlayerAndCamera(bool bFinisher)
{
	ASideScrollingCharacter* Player = EncounterPlayer.Get();
	if (!Player) Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Player)
	{
		EncounterPlayer = Player;
		Player->PrepareForFinisher();
		PreviousMovementMode = Player->GetCharacterMovement()->MovementMode;
		Player->GetCharacterMovement()->DisableMovement();
		if (!bFinisher) Player->SetEvolutionComboHeld(true);
		bMovementFrozen = true;
	}
	if (APlayerController *PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PreviousTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
		PreviousViewTarget = PC->GetViewTarget();
		bCameraOverride = true;
		if (bFinisher) UGameplayStatics::SetGlobalTimeDilation(this, FinisherTimeDilation);
		ACameraActor* Camera = bFinisher ? CloseupCamera : nullptr;
		FrameCamera(Camera, !bFinisher);
		if (Camera) PC->SetViewTargetWithBlend(Camera, 0.2f);
	}
}

void AQuasicomboBoss::FrameCamera(ACameraActor* Camera, bool bImpact)
{
	if (!Camera) return;
	const FVector BossCenter = GetActorLocation() + FVector(0, 0, 35);
	const FVector PlayerCenter = EncounterPlayer.IsValid() ? EncounterPlayer->GetActorLocation() : BossCenter;
	const FVector Target = (BossCenter + PlayerCenter) * 0.5f;
	const float Separation = FMath::Abs(BossCenter.X - PlayerCenter.X);
	const FVector Eye = Target + FVector(bImpact ? 130 : -100, FMath::Max(bImpact ? 650.0f : 500.0f, Separation * 1.2f), bImpact ? 190 : 90);
	Camera->SetActorLocation(Eye);
	Camera->SetActorRotation((Target - Eye).Rotation());
	Camera->GetCameraComponent()->SetFieldOfView(bImpact ? 55.0f : 48.0f);
}

void AQuasicomboBoss::ShowImpactCamera()
{
	if (!bCameraOverride) return;
	ACameraActor* Camera = QTE->GetPromptNumber() == 1 && !QTE->IsFinishingBeat() ? CloseupCamera : ImpactCamera;
	FrameCamera(Camera, Camera == ImpactCamera);
	if (Camera)
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetViewTargetWithBlend(Camera, 0.15f);
}

void AQuasicomboBoss::RestoreCameraAndTime(float CameraBlendSeconds)
{
	RestoreRetryPause();
	if(bEvolutionPlayerHidden)
	{
		bEvolutionPlayerHidden=false;
		if(auto* Player=EncounterPlayer.Get()) Player->SetActorHiddenInGame(bPlayerWasHidden);
	}
	if (bMovementFrozen)
	{
		bMovementFrozen = false;
		if (ASideScrollingCharacter* Player = EncounterPlayer.Get())
		{
			Player->SetEvolutionComboHeld(false);
			Player->GetCharacterMovement()->SetMovementMode(PreviousMovementMode);
		}
	}
	if (!bCameraOverride || !GetWorld()) return;
	bCameraOverride = false;
	UGameplayStatics::SetGlobalTimeDilation(this, PreviousTimeDilation);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		AActor* Target = PreviousViewTarget.Get();
		if (!IsValid(Target)) Target = PC->GetPawn();
		if (IsValid(Target)) PC->SetViewTargetWithBlend(Target, CameraBlendSeconds);
	}
	PreviousViewTarget.Reset();
}

bool AQuasicomboBoss::SubmitQTEInput(EQuasicomboQTEInput Input)
{
	return QTE && QTE->SubmitPrompt(Input);
}
int32 AQuasicomboBoss::GetArmorBars() const
{
	return FMath::DivideAndRoundUp(ArmorHitsRemaining, FMath::Max(1, HitsPerArmorBar));
}
float AQuasicomboBoss::GetBaseHealthFraction() const
{
	return float(BaseHitsRemaining) / FMath::Max(1, BaseHealthHits);
}
float AQuasicomboBoss::GetArmorFraction(int32 Index) const
{
	return FMath::Clamp(float(ArmorHitsRemaining - Index * HitsPerArmorBar) / FMath::Max(1, HitsPerArmorBar), 0.0f, 1.0f);
}

void AQuasicomboBoss::UpdateAppearance()
{
	const int32 Skin = bSkinCaptured ? StartingSkin : QuasicomboBossRules::SkinPalette(GetEffectiveTau());
	if (SkinMaterials.IsValidIndex(Skin) && SkinMaterials[Skin])
		GetMesh()->SetMaterial(0, SkinMaterials[Skin]);
	if (!bEyesRevealed)
		GetMesh()->SetMaterial(1, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Lizardman_Berserker/Materials/M_Eyes_01")));
	if (Phase != EQuasicomboBossPhase::Evolution)
		for (int32 I = 0; I < ArmorPieces.Num(); ++I)
			ArmorPieces[I]->SetVisibility(GetArmorBars() > 0 && (ArmorInGroup(I, 0) || (GetArmorBars() == 2 && ArmorInGroup(I, 1))));
}

void AQuasicomboBoss::UpdateHealthBar()
{
	if (LifeBarWidget && MaxHP > 0.0f)
		LifeBarWidget->SetLifePercentage(GetBaseHealthFraction());
	if (UWidgetComponent* Bar = FindComponentByClass<UWidgetComponent>())
		Bar->SetHiddenInGame(true);
}

void AQuasicomboBoss::UpdateMusic()
{
	if (auto* Mode = Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (Phase == EQuasicomboBossPhase::Waiting || Phase == EQuasicomboBossPhase::AwaitPickup ||
			Phase == EQuasicomboBossPhase::Complete) return;
		const int32 Tier = FMath::Max(QuasicomboBossRules::AggressionTier(GetEffectiveTau()), GetArmorBars());
		USoundBase* Override = Tier == 0 ? LowQuantumJazz.Get() : Tier == 1 ? MediumQuantumJazz.Get() : HighQuantumJazz.Get();
		if (Phase == EQuasicomboBossPhase::QTE && QTEMusic) Override = QTEMusic;
		if (!Override) Override = OriginalMusic;
		const auto MusicState = Phase == EQuasicomboBossPhase::Evolution ? EQuasicomboMusicState::Evolution :
			Phase == EQuasicomboBossPhase::QTE ? EQuasicomboMusicState::QTE :
			Phase == EQuasicomboBossPhase::Retry ? EQuasicomboMusicState::Retry : EQuasicomboMusicState::Combat;
		Mode->MusicController->SetEncounterState(MusicState, Override);
		return;
	}
	USoundBase* Desired = OriginalMusic;
	if (Phase != EQuasicomboBossPhase::Waiting)
	{
		const int32 Tier = FMath::Max(QuasicomboBossRules::AggressionTier(GetEffectiveTau()), GetArmorBars());
		Desired = Tier == 0 ? LowQuantumJazz : Tier == 1 ? MediumQuantumJazz : HighQuantumJazz;
		if (Phase == EQuasicomboBossPhase::QTE && QTEMusic) Desired = QTEMusic;
		if (!Desired) Desired = CurrentMusic ? CurrentMusic.Get() : OriginalMusic.Get();
	}
	if (!Desired || Desired == CurrentMusic || Phase == EQuasicomboBossPhase::AwaitPickup || Phase == EQuasicomboBossPhase::Complete) return;
	UAudioComponent* Outgoing = bUseMusicA ? MusicA : MusicB;
	bUseMusicA = !bUseMusicA;
	UAudioComponent* Incoming = bUseMusicA ? MusicA : MusicB;
	Incoming->Stop();
	Incoming->SetSound(Desired);
	const float Duration = Desired->GetDuration();
	const float Offset = Duration > 0.0f && Duration < 100000.0f ? FMath::Fmod(MusicElapsed, Duration) : 0.0f;
	Incoming->FadeIn(1.0f, 0.7f, Offset);
	Outgoing->FadeOut(1.0f, 0.0f);
	CurrentMusic = Desired;
}

FString AQuasicomboBoss::GetEncounterStatus() const
{
	if (Phase == EQuasicomboBossPhase::Waiting)
		return FString();
	return FString::Printf(TEXT("Lizardman Berserker | Braid Aggression (Tau): %.0f%%"), GetEffectiveTau() * 100);
}

FString AQuasicomboBoss::GetEvolutionCue() const
{
	return FString();
}

bool AQuasicomboBoss::ShouldUseChargedSideAttack() const
{
	if(bOpeningPunch) return false;
	return Phase == EQuasicomboBossPhase::Combat && TailAttackAnimation && FMath::FRand() < GetEffectiveTau();
}

bool AQuasicomboBoss::StartCustomSideAttack(bool bCharged)
{
	if (!bCharged || !TailAttackAnimation || !GetMesh()->GetAnimInstance()) return false;
	bTailActive = true;
	bTailHit = false;
	PreviousTailPoints.Reset();
	bIsAttacking = true;
	SuppressFallbackAttackTrace();
	TailRate = 1.0f / FMath::Max(0.7f, ChargedAttackWindup);
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	TailMontage = Anim->PlaySlotAnimationAsDynamicMontage(TailAttackAnimation, TEXT("DefaultSlot"), 0.12f, 0.18f, TailRate);
	if (!TailMontage) { bTailActive = false; bIsAttacking = false; return false; }
	FOnMontageEnded Ended;
	Ended.BindUObject(this, &AQuasicomboBoss::EndTailAttack);
	Anim->Montage_SetEndDelegate(Ended, TailMontage);
	return true;
}

void AQuasicomboBoss::EndTailAttack(UAnimMontage*, bool)
{
	bTailActive = false;
	bIsAttacking = false;
	FinishSideAttack();
}

void AQuasicomboBoss::DoAttackTrace(FName Bone)
{
	if (bTailActive || Phase != EQuasicomboBossPhase::Combat) return;
	Super::DoAttackTrace(Bone);
}

void AQuasicomboBoss::UpdateTailAttack()
{
	if (Phase != EQuasicomboBossPhase::Combat || CombatState != ESideCombatState::Attack) return;
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!Anim || !TailMontage) return;
	const float Position = Anim->Montage_GetPosition(TailMontage);
	TArray<FVector> Points;
	for (int32 Index = 1; Index <= 7; ++Index)
		Points.Add(GetMesh()->GetSocketLocation(FName(*FString::Printf(TEXT("u_Tail_%02d"), Index))));
	if (!bTailHit && Position >= 0.70f && Position <= 1.20f)
	{
		FCollisionObjectQueryParams Objects(ECC_Pawn);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(QuantumTailSweep), false, this);
		for (int32 Index = 0; Index < Points.Num() && !bTailHit; ++Index)
		{
			const FVector Starts[] = { Index > 0 ? Points[Index-1] : Points[Index], PreviousTailPoints.IsValidIndex(Index) ? PreviousTailPoints[Index] : Points[Index] };
			for (const FVector& Start : Starts)
			{
				TArray<FHitResult> Hits;
				GetWorld()->SweepMultiByObjectType(Hits, Start, Points[Index], FQuat::Identity, Objects, FCollisionShape::MakeSphere(26.0f), Params);
				for (const FHitResult& Hit : Hits)
				{
					ASideScrollingCharacter *Player = Cast<ASideScrollingCharacter>(Hit.GetActor());
					if (!Player || bTailHit)
						continue;
					bTailHit = true;
					Player->ApplyDamage(TailDamage, this, Hit.ImpactPoint, GetActorForwardVector() * 220.0f + FVector::UpVector * 200.0f);
				}
			}
		}
	}
	PreviousTailPoints = MoveTemp(Points);
}

void AQuasicomboBoss::EndPlay(const EEndPlayReason::Type Reason)
{
	EncounterLighting->Restore();
	Evolution->Cancel(); Evolution->ClearDebris();
	RestoreRetryPause();
	if(!QTE||!Cast<AQuasicomboQTEDirector>(QTE->GetOwner())) RestoreCameraAndTime();
	Super::EndPlay(Reason);
}



void AQuasicomboBoss::SelectQTEPreview(int32 Count)
{
#if WITH_EDITORONLY_DATA
    if(!GetWorld()||GetWorld()->WorldType!=EWorldType::PIE||Phase==EQuasicomboBossPhase::QTE||bOutcomeSampled) return;
    bEnableQTEPreview=true; PreviewQTEPrompts=FMath::Clamp(Count,2,6);
    if(Count==6)
        if(auto* Player=Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this,0))) Player->GetBusterCannon()->Equip();
    UE_LOG(LogTemp,Display,TEXT("QTE preview: final %d prompts, %d rounds; deplete boss health to start"),
        PreviewQTEPrompts,FMath::Clamp(PreviewQTERounds,1,3));
#endif
}


void AQuasicomboBoss::ResetAfterQTERound(bool bSucceeded)
{
    RestoreCinematicRagdoll();
    Evolution->ClearDebris();
    bOpeningCharge=false;
    if(bSucceeded) FinisherRound=FMath::Min(FinisherRound+1,FinisherRounds);
    BaseHitsRemaining=BaseHealthHits; ArmorHitsRemaining=AwardedArmorBars*HitsPerArmorBar;
    SetResistance(BossResistanceForArmorBars(GetArmorBars()));
    bTailActive=false;
    ResetSideCombat(BaseHealthHits+ArmorHitsRemaining);
    Phase=EQuasicomboBossPhase::Combat;
    UpdateAppearance(); UpdateHealthBar(); UpdateMusic();
}


void AQuasicomboBoss::BeginCinematicRagdoll(bool bControlledFlight)
{
    GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->DisableMovement();
    auto* RagdollMesh=GetMesh();
    // Enable the mesh's physics collision before recreating its body instances.
    SetActorEnableCollision(true);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RagdollMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    RagdollMesh->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    RagdollMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if(!CinematicPhysicsAsset&&RagdollMesh->GetPhysicsAsset())
    {
        RagdollMeshRelative=RagdollMesh->GetRelativeTransform();
        OriginalPhysicsAsset=RagdollMesh->GetPhysicsAsset();
        // The imported bodies overlap in the final death pose. Keep constraints and
        // world contacts, but remove self-collision impulses in this actor's copy.
        CinematicPhysicsAsset=DuplicateObject<UPhysicsAsset>(OriginalPhysicsAsset,this);
        for(int32 A=0;A<CinematicPhysicsAsset->SkeletalBodySetups.Num();++A)
            for(int32 B=A+1;B<CinematicPhysicsAsset->SkeletalBodySetups.Num();++B)
                CinematicPhysicsAsset->DisableCollision(A,B);
        RagdollMesh->SetPhysicsAsset(CinematicPhysicsAsset,true);
    }
    // Preserve evaluated death/contact bones before changing slot ownership.
    RagdollMesh->bPauseAnims=true;
    RagdollMesh->SetSimulatePhysics(true); RagdollMesh->SetPhysicsBlendWeight(1);
    RagdollMesh->SetEnableGravity(!bControlledFlight);
    RagdollMesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    RagdollMesh->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    for(FBodyInstance* Body:RagdollMesh->Bodies) if(Body)
    {
        Body->LinearDamping=1.5f; Body->AngularDamping=bControlledFlight?.4f:3.f;
        Body->UpdateDampingProperties();
        Body->SetMaxDepenetrationVelocity(100.f);
    }
    RagdollMesh->WakeAllRigidBodies();
}


void AQuasicomboBoss::RestoreCinematicRagdoll()
{
    if(!CinematicPhysicsAsset) return;
    auto* RagdollMesh=GetMesh();
    RagdollMesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
    RagdollMesh->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    RagdollMesh->SetSimulatePhysics(false); RagdollMesh->SetPhysicsBlendWeight(0);
    RagdollMesh->AttachToComponent(GetCapsuleComponent(),FAttachmentTransformRules::KeepWorldTransform);
    RagdollMesh->SetRelativeTransform(RagdollMeshRelative);
    RagdollMesh->SetPhysicsAsset(OriginalPhysicsAsset,true);
    RagdollMesh->SetAllBodiesSimulatePhysics(false);
    RagdollMesh->SetEnableGravity(true); RagdollMesh->bPauseAnims=false;
    RagdollMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CinematicPhysicsAsset=nullptr; OriginalPhysicsAsset=nullptr;
}


void AQuasicomboBoss::FinishCinematicDefeat()
{
    Phase=EQuasicomboBossPhase::AwaitPickup; bCinematicDefeated=true;
    BeginCinematicRagdoll();
    MusicA->FadeOut(.8f,0); MusicB->FadeOut(.8f,0);
    if(auto* Mode=Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(this)))
        Mode->MusicController->StopMusic(.8f);
    UpdateHealthBar();
}
