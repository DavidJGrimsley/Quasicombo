#include "QuasicomboBoss.h"
#include "QuasicomboBossRules.h"
#include "QuasicomboBossAnimInstance.h"
#include "QuasicomboVictoryPickup.h"
#include "CombatLifeBar.h"
#include "QuasicomboRunSubsystem.h"
#include "QuantumApiSettings.h"
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
}

AQuasicomboBoss::AQuasicomboBoss()
{
	// Ground correction must see the pose produced by this frame's animation.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	RequiredHits = 4;
	MaxHP = 4.0f;
	AggroRange = 800.0f;
	AttackRange = 160.0f;
	MeleeTraceDistance = 150.0f;
	AttackTimeout = 3.5f;
	MeleeDamage = 2.0f;
	TailDamage = 4.0f;
	SetResistance(BossResistanceForArmorBars(0));
	Quantum = CreateDefaultSubobject<UQuantumBossComponent>(TEXT("QuantumState"));
	QTE = CreateDefaultSubobject<UQuasicomboQTEComponent>(TEXT("FinalQTE"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Lizardman_Berserker/Mesh/SeparatedMesh/SK_Body"));
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
	for (const TCHAR* Name : { TEXT("MI_Body_Skin_01_Inst"), TEXT("MI_Body_Skin_06_Inst"), TEXT("MI_Body_Skin_12_Inst") })
	{
		ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(*(FString(TEXT("/Game/Lizardman_Berserker/Materials/")) + Name));
		SkinMaterials.Add(Material.Object);
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

void AQuasicomboBoss::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -87), FRotator(0, -90, 0));
	for (USkeletalMeshComponent* Part : ArmorPieces) Part->SetLeaderPoseComponent(GetMesh());
	UpdateAppearance();
}

void AQuasicomboBoss::BeginPlay()
{
	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -87), FRotator(0, -90, 0));
	BaseHealthHits = FMath::Max(4, BaseHealthHits);
	HitsPerArmorBar = FMath::Max(1, HitsPerArmorBar);
	RequiredHits = BaseHealthHits;
	BaseHitsRemaining = BaseHealthHits;
	SetResistance(BossResistanceForArmorBars(0));
	Super::BeginPlay();
	BossStartTransform = GetActorTransform();
	for (USkeletalMeshComponent* Part : ArmorPieces) Part->SetLeaderPoseComponent(GetMesh());
	if (UWidgetComponent* Bar = FindComponentByClass<UWidgetComponent>())
	{
		Bar->SetRelativeLocation(FVector(0, 0, 95));
		Bar->SetHiddenInGame(true);
	}
	Quantum->OnQuantumStateReady.AddDynamic(this, &AQuasicomboBoss::ApplyQuantumTuning);
	Quantum->OnQuantumUpdated.AddDynamic(this, &AQuasicomboBoss::HandleQuantumUpdate);
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
		Run->OnRunEnded.AddDynamic(this, &AQuasicomboBoss::HandleRunEnded);
	if (UMaterialInterface* Eyes = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Lizardman_Berserker/Materials/M_GlowEyes")))
		EyeMaterial = GetMesh()->CreateDynamicMaterialInstance(1, Eyes);
	ApplyQuantumTuning(Quantum->GetVacuumProbability(), Quantum->GetTauProbability(), Quantum->UsedFallback());
	UpdateMusic();
}

void AQuasicomboBoss::StartEncounter(ASideScrollingCharacter* Player)
{
	if (Phase != EQuasicomboBossPhase::Waiting) return;
	const UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	if (!Run || Run->GetOutcome() != EQuasicomboRunOutcome::Playing) return;
	EncounterPlayer = Player;
	if (Player) PlayerStartTransform = Player->GetActorTransform();
	BossStartTransform = GetActorTransform();
	Phase = EQuasicomboBossPhase::Combat;
	ResetSideCombat(BaseHealthHits);
	UpdateHealthBar();
	if (UQuasicomboRunSubsystem* MutableRun = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
		MutableRun->RegisterBossEncounter(this);
	ApplyQuantumTuning(Quantum->GetVacuumProbability(), GetEffectiveTau(), Quantum->UsedFallback());
}

void AQuasicomboBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CorrectVisualGrounding(DeltaSeconds);
	if (bCorpseSettling) UpdateCorpseSettle();
	// The arena transfer is normally responsible for starting combat. If the
	// player reaches the lizard without crossing that small trigger, proximity
	// must still make the placed boss fightable.
	if (Phase == EQuasicomboBossPhase::Waiting)
	{
		ASideScrollingCharacter* Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (Player && Player->GetCurrentHP() > 0.0f)
		{
			const FVector Offset = Player->GetActorLocation() - GetActorLocation();
			if (FMath::Abs(Offset.X) <= 650.0f && FMath::Abs(Offset.Y) <= 180.0f && FMath::Abs(Offset.Z) <= 220.0f)
				StartEncounter(Player);
		}
	}
	MusicElapsed += FApp::GetDeltaTime();
	if (Phase == EQuasicomboBossPhase::Evolution)
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - PhaseStartedAt;
		EvolutionGlow->SetIntensity(5500.0f + 2500.0f * FMath::Sin(static_cast<float>(Elapsed) * 13.0f));
		if (Elapsed >= EvolutionDeadlineSeconds) Quantum->TimeoutEvolution();
		if (Quantum->HasEvolutionResolved() && Elapsed >= EvolutionMinimumSeconds) FinishEvolution();
	}
	if (bTailActive) UpdateTailAttack();
	// SoundWave slots can be looping or one-shot; both remain continuous.
	if (CurrentMusic && Phase != EQuasicomboBossPhase::AwaitPickup && Phase != EQuasicomboBossPhase::Complete)
	{
		UAudioComponent* Active = bUseMusicA ? MusicA : MusicB;
		if (!Active->IsPlaying()) Active->Play(0.0f);
	}
}

void AQuasicomboBoss::CorrectVisualGrounding(float DeltaSeconds)
{
	if (!GetWorld() || GetMesh()->IsSimulatingPhysics() || IsHidden() ||
		Phase == EQuasicomboBossPhase::AwaitPickup || Phase == EQuasicomboBossPhase::Complete) return;
	// Retargeted attacks move the legs relative to the root. The ankle bones
	// are about 20 cm above this mesh's rendered claws, so track the lower
	// ball bone instead of letting the toes pass through the floor.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Query;
	Query.AddIgnoredActor(this);
	FHitResult Ground;
	const FVector Center = GetActorLocation();
	if (!GetWorld()->LineTraceSingleByObjectType(Ground, Center + FVector(0, 0, 120),
		Center - FVector(0, 0, 450), Objects, Query) || Ground.ImpactNormal.Z < 0.65f) return;
	const float LowBall = FMath::Min(GetMesh()->GetSocketLocation(TEXT("ball_l")).Z,
		GetMesh()->GetSocketLocation(TEXT("ball_r")).Z);
	// The imported body extends about 17 local cm below the ball bones.
	const float ClawClearance = 22.0f * GetActorScale3D().Z * GetMesh()->GetRelativeScale3D().Z;
	const float Error = Ground.ImpactPoint.Z + ClawClearance - LowBall;
	if (FMath::Abs(Error) < 1.0f) return;
	FVector Offset = GetMesh()->GetRelativeLocation();
	// Never push the mesh below its grounded Blueprint pose just because an
	// attack lifts a leg; that pose would bury the claws when the montage ends.
	const float TargetZ = FMath::Clamp(Offset.Z + Error, -87.0f, -60.0f);
	// Correct penetration immediately; lower a raised pose smoothly.
	Offset.Z = Error > 0.0f ? TargetZ : FMath::FInterpTo(Offset.Z, TargetZ, DeltaSeconds, 9.0f);
	GetMesh()->SetRelativeLocation(Offset);
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
			if (FMath::Abs(Offset.X) <= 650.0f && FMath::Abs(Offset.Y) <= 180.0f && FMath::Abs(Offset.Z) <= 220.0f)
				StartEncounter(Player);
		}
	}
	const int32 Before = AcceptedStrikes;
	Super::ApplyDamage(Damage, DamageCauser, Location, Impulse);
	if (AcceptedStrikes != Before && bTailActive && DidLastStrikeTriggerHitReaction())
	{
		bTailActive = false;
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.08f, TailMontage);
	}
}

void AQuasicomboBoss::ResolveAcceptedStrike(AActor* Causer, const FVector& Location, const FVector& Impulse)
{
	if (ArmorHitsRemaining > 0) --ArmorHitsRemaining;
	else BaseHitsRemaining = FMath::Max(0, BaseHitsRemaining - 1);
	SetResistance(BossResistanceForArmorBars(GetArmorBars()));
	if (BaseHitsRemaining == 0 && ArmorHitsRemaining == 0)
	{
		HandleRequiredHitsReached(Causer, Location, Impulse);
	}
	else
	{
		// Keep total HP/effects in sync. A resisted strike still damages the boss,
		// but does not cancel its attack or force the authored stagger animation.
		ApplyAcceptedStrikeDamage(1.0f, Causer, Location, FVector::ZeroVector);
		GetMesh()->SetPhysicsBlendWeight(0.0f);
		CurrentHP = static_cast<float>(BaseHitsRemaining + ArmorHitsRemaining);
		if (DidLastStrikeTriggerHitReaction())
		{
			GetCharacterMovement()->StopMovementImmediately();
			if (UAnimInstance* Anim = GetMesh()->GetAnimInstance(); Anim && HitReactionAnimation)
				Anim->PlaySlotAnimationAsDynamicMontage(HitReactionAnimation, TEXT("DefaultSlot"), 0.06f, 0.12f);
		}
	}
	UpdateAppearance();
	UpdateHealthBar();
	UpdateMusic();
}

void AQuasicomboBoss::OnStrikeAccepted(int32 StrikeCount)
{
	if (StrikeCount != 3 || bHasEvolved || Phase != EQuasicomboBossPhase::Combat) return;
	EvolutionBeforeTau = GetEffectiveTau();
	Phase = EQuasicomboBossPhase::Evolution;
	EvolutionGlow->SetVisibility(true);
	PhaseStartedAt = GetWorld()->GetTimeSeconds();
	const float ApiTimeout = Quantum->bUseLiveQuantumApi ? GetDefault<UQuantumApiSettings>()->RequestTimeoutSeconds + 1.0f : 0.0f;
	EvolutionDeadlineSeconds = FMath::Max3(EvolutionMinimumSeconds, EvolutionTimeoutSeconds, ApiTimeout);
	bTailActive = false;
	SuspendSideCombat();
	GetCharacterMovement()->DisableMovement();
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance(); Anim && HitReactionAnimation)
		Anim->PlaySlotAnimationAsDynamicMontage(HitReactionAnimation, TEXT("DefaultSlot"), 0.06f, 0.12f);
	FreezePlayerAndCamera(false);
	Quantum->EvolveState();
}

void AQuasicomboBoss::FinishEvolution()
{
	if (bHasEvolved || Phase != EQuasicomboBossPhase::Evolution) return;
	bHasEvolved = true;
	EvolutionAfterTau = GetEffectiveTau();
	bEvolutionUsedFallback = Quantum->UsedFallback();
	AwardedArmorBars = QuasicomboBossRules::ArmorBars(Quantum->GetTauProbability() - Quantum->GetPreEvolutionTau(),
		FMath::Max(0.0, FirstArmorThreshold), FMath::Max(FirstArmorThreshold, SecondArmorThreshold));
#if WITH_EDITORONLY_DATA
	if (GetWorld()->WorldType == EWorldType::PIE && PreviewArmorBars >= 0)
		AwardedArmorBars = FMath::Clamp(PreviewArmorBars, 0, 2);
#endif
	ArmorHitsRemaining = AwardedArmorBars * HitsPerArmorBar;
	SetResistance(BossResistanceForArmorBars(GetArmorBars()));
	RequiredHits = BaseHealthHits + ArmorHitsRemaining;
	MaxHP = static_cast<float>(RequiredHits);
	CurrentHP = static_cast<float>(BaseHitsRemaining + ArmorHitsRemaining);
	EvolutionGlow->SetVisibility(false);
	EvolutionResultUntil = GetWorld()->GetTimeSeconds() + 3.0f;
	RestoreCameraAndTime();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Phase = EQuasicomboBossPhase::Combat;
	UpdateAppearance();
	UpdateHealthBar();
	UpdateMusic();
}

void AQuasicomboBoss::HandleRequiredHitsReached(AActor*, const FVector&, const FVector&)
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
	if (Phase == EQuasicomboBossPhase::QTE || Phase == EQuasicomboBossPhase::Complete) return;
	const UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	if (!Run || Run->GetOutcome() != EQuasicomboRunOutcome::Playing) return;
	Phase = EQuasicomboBossPhase::QTE;
	UpdateHealthBar();
	Quantum->FreezeForFinisher();
	if (!bOutcomeSampled)
	{
		bOutcomeSampled = true;
		bTauOutcome = FMath::FRand() < GetEffectiveTau();
		FinisherRounds = QuasicomboBossRules::QTERounds(GetEffectiveTau());
#if WITH_EDITORONLY_DATA
		if (GetWorld()->WorldType == EWorldType::PIE)
		{
			if (PreviewQTEOutcome >= 0) bTauOutcome = PreviewQTEOutcome == 1;
			if (PreviewQTERounds > 0) FinisherRounds = FMath::Clamp(PreviewQTERounds, 1, 3);
		}
#endif
	}
	FreezePlayerAndCamera(true);
	OnFinisherStarted(bTauOutcome);
	QTE->BeginQTE(bTauOutcome, FinisherRounds);
	UpdateMusic();
}

bool AQuasicomboBoss::HandleEncounterFailure()
{
	if (Phase == EQuasicomboBossPhase::Waiting || Phase == EQuasicomboBossPhase::AwaitPickup) return false;
	if (Phase == EQuasicomboBossPhase::Retry || Phase == EQuasicomboBossPhase::Complete) return true;
	QTE->CancelQTE();
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
	GetCharacterMovement()->DisableMovement();
	if (ASideScrollingCharacter* Player = EncounterPlayer.Get())
	{
		Player->PrepareForFinisher();
		Player->GetCharacterMovement()->DisableMovement();
	}
	GetWorldTimerManager().SetTimer(RetryTimer, this, &AQuasicomboBoss::RestartEncounter, FMath::Max(0.01f, RetryDelaySeconds), false);
	return true;
}

void AQuasicomboBoss::RestartEncounter()
{
	if (Phase != EQuasicomboBossPhase::Retry) return;
	SetActorTransform(BossStartTransform, false, nullptr, ETeleportType::TeleportPhysics);
	BaseHitsRemaining = BaseHealthHits;
	ArmorHitsRemaining = AwardedArmorBars * HitsPerArmorBar;
	SetResistance(BossResistanceForArmorBars(GetArmorBars()));
	bCorpseSettling = false;
	GetMesh()->bPauseAnims = false;
	ResetSideCombat(BaseHealthHits + ArmorHitsRemaining);
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetPhysicsBlendWeight(0.0f);
	Quantum->ResumeAfterRetry();
	if (ASideScrollingCharacter* Player = EncounterPlayer.Get()) Player->ResetForBossRetry(PlayerStartTransform);
	Phase = EQuasicomboBossPhase::Combat;
	UpdateHealthBar();
	ApplyQuantumTuning(0.0, GetEffectiveTau(), Quantum->UsedFallback());
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
	// Let the complete death clip play before revealing the reward and
	// handing the still-visible body to ragdoll physics.
	const float FallSeconds = DefeatAnimation ? DefeatAnimation->GetPlayLength() : 1.5f;
	GetWorldTimerManager().SetTimer(VictoryPickupTimer, this, &AQuasicomboBoss::SpawnVictoryPickup,
		FMath::Max(0.2f, FallSeconds + 0.1f), false);
}

void AQuasicomboBoss::SpawnVictoryPickup()
{
	if (Phase != EQuasicomboBossPhase::AwaitPickup || !GetWorld() || IsValid(VictoryPickup)) return;
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
		// The clip ends bent over. Let the physics asset finish the collapse,
		// then sleep it on the floor before it can drift through the platform.
		CorpseFloorZ = Ground.bBlockingHit ? Ground.ImpactPoint.Z :
			GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		CorpseSettleStartedAt = GetWorld()->GetTimeSeconds();
		GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
		GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->SetPhysicsBlendWeight(1.0f);
		bCorpseSettling = true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Quasicombo: victory pickup failed to spawn"));
		GetWorldTimerManager().SetTimer(VictoryPickupTimer, this, &AQuasicomboBoss::SpawnVictoryPickup, 0.5f, false);
	}
}

void AQuasicomboBoss::UpdateCorpseSettle()
{
	const double Elapsed = GetWorld()->GetTimeSeconds() - CorpseSettleStartedAt;
	if (Elapsed < 1.0) return;
	const float HeadZ = GetMesh()->GetSocketLocation(TEXT("head")).Z;
	if (HeadZ > CorpseFloorZ + 50.0f && Elapsed < 5.5) return;
	GetMesh()->SetEnableGravity(false);
	GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->PutAllRigidBodiesToSleep();
	bCorpseSettling = false;
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
	Phase = EQuasicomboBossPhase::Complete;
	GetWorldTimerManager().ClearTimer(RetryTimer);
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
		bMovementFrozen = true;
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PreviousTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
		PreviousViewTarget = PC->GetViewTarget();
		bCameraOverride = true;
		if (bFinisher) UGameplayStatics::SetGlobalTimeDilation(this, FinisherTimeDilation);
		ACameraActor* Camera = bFinisher ? CloseupCamera : ImpactCamera;
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

void AQuasicomboBoss::RestoreCameraAndTime()
{
	if (bMovementFrozen)
	{
		bMovementFrozen = false;
		if (ASideScrollingCharacter* Player = EncounterPlayer.Get())
			Player->GetCharacterMovement()->SetMovementMode(PreviousMovementMode);
	}
	if (!bCameraOverride || !GetWorld()) return;
	bCameraOverride = false;
	UGameplayStatics::SetGlobalTimeDilation(this, PreviousTimeDilation);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		AActor* Target = PreviousViewTarget.Get();
		if (!IsValid(Target)) Target = PC->GetPawn();
		if (IsValid(Target)) PC->SetViewTargetWithBlend(Target, 0.25f);
	}
	PreviousViewTarget.Reset();
}

bool AQuasicomboBoss::SubmitQTEInput(EQuasicomboQTEInput Input) { return QTE && QTE->SubmitPrompt(Input); }
int32 AQuasicomboBoss::GetArmorBars() const { return FMath::DivideAndRoundUp(ArmorHitsRemaining, FMath::Max(1, HitsPerArmorBar)); }
float AQuasicomboBoss::GetBaseHealthFraction() const { return float(BaseHitsRemaining) / FMath::Max(1, BaseHealthHits); }
float AQuasicomboBoss::GetArmorFraction(int32 Index) const
{
	return FMath::Clamp(float(ArmorHitsRemaining - Index * HitsPerArmorBar) / FMath::Max(1, HitsPerArmorBar), 0.0f, 1.0f);
}

void AQuasicomboBoss::UpdateAppearance()
{
	const int32 Tier = QuasicomboBossRules::AggressionTier(GetEffectiveTau());
	if (SkinMaterials.IsValidIndex(Tier) && SkinMaterials[Tier]) GetMesh()->SetMaterial(0, SkinMaterials[Tier]);
	if (EyeMaterial)
	{
		const FLinearColor Colors[] = {FLinearColor(0.05f,0.35f,1.0f), FLinearColor(1.0f,0.65f,0.04f), FLinearColor(1.0f,0.04f,0.02f)};
		EyeMaterial->SetVectorParameterValue(TEXT("Color"), Colors[Tier]);
		EyeMaterial->SetScalarParameterValue(TEXT("Emissive"), 3.0f + Tier * 2.0f);
	}
	const int32 Bars = GetArmorBars();
	for (int32 Index = 0; Index < ArmorPieces.Num(); ++Index)
	{
		const bool bVisible = Index < 4 ? Bars >= 1 : (Index == 4 ? Bars == 1 : Bars == 2);
		ArmorPieces[Index]->SetVisibility(bVisible);
	}
}

void AQuasicomboBoss::UpdateHealthBar()
{
	if (LifeBarWidget && MaxHP > 0.0f)
		LifeBarWidget->SetLifePercentage(FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f));
	if (UWidgetComponent* Bar = FindComponentByClass<UWidgetComponent>())
		Bar->SetHiddenInGame(Phase == EQuasicomboBossPhase::Waiting ||
			Phase == EQuasicomboBossPhase::QTE || Phase == EQuasicomboBossPhase::AwaitPickup || Phase == EQuasicomboBossPhase::Complete);
}

void AQuasicomboBoss::UpdateMusic()
{
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
	if (Phase == EQuasicomboBossPhase::Waiting) return FString();
	if (Phase == EQuasicomboBossPhase::Retry) return TEXT("ONE MORE CHANCE - restarting boss");
	if (Phase == EQuasicomboBossPhase::Evolution)
		return FString::Printf(TEXT("TIME EVOLUTION  |  Starting Tau %.0f%%"), EvolutionBeforeTau * 100);
	if (Phase == EQuasicomboBossPhase::QTE)
		return FString::Printf(TEXT("COLLAPSE: %s  |  %d retry remaining"), bTauOutcome ? TEXT("TAU") : TEXT("VACUUM"), GetRetriesRemaining());
	if (Phase == EQuasicomboBossPhase::AwaitPickup) return FString();
	return FString::Printf(TEXT("LIZARDMAN  |  %s Tau %.0f%%  |  %d retry remaining"), bHasEvolved ? TEXT("Evolved") : TEXT("Braid"), GetEffectiveTau()*100, GetRetriesRemaining());
}

FString AQuasicomboBoss::GetEvolutionCue() const
{
	if (Phase == EQuasicomboBossPhase::Evolution)
		return TEXT("TIME EVOLUTION\nWatch the armor, skin and eyes");
	if (Phase != EQuasicomboBossPhase::Combat || !GetWorld() ||
		GetWorld()->GetTimeSeconds() >= EvolutionResultUntil) return FString();
#if WITH_EDITORONLY_DATA
	const bool bPreview = GetWorld()->WorldType == EWorldType::PIE && PreviewTau >= 0.0;
#else
	const bool bPreview = false;
#endif
	if (!bPreview && bEvolutionUsedFallback && FMath::IsNearlyEqual(EvolutionBeforeTau, EvolutionAfterTau, 0.001) && AwardedArmorBars == 0)
	{
		const FString& Reason = Quantum->GetEvolutionFailureReason();
		if (Reason == TEXT("encounter deadline"))
			return TEXT("TIME EVOLUTION: NO CHANGE\nAPI response timed out; previous Tau kept");
		if (Reason == TEXT("transport_error"))
			return TEXT("TIME EVOLUTION: NO CHANGE\nAPI connection failed; previous Tau kept");
		if (Reason == TEXT("auth_required"))
			return TEXT("TIME EVOLUTION: NO CHANGE\nAPI key was rejected; previous Tau kept");
		if (Reason == TEXT("offline mode"))
			return TEXT("TIME EVOLUTION: NO CHANGE\nOffline mode kept the previous Tau");
		return TEXT("TIME EVOLUTION: NO CHANGE\nAPI result unavailable; previous Tau kept");
	}
	return FString::Printf(TEXT("%s: TAU %.0f%% -> %.0f%%\n+%d ARMOR %s  |  %s"),
		bPreview ? TEXT("PIE EVOLUTION PREVIEW") : TEXT("TIME EVOLUTION"), EvolutionBeforeTau * 100,
		EvolutionAfterTau * 100, AwardedArmorBars, AwardedArmorBars == 1 ? TEXT("BAR") : TEXT("BARS"),
		EvolutionAfterTau > EvolutionBeforeTau + 0.001 ? TEXT("FASTER ATTACKS") : TEXT("LOOK AT SKIN / EYES"));
}

bool AQuasicomboBoss::ShouldUseChargedSideAttack() const
{
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
					ASideScrollingCharacter* Player = Cast<ASideScrollingCharacter>(Hit.GetActor());
					if (!Player || bTailHit) continue;
					bTailHit = true;
					Player->ApplyDamage(TailDamage, this, Hit.ImpactPoint, GetActorForwardVector()*220.0f + FVector::UpVector*200.0f);
				}
			}
		}
	}
	PreviousTailPoints = MoveTemp(Points);
}

void AQuasicomboBoss::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(RetryTimer);
	RestoreCameraAndTime();
	Super::EndPlay(Reason);
}

