// Copyright Epic Games, Inc. All Rights Reserved.

#include "SideScrollingCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "SideScrollingInteractable.h"
#include "CombatLifeBar.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "QuasicomboBoss.h"
#include "QuasicomboRunSubsystem.h"
#include "CombatEnemy.h"
#include "SideScrollingCombatEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
bool SubmitFinisherInput(AActor* WorldContext, EQuasicomboQTEInput Input)
{
	if (WorldContext && WorldContext->GetWorld())
	{
		if (const UQuasicomboRunSubsystem* Run = WorldContext->GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
		{
			if (Run->IsBossCombatLocked()) return true;
		}
	}
	if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(UGameplayStatics::GetActorOfClass(WorldContext, AQuasicomboBoss::StaticClass())))
	{
		return Boss->SubmitQTEInput(Input);
	}
	return false;
}
}

ASideScrollingCharacter::ASideScrollingCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Bind montage delegates used by the imported platforming/combat behavior.
	OnDashMontageEnded.BindUObject(this, &ASideScrollingCharacter::DashMontageEnded);
	OnAttackMontageEnded.BindUObject(this, &ASideScrollingCharacter::AttackMontageEnded);

	// Reuse the original template assets directly so the SideScroller Blueprint
	// does not need to be manually rewired after this C++ change.
	static ConstructorHelpers::FObjectFinder<UInputAction> DashActionFinder(TEXT("/Game/Variant_Platforming/Input/Actions/IA_Dash.IA_Dash"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ComboActionFinder(TEXT("/Game/Variant_Combat/Input/Actions/IA_ComboAttack.IA_ComboAttack"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ChargedActionFinder(TEXT("/Game/Variant_Combat/Input/Actions/IA_ChargedAttack.IA_ChargedAttack"));

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DashMontageFinder(TEXT("/Game/Variant_Platforming/Anims/AM_Dash.AM_Dash"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ComboMontageFinder(TEXT("/Game/Variant_Combat/Anims/AM_ComboAttack.AM_ComboAttack"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ChargedMontageFinder(TEXT("/Game/Variant_Combat/Anims/AM_ChargedAttack.AM_ChargedAttack"));

	// Radical Mike is now present in this private branch. Keep the normal
	// SideScroller gameplay class and use the marketplace asset only as the
	// visual/animation layer.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MikeMeshFinder(TEXT("/Game/RadicalMike/Mesh/SKM_MegaMikeZ.SKM_MegaMikeZ"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeIdleFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_Idle.Anim_ZMIKE_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeWalkFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_Walk.Anim_ZMIKE_Walk"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeRunFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_Run.Anim_ZMIKE_Run"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeJumpFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_Jump.Anim_ZMIKE_Jump"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeDashFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_Run_Faster.Anim_ZMIKE_Run_Faster"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikePunchRFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_PunchR.Anim_ZMIKE_PunchR"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikePunchLFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_PunchL.Anim_ZMIKE_PunchL"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeUppercutRFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_UppercutR.Anim_ZMIKE_UppercutR"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeUppercutLFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_UppercutL.Anim_ZMIKE_UppercutL"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeChargeFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_IdleAggro.Anim_ZMIKE_IdleAggro"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MikeHitFinder(TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_HitRegisterFront.Anim_ZMIKE_HitRegisterFront"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> MikeTrailFinder(TEXT("/Game/Variant_Platforming/VFX/NS_Jump_Trail.NS_Jump_Trail"));

	if (DashActionFinder.Succeeded())
	{
		DashAction = DashActionFinder.Object;
	}

	if (ComboActionFinder.Succeeded())
	{
		ComboAttackAction = ComboActionFinder.Object;
	}

	if (ChargedActionFinder.Succeeded())
	{
		ChargedAttackAction = ChargedActionFinder.Object;
	}

	if (DashMontageFinder.Succeeded())
	{
		DashMontage = DashMontageFinder.Object;
	}

	if (ComboMontageFinder.Succeeded())
	{
		ComboAttackMontage = ComboMontageFinder.Object;
	}

	if (ChargedMontageFinder.Succeeded())
	{
		ChargedAttackMontage = ChargedMontageFinder.Object;
	}

	if (MikeMeshFinder.Succeeded())
	{
		CharacterVisualMeshOverride = MikeMeshFinder.Object;
	}
	if (MikeIdleFinder.Succeeded()) NativeIdleAnimation = MikeIdleFinder.Object;
	if (MikeWalkFinder.Succeeded()) NativeWalkAnimation = MikeWalkFinder.Object;
	if (MikeRunFinder.Succeeded()) NativeRunAnimation = MikeRunFinder.Object;
	if (MikeJumpFinder.Succeeded()) NativeJumpAnimation = MikeJumpFinder.Object;
	if (MikeDashFinder.Succeeded()) NativeDashAnimation = MikeDashFinder.Object;
	if (MikePunchRFinder.Succeeded()) NativeComboAnimations.Add(MikePunchRFinder.Object);
	if (MikeUppercutLFinder.Succeeded()) NativeComboAnimations.Add(MikeUppercutLFinder.Object);
	if (MikePunchRFinder.Succeeded()) NativeComboAnimations.Add(MikePunchRFinder.Object);
	if (MikePunchLFinder.Succeeded()) NativeComboAnimations.Add(MikePunchLFinder.Object);
	if (MikeUppercutRFinder.Succeeded()) NativeComboAnimations.Add(MikeUppercutRFinder.Object);
	if (MikeChargeFinder.Succeeded()) NativeChargedHoldAnimation = MikeChargeFinder.Object;
	if (MikeUppercutRFinder.Succeeded()) NativeChargedReleaseAnimation = MikeUppercutRFinder.Object;
	if (MikeHitFinder.Succeeded()) NativeHitReactionAnimation = MikeHitFinder.Object;
	if (MikeTrailFinder.Succeeded()) MikeJumpTrailSystem = MikeTrailFinder.Object;

	// create the camera component
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetRelativeLocationAndRotation(FVector(0.0f, 300.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f));

	// create the combat life bar
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);
	LifeBar->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	LifeBar->SetWidgetSpace(EWidgetSpace::Screen);
	LifeBar->SetDrawAtDesiredSize(true);

	static ConstructorHelpers::FClassFinder<UUserWidget> LifeBarClassFinder(TEXT("/Game/Variant_Combat/UI/UI_LifeBar"));
	if (LifeBarClassFinder.Succeeded())
	{
		LifeBar->SetWidgetClass(LifeBarClassFinder.Class);
	}

	// configure the collision capsule
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 90.0f);

	// configure the Pawn properties
	bUseControllerRotationYaw = false;

	// configure the character movement component
	GetCharacterMovement()->GravityScale = 1.75f;
	GetCharacterMovement()->MaxAcceleration = 1500.0f;
	GetCharacterMovement()->BrakingFrictionFactor = 1.0f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->Mass = 500.0f;

	GetCharacterMovement()->SetWalkableFloorAngle(75.0f);
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->bIgnoreBaseRotation = true;

	GetCharacterMovement()->PerchRadiusThreshold = 15.0f;
	GetCharacterMovement()->LedgeCheckThreshold = 6.0f;

	GetCharacterMovement()->JumpZVelocity = 750.0f;
	GetCharacterMovement()->AirControl = 1.0f;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 750.0f, 0.0f);
	GetCharacterMovement()->bOrientRotationToMovement = true;

	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));
	GetCharacterMovement()->bConstrainToPlane = true;

	// enable double jump and coyote time
	JumpMaxCount = 3;

	AttackTraceFallbackSockets = {
		TEXT("Attack_R"), TEXT("Attack_L"),
		TEXT("BusterR"), TEXT("BusterL"),
		TEXT("hand_r"), TEXT("hand_l"),
		TEXT("RightHand"), TEXT("LeftHand")
	};
	PhysicsAnchorBoneCandidates = {
		TEXT("pelvis"), TEXT("Pelvis"),
		TEXT("hips"), TEXT("Hips"),
		TEXT("hip"), TEXT("Hip"),
		TEXT("root"), TEXT("Root")
	};

	// Combat template actors identify the player by this tag.
	Tags.AddUnique(FName(TEXT("Player")));
}

void ASideScrollingCharacter::ApplyCharacterVisualOverride()
{
	if (CharacterVisualMeshOverride)
	{
		GetMesh()->SetSkeletalMesh(CharacterVisualMeshOverride, true);
	}

	bNativeCharacterAnimationActive = bUseNativeCharacterAnimation && CharacterVisualMeshOverride && !CharacterVisualAnimClassOverride;
	bMikeAnimationActive = CharacterVisualMeshOverride && CharacterVisualAnimClassOverride;

	if (CharacterVisualAnimClassOverride)
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(CharacterVisualAnimClassOverride);
	}
	else if (bNativeCharacterAnimationActive)
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		PlayNativeCharacterAnimation(NativeIdleAnimation, true);
	}

	if (bOverrideCharacterMeshTransform)
	{
		GetMesh()->SetRelativeLocation(CharacterMeshRelativeLocation);
		GetMesh()->SetRelativeRotation(CharacterMeshRelativeRotation);
		GetMesh()->SetRelativeScale3D(CharacterMeshRelativeScale);
	}
}

void ASideScrollingCharacter::PlayNativeCharacterAnimation(UAnimationAsset* Animation, bool bLooping, float LockSeconds)
{
	if (!bNativeCharacterAnimationActive || !Animation)
	{
		return;
	}

	if (CurrentNativeAnimation != Animation || bCurrentNativeAnimationLooping != bLooping)
	{
		GetMesh()->PlayAnimation(Animation, bLooping);
		CurrentNativeAnimation = Animation;
		bCurrentNativeAnimationLooping = bLooping;
	}

	if (LockSeconds > 0.0f && GetWorld())
	{
		NativeAnimationLockUntil = FMath::Max(NativeAnimationLockUntil, GetWorld()->GetTimeSeconds() + LockSeconds);
	}
}

void ASideScrollingCharacter::UpdateNativeCharacterAnimation()
{
	if (!bNativeCharacterAnimationActive || !GetWorld() || CurrentHP <= 0.0f || bIsAttacking || bIsDashing)
	{
		return;
	}

	if (GetWorld()->GetTimeSeconds() < NativeAnimationLockUntil)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->IsFalling())
	{
		PlayNativeCharacterAnimation(NativeJumpAnimation, true);
		return;
	}

	const float HorizontalSpeed = FMath::Abs(Movement->Velocity.X);
	if (HorizontalSpeed >= NativeRunSpeedThreshold)
	{
		PlayNativeCharacterAnimation(NativeRunAnimation, true);
	}
	else if (HorizontalSpeed > 5.0f)
	{
		PlayNativeCharacterAnimation(NativeWalkAnimation, true);
	}
	else
	{
		PlayNativeCharacterAnimation(NativeIdleAnimation, true);
	}
}

void ASideScrollingCharacter::DumpCharacterRigInfo() const
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("MikeRig: character has no skeletal mesh component."));
		return;
	}

	const USkeletalMesh* MeshAsset = MeshComponent->GetSkeletalMeshAsset();
	UE_LOG(LogTemp, Display, TEXT("MikeRig: Mesh=%s PhysicsAsset=%s Bones=%d"),
		MeshAsset ? *MeshAsset->GetPathName() : TEXT("None"),
		MeshComponent->GetPhysicsAsset() ? *MeshComponent->GetPhysicsAsset()->GetPathName() : TEXT("None"),
		MeshComponent->GetNumBones());

	for (int32 BoneIndex = 0; BoneIndex < MeshComponent->GetNumBones(); ++BoneIndex)
	{
		UE_LOG(LogTemp, Display, TEXT("MikeRig: Bone[%d]=%s"), BoneIndex, *MeshComponent->GetBoneName(BoneIndex).ToString());
	}

	for (const FName SocketName : MeshComponent->GetAllSocketNames())
	{
		UE_LOG(LogTemp, Display, TEXT("MikeRig: SocketOrBone=%s"), *SocketName.ToString());
	}
}

void ASideScrollingCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyCharacterVisualOverride();
	if (MikeJumpTrailSystem && GetMesh()->GetBoneIndex(TEXT("foot_l")) != INDEX_NONE &&
		GetMesh()->GetBoneIndex(TEXT("foot_r")) != INDEX_NONE)
	{
		MikeLeftJumpTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(MikeJumpTrailSystem, GetMesh(),
			TEXT("foot_l"), FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget,
			false, false, ENCPoolMethod::None, false);
		MikeRightJumpTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(MikeJumpTrailSystem, GetMesh(),
			TEXT("foot_r"), FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget,
			false, false, ENCPoolMethod::None, false);
		for (UNiagaraComponent* Trail : {MikeLeftJumpTrail.Get(), MikeRightJumpTrail.Get()})
		{
			if (Trail) Trail->SetVariableLinearColor(TEXT("User.Color"), MikeJumpTrailColor);
		}
	}

	// Ensure the effective health of this player is 10 HP even when its
	// Blueprint supplies a different starting MaxHP value.
	MaxHP = FMath::Max(10.0f, MaxHP);

	// Initialize the combat life bar if its Blueprint class loaded successfully.
	if (LifeBar)
	{
		LifeBar->InitWidget();
		LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
		if (LifeBarWidget)
		{
			// Screen-space widget components size from their widget layout, not
			// their component transform. Preserve the authored render scale.
			const FVector2D AuthoredScale = LifeBarWidget->GetRenderTransform().Scale;
			LifeBarWidget->SetRenderScale(AuthoredScale * 1.3f);
		}
	}

	// The Combat Blueprint normally supplies montage section names. Since the
	// SideScroller Blueprint predates these properties, infer useful defaults
	// directly from the imported montages.
	if (ComboAttackMontage && ComboSectionNames.IsEmpty())
	{
		for (int32 SectionIndex = 0; SectionIndex < ComboAttackMontage->GetNumSections(); ++SectionIndex)
		{
			ComboSectionNames.Add(ComboAttackMontage->GetSectionName(SectionIndex));
		}
	}

	if (ChargedAttackMontage)
	{
		const int32 NumSections = ChargedAttackMontage->GetNumSections();

		if (ChargeLoopSection.IsNone())
		{
			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				const FName SectionName = ChargedAttackMontage->GetSectionName(SectionIndex);
				const FString SectionString = SectionName.ToString();

				if (SectionString.Contains(TEXT("Loop"), ESearchCase::IgnoreCase) ||
					SectionString.Contains(TEXT("Charge"), ESearchCase::IgnoreCase))
				{
					ChargeLoopSection = SectionName;
					break;
				}
			}

			if (ChargeLoopSection.IsNone() && NumSections > 1)
			{
				ChargeLoopSection = ChargedAttackMontage->GetSectionName(1);
			}
		}

		if (ChargeAttackSection.IsNone())
		{
			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				const FName SectionName = ChargedAttackMontage->GetSectionName(SectionIndex);
				const FString SectionString = SectionName.ToString();

				if ((SectionString.Contains(TEXT("Attack"), ESearchCase::IgnoreCase) ||
					 SectionString.Contains(TEXT("Release"), ESearchCase::IgnoreCase)) &&
					SectionName != ChargeLoopSection)
				{
					ChargeAttackSection = SectionName;
					break;
				}
			}

			if (ChargeAttackSection.IsNone() && NumSections > 0)
			{
				ChargeAttackSection = ChargedAttackMontage->GetSectionName(NumSections - 1);
			}
		}
	}

	ResetHP();
}

void ASideScrollingCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateNativeCharacterAnimation();

	if (!bHasTakenDamage || CurrentHP <= 0.0f || CurrentHP >= MaxHP || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// Count only the part of this frame after the three-second grace period.
	// A large frame crossing the boundary must not heal for time before it.
	const float TimeSinceDamage = GetWorld()->GetTimeSeconds() - LastDamageTakenTime;
	const float RegenerationTime = FMath::Clamp(TimeSinceDamage - HealthRegenDelay, 0.0f, DeltaSeconds);
	if (RegenerationTime > 0.0f)
	{
		ApplyHealing(MaxHP * HealthRegenFractionPerSecond * RegenerationTime, this);
	}
}

void ASideScrollingCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear timers owned by imported platforming/combat behavior
	GetWorld()->GetTimerManager().ClearTimer(WallJumpTimer);
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
	GetWorld()->GetTimerManager().ClearTimer(HitComboResetTimer);
	GetWorld()->GetTimerManager().ClearTimer(ChargedDashStrikeTimer);
	GetWorld()->GetTimerManager().ClearTimer(DashFallbackTimer);
	GetWorld()->GetTimerManager().ClearTimer(FallbackAttackStrikeTimer);
	GetWorld()->GetTimerManager().ClearTimer(FallbackAttackRecoveryTimer);
	GetWorld()->GetTimerManager().ClearTimer(DeathRagdollTimer);
}

void ASideScrollingCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Existing SideScroller actions.
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASideScrollingCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASideScrollingCharacter::DoJumpEnd);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::DoInteract);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::Move);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::Drop);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Completed, this, &ASideScrollingCharacter::DropReleased);

		// Imported platforming dash.
		if (DashAction)
		{
			EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &ASideScrollingCharacter::Dash);
		}

		// Imported Combat variant attacks.
		if (ComboAttackAction)
		{
			EnhancedInputComponent->BindAction(ComboAttackAction, ETriggerEvent::Started, this, &ASideScrollingCharacter::ComboAttackPressed);
		}

		if (ChargedAttackAction)
		{
			EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Started, this, &ASideScrollingCharacter::ChargedAttackPressed);
			EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Completed, this, &ASideScrollingCharacter::ChargedAttackReleased);
			EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Canceled, this, &ASideScrollingCharacter::ChargedAttackReleased);
		}
	}
}

void ASideScrollingCharacter::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// only apply push impulse if we're falling
	if (!GetCharacterMovement()->IsFalling())
	{
		return;
	}

	if (OtherComp && OtherComp->Mobility == EComponentMobility::Movable && OtherComp->IsSimulatingPhysics())
	{
		const FVector PushDir = FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
		OtherComp->AddImpulse(PushDir * JumpPushImpulse, NAME_None, true);
	}
}

void ASideScrollingCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// reset advanced movement state
	bHasDoubleJumped = false;
	bHasDashed = false;

	if (bIsDashing)
	{
		EndDash();
	}

	UpdateMikeJumpTrailState(false);

	// Finish any partial hit-reaction ragdoll when the living player lands.
	if (CurrentHP > 0.0f)
	{
		GetMesh()->SetPhysicsBlendWeight(0.0f);
	}
}

void ASideScrollingCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling)
	{
		LastFallTime = GetWorld()->GetTimeSeconds();
	}
}

void ASideScrollingCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MoveVector = Value.Get<FVector2D>();
	DoMove(MoveVector.Y);
}

void ASideScrollingCharacter::Dash()
{
	DoDash();
}

void ASideScrollingCharacter::Drop(const FInputActionValue& Value)
{
	DoDrop(Value.Get<float>());
}

void ASideScrollingCharacter::DropReleased(const FInputActionValue& Value)
{
	DoDrop(0.0f);
}

void ASideScrollingCharacter::ComboAttackPressed()
{
	DoComboAttackStart();
}

void ASideScrollingCharacter::ChargedAttackPressed()
{
	DoChargedAttackStart();
}

void ASideScrollingCharacter::ChargedAttackReleased()
{
	DoChargedAttackEnd();
}

void ASideScrollingCharacter::DoMove(float Forward)
{
	if (const UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		if (Run->IsBossCombatLocked()) return;
	}
	if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(UGameplayStatics::GetActorOfClass(this, AQuasicomboBoss::StaticClass())))
	{
		if (Boss->QTE && Boss->QTE->IsQTEActive()) return;
	}
	if (!bHasWallJumped)
	{
		ActionValueY = Forward;

		const FVector MoveDir = FVector(1.0f, Forward > 0.0f ? 0.1f : -0.1f, 0.0f);
		AddMovementInput(MoveDir, Forward);
	}
}

void ASideScrollingCharacter::DoDrop(float Value)
{
	DropValue = Value;
}

void ASideScrollingCharacter::DoJumpStart()
{
	if (SubmitFinisherInput(this, EQuasicomboQTEInput::Jump)) return;
	MultiJump();
}

void ASideScrollingCharacter::UpdateMikeJumpTrailState(bool bEnabled)
{
	for (UNiagaraComponent* Trail : {MikeLeftJumpTrail.Get(), MikeRightJumpTrail.Get()})
	{
		if (!Trail) continue;
		if (bEnabled && !Trail->IsActive()) Trail->Activate(true);
		else if (!bEnabled && Trail->IsActive()) Trail->Deactivate();
	}
}

void ASideScrollingCharacter::DoJumpEnd()
{
	StopJumping();
}

void ASideScrollingCharacter::DoDash()
{
	if (SubmitFinisherInput(this, EQuasicomboQTEInput::Dash)) return;
	// One dash per airtime, matching the Platforming variant.
	if (bHasDashed || (bIsAttacking && !bIsChargingAttack) || CurrentHP <= 0.0f)
	{
		return;
	}

	bChargedDashAttack = bIsChargingAttack;
	if (bChargedDashAttack)
	{
		++CurrentStrikeSerial;
		GetWorldTimerManager().ClearTimer(FallbackAttackStrikeTimer);
		GetWorldTimerManager().ClearTimer(FallbackAttackRecoveryTimer);
		bUsingFallbackChargedAttack = false;
		CachedAttackInputTime = -1000.0f;
		bIsChargingAttack = false;
		bIsAttacking = false;
		HitActorsThisStrike.Reset();
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.05f, bMikeAnimationActive ? MikeChargedHoldMontage : ChargedAttackMontage);
		}
		OnChargedDashStarted();
	}

	bIsDashing = true;
	bHasDashed = true;

	PreDashGravityScale = GetCharacterMovement()->GravityScale;
	PreDashVelocity = GetCharacterMovement()->Velocity;
	GetCharacterMovement()->GravityScale = 0.0f;
	const float DashDirection = !FMath::IsNearlyZero(ActionValueY) ? FMath::Sign(ActionValueY) : FMath::Sign(GetActorForwardVector().X);
	GetCharacterMovement()->Velocity = FVector(DashDirection * DashHorizontalSpeed, 0.0f, 0.0f);

	UpdateMikeJumpTrailState(true);
	if (bChargedDashAttack)
	{
		GetWorldTimerManager().SetTimer(ChargedDashStrikeTimer, this, &ASideScrollingCharacter::PerformChargedDashStrike, FMath::Max(0.01f, ChargedDashStrikeDelay), false);
	}

	if (bNativeCharacterAnimationActive)
	{
		PlayNativeCharacterAnimation(NativeDashAnimation, true, DashFallbackDuration);
	}
	if (bMikeAnimationActive && MikeDashMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			if (AnimInstance->Montage_Play(MikeDashMontage, MikeDashPlayRate) > 0.0f)
			{
				// The visual clip is shorter than the original dash. Loop it until
				// the gameplay timer ends so the animation covers the full gap jump.
				const FName Section = MikeDashMontage->GetSectionName(0);
				AnimInstance->Montage_SetNextSection(Section, Section, MikeDashMontage);
			}
		}
	}
	if (bNativeCharacterAnimationActive || bMikeAnimationActive)
	{
		OnFallbackDashStarted();
		const float MinimumFallbackDuration = bChargedDashAttack ? ChargedDashStrikeDelay + 0.02f : 0.01f;
		GetWorldTimerManager().SetTimer(
			DashFallbackTimer, this, &ASideScrollingCharacter::EndDash,
			FMath::Max(DashFallbackDuration, MinimumFallbackDuration), false);
		return;
	}

	// AM_Dash contains the template's dash/root-motion behavior.
	if (DashMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			const float MontageLength = AnimInstance->Montage_Play(DashMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

			if (MontageLength > 0.0f)
			{
				AnimInstance->Montage_SetEndDelegate(OnDashMontageEnded, DashMontage);
				return;
			}
		}
	}

	// Keep dash gameplay alive even when the replacement rig cannot play the
	// Manny montage. Blueprint can use this hook to play a native Mike clip.
	OnFallbackDashStarted();
	const float MinimumFallbackDuration = bChargedDashAttack ? ChargedDashStrikeDelay + 0.02f : 0.01f;
	GetWorldTimerManager().SetTimer(
		DashFallbackTimer,
		this,
		&ASideScrollingCharacter::EndDash,
		FMath::Max(DashFallbackDuration, MinimumFallbackDuration),
		false
	);
}

void ASideScrollingCharacter::PerformChargedDashStrike()
{
	if (bIsDashing && bChargedDashAttack && CurrentHP > 0.0f)
	{
		DoAttackTrace(NAME_None);
	}
}

void ASideScrollingCharacter::DashMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	EndDash();
}

void ASideScrollingCharacter::EndDash()
{
	if (!bIsDashing)
	{
		return;
	}

	GetCharacterMovement()->GravityScale = PreDashGravityScale;
	bIsDashing = false;
	GetWorldTimerManager().ClearTimer(DashFallbackTimer);
	GetWorldTimerManager().ClearTimer(ChargedDashStrikeTimer);
	bChargedDashAttack = false;
	if (bMikeAnimationActive && MikeDashMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.05f, MikeDashMontage);
		}
	}
	if (GetCharacterMovement()->IsFalling())
	{
		// Root motion can leave the movement velocity at zero when a dash ends.
		// Retain the jump's rise or fall, and any horizontal speed it had gained.
		FVector ExitVelocity = GetCharacterMovement()->Velocity;
		if (FMath::IsNearlyZero(ExitVelocity.X)) ExitVelocity.X = PreDashVelocity.X;
		ExitVelocity.Z = PreDashVelocity.Z;
		GetCharacterMovement()->Velocity = ExitVelocity;
	}
	if (GetCharacterMovement()->IsMovingOnGround()) UpdateMikeJumpTrailState(false);
	if (bNativeCharacterAnimationActive)
	{
		NativeAnimationLockUntil = 0.0f;
		UpdateNativeCharacterAnimation();
	}

	if (GetCharacterMovement()->IsMovingOnGround())
	{
		bHasDashed = false;
	}
}

void ASideScrollingCharacter::DoInteract()
{
	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + FVector(100.0f, 0.0f, 0.0f);

	FCollisionShape ColSphere;
	ColSphere.SetSphere(InteractionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, ColSphere, QueryParams))
	{
		if (ISideScrollingInteractable* Interactable = Cast<ISideScrollingInteractable>(OutHit.GetActor()))
		{
			Interactable->Interaction(this);
		}
	}
}

void ASideScrollingCharacter::DoComboAttackStart()
{
	if (SubmitFinisherInput(this, EQuasicomboQTEInput::Attack)) return;
	if (CurrentHP <= 0.0f)
	{
		return;
	}

	if (bIsDashing)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.05f, bMikeAnimationActive ? MikeDashMontage : DashMontage);
		}
		EndDash();
	}

	if (bIsAttacking)
	{
		if (bMikeAnimationActive && !bIsChargingAttack)
		{
			const int32 RemainingStages = FMath::Max(0, MikeComboMontages.Num() - 1 - MikeComboStage);
			MikeBufferedComboInputs = FMath::Min(MikeBufferedComboInputs + 1, RemainingStages);
		}
		else
		{
			CachedAttackInputTime = GetWorld()->GetTimeSeconds();
			if (!bIsChargingAttack) bComboAdvanceQueued = true;
		}
		return;
	}

	ComboAttack();
}

void ASideScrollingCharacter::DoComboAttackEnd()
{
	// Kept for API parity with the Combat character.
}

void ASideScrollingCharacter::DoChargedAttackStart()
{
	if (SubmitFinisherInput(this, EQuasicomboQTEInput::Invalid)) return;
	if (CurrentHP <= 0.0f)
	{
		return;
	}

	if (bIsDashing)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.05f, bMikeAnimationActive ? MikeDashMontage : DashMontage);
		}
		EndDash();
	}

	bIsChargingAttack = true;

	if (bIsAttacking)
	{
		if (!bHasLoopedChargedAttack)
		{
			bHasReleasedChargedAttack = false;
		}

		CachedAttackInputTime = GetWorld()->GetTimeSeconds();
		return;
	}

	ChargedAttack();
}

void ASideScrollingCharacter::DoChargedAttackEnd()
{
	bIsChargingAttack = false;
	if (bMikeAnimationActive && bIsAttacking && !bMikeChargedReleaseActive && !bUsingFallbackChargedAttack &&
		MikeChargedHoldMontage && GetMesh()->GetAnimInstance() &&
		GetMesh()->GetAnimInstance()->Montage_IsPlaying(MikeChargedHoldMontage))
	{
		ReleaseMikeChargedAttack();
		return;
	}

	if (bUsingFallbackChargedAttack)
	{
		bHasReleasedChargedAttack = true;
		PerformFallbackChargedStrike();
		return;
	}

	if (bHasLoopedChargedAttack && !bHasReleasedChargedAttack)
	{
		bHasReleasedChargedAttack = true;
		LoopOrResolveChargedAttack();
	}
}

void ASideScrollingCharacter::MultiJump()
{
	// Preserve the Platforming variant rule that jumping cannot interrupt a dash.
	if (bIsDashing)
	{
		return;
	}

	if (DropValue > 0.0f)
	{
		CheckForSoftCollision();
		return;
	}

	DropValue = 0.0f;

	if (!GetCharacterMovement()->IsFalling())
	{
		Jump();
		UpdateMikeJumpTrailState(true);
		return;
	}

	if (!bHasWallJumped && !FMath::IsNearlyZero(ActionValueY))
	{
		FHitResult OutHit;

		const FVector Start = GetActorLocation();
		const FVector End = Start + (FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f) * WallJumpTraceDistance);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

		if (OutHit.bBlockingHit)
		{
			const FRotator BounceRot = UKismetMathLibrary::MakeRotFromX(OutHit.ImpactNormal);
			SetActorRotation(FRotator(0.0f, BounceRot.Yaw, 0.0f));

			FVector WallJumpImpulse = OutHit.ImpactNormal * WallJumpHorizontalImpulse;
			WallJumpImpulse.Z = GetCharacterMovement()->JumpZVelocity * WallJumpVerticalMultiplier;

			LaunchCharacter(WallJumpImpulse, true, true);
			UpdateMikeJumpTrailState(true);

			bHasWallJumped = true;
			GetWorld()->GetTimerManager().SetTimer(WallJumpTimer, this, &ASideScrollingCharacter::ResetWallJump, DelayBetweenWallJumps, false);

			return;
		}
	}

	if (!bHasWallJumped)
	{
		if (GetWorld()->GetTimeSeconds() - LastFallTime < MaxCoyoteTime)
		{
			Jump();
			UpdateMikeJumpTrailState(true);
		}
		else if (!bHasDoubleJumped)
		{
			bHasDoubleJumped = true;
			Jump();
			UpdateMikeJumpTrailState(true);
		}
	}
}

void ASideScrollingCharacter::CheckForSoftCollision()
{
	DropValue = 0.0f;

	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + (FVector::DownVector * SoftCollisionTraceDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(SoftCollisionObjectType);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, QueryParams);

	if (OutHit.GetActor())
	{
		SetSoftCollision(true);
	}
}

void ASideScrollingCharacter::ResetWallJump()
{
	bHasWallJumped = false;
}

void ASideScrollingCharacter::SetSoftCollision(bool bEnabled)
{
	GetCapsuleComponent()->SetCollisionResponseToChannel(SoftCollisionObjectType, bEnabled ? ECR_Ignore : ECR_Block);
}

bool ASideScrollingCharacter::HasDoubleJumped() const
{
	return bHasDoubleJumped;
}

bool ASideScrollingCharacter::HasWallJumped() const
{
	return bHasWallJumped;
}

void ASideScrollingCharacter::RegisterHitCombo()
{
	++HitComboCount;
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) Run->RecordCombo(HitComboCount);

	// Every successful hit refreshes the expiry window.
	GetWorld()->GetTimerManager().ClearTimer(HitComboResetTimer);

	if (HitComboResetDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			HitComboResetTimer,
			this,
			&ASideScrollingCharacter::ResetHitCombo,
			HitComboResetDelay,
			false
		);
	}

	OnHitComboStateChanged();
}

void ASideScrollingCharacter::PrepareForFinisher()
{
	if (bIsDashing) EndDash();
	GetWorldTimerManager().ClearTimer(FallbackAttackStrikeTimer);
	GetWorldTimerManager().ClearTimer(FallbackAttackRecoveryTimer);
	bUsingFallbackChargedAttack = false;
	CachedAttackInputTime = -1000.0f;
	bIsAttacking = false;
	bIsChargingAttack = false;
	bHasLoopedChargedAttack = false;
	bHasReleasedChargedAttack = false;
	bMikeComboWindowOpen = false;
	bMikeChargedReleaseActive = false;
	MikeBufferedComboInputs = 0;
	MikeComboStage = INDEX_NONE;
	ActiveMikeAttackMontage = nullptr;
	HitActorsThisStrike.Reset();
	GetCharacterMovement()->StopMovementImmediately();
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance()) AnimInstance->StopAllMontages(0.1f);
}

void ASideScrollingCharacter::ResetHitCombo()
{
	const bool bHadCombo = HitComboCount > 0;
	HitComboCount = 0;
	GetWorld()->GetTimerManager().ClearTimer(HitComboResetTimer);

	if (bHadCombo)
	{
		OnHitComboStateChanged();
	}
}

void ASideScrollingCharacter::ResetForBossRetry(const FTransform& Transform)
{
	PrepareForFinisher();
	ResetHitCombo();
	GetWorldTimerManager().ClearTimer(WallJumpTimer);
	GetWorldTimerManager().ClearTimer(ChargedDashStrikeTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	bComboAdvanceQueued = false;
	bHasDashed = bHasDoubleJumped = bHasWallJumped = false;
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetPhysicsBlendWeight(0.0f);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	ResetHP();
}

float ASideScrollingCharacter::GetHitComboTimeRemaining() const
{
	if (!GetWorld())
	{
		return 0.0f;
	}

	const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(HitComboResetTimer);
	return Remaining > 0.0f ? Remaining : 0.0f;
}

void ASideScrollingCharacter::ResetHP()
{
	CurrentHP = MaxHP;
	bHasTakenDamage = false;
	PendingDeathImpulse = FVector::ZeroVector;
	PendingDeathImpactPoint = FVector::ZeroVector;

	if (LifeBar)
	{
		LifeBar->SetHiddenInGame(false);
	}

	if (LifeBarWidget)
	{
		LifeBarWidget->SetBarColor(LifeBarColor);
	}

	UpdateLifeBar();
}

void ASideScrollingCharacter::UpdateLifeBar()
{
	if (LifeBarWidget)
	{
		const float Percent = MaxHP > 0.0f ? FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f) : 0.0f;
		LifeBarWidget->SetLifePercentage(Percent);
	}
}

void ASideScrollingCharacter::ComboAttack()
{
	++CurrentStrikeSerial;
	HitActorsThisStrike.Reset();
	bIsAttacking = true;
	bIsChargingAttack = false;
	bComboAdvanceQueued = false;
	ComboCount = 0;
	bMikeComboWindowOpen = false;

	NotifyEnemiesOfIncomingAttack();
	if (bMikeAnimationActive && MikeComboMontages.Num() == 5)
	{
		MikeComboStage = MikeComboStage == INDEX_NONE ? 0 : FMath::Min(MikeComboStage + 1, MikeComboMontages.Num() - 1);
		UAnimMontage* MikeMontage = MikeComboMontages[MikeComboStage].Get();
		if (MikeMontage)
		{
			if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
			{
				ActiveMikeAttackMontage = MikeMontage;
				const bool bUppercut = MikeComboStage == 1 || MikeComboStage == 4;
				if (AnimInstance->Montage_Play(MikeMontage, bUppercut ? MikeUppercutPlayRate : MikeComboPlayRate) > 0.0f)
				{
					AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, MikeMontage);
					return;
				}
				ActiveMikeAttackMontage = nullptr;
			}
		}
	}

	if (bNativeCharacterAnimationActive && NativeComboAnimations.Num() > 0)
	{
		const int32 AnimationIndex = static_cast<int32>((CurrentStrikeSerial - 1) % NativeComboAnimations.Num());
		PlayNativeCharacterAnimation(NativeComboAnimations[AnimationIndex], false, FallbackComboRecoveryTime);
	}

	if (!bNativeCharacterAnimationActive && !bMikeAnimationActive && ComboAttackMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			const float MontageLength = AnimInstance->Montage_Play(ComboAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

			if (MontageLength > 0.0f)
			{
				AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ComboAttackMontage);
				return;
			}
		}
	}

	OnFallbackComboAttackStarted(CurrentStrikeSerial);
	GetWorldTimerManager().ClearTimer(FallbackAttackStrikeTimer);
	GetWorldTimerManager().ClearTimer(FallbackAttackRecoveryTimer);

	if (FallbackComboStrikeDelay <= 0.0f)
	{
		PerformFallbackComboStrike();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			FallbackAttackStrikeTimer,
			this,
			&ASideScrollingCharacter::PerformFallbackComboStrike,
			FallbackComboStrikeDelay,
			false
		);
	}

	GetWorldTimerManager().SetTimer(
		FallbackAttackRecoveryTimer,
		this,
		&ASideScrollingCharacter::FinishFallbackAttack,
		FMath::Max(FallbackComboRecoveryTime, FallbackComboStrikeDelay + 0.01f),
		false
	);
}

void ASideScrollingCharacter::ChargedAttack()
{
	MikeComboStage = INDEX_NONE;
	MikeBufferedComboInputs = 0;
	++CurrentStrikeSerial;
	HitActorsThisStrike.Reset();
	bIsAttacking = true;
	bComboAdvanceQueued = false;
	bHasLoopedChargedAttack = false;
	bHasReleasedChargedAttack = false;
	bMikeChargedReleaseActive = false;
	bUsingFallbackChargedAttack = false;

	NotifyEnemiesOfIncomingAttack();
	if (bMikeAnimationActive && MikeChargedHoldMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			if (AnimInstance->Montage_Play(MikeChargedHoldMontage) > 0.0f)
			{
				ActiveMikeAttackMontage = MikeChargedHoldMontage;
				const FName Section = MikeChargedHoldMontage->GetSectionName(0);
				AnimInstance->Montage_SetNextSection(Section, Section, MikeChargedHoldMontage);
				return;
			}
		}
	}

	if (bNativeCharacterAnimationActive)
	{
		PlayNativeCharacterAnimation(NativeChargedHoldAnimation, true);
	}

	if (!bNativeCharacterAnimationActive && !bMikeAnimationActive && ChargedAttackMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			const float MontageLength = AnimInstance->Montage_Play(ChargedAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

			if (MontageLength > 0.0f)
			{
				AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ChargedAttackMontage);
				return;
			}
		}
	}

	bUsingFallbackChargedAttack = true;
	OnFallbackChargedAttackStarted(CurrentStrikeSerial);
}

void ASideScrollingCharacter::PerformFallbackComboStrike()
{
	if (bIsAttacking && CurrentHP > 0.0f)
	{
		DoAttackTrace(NAME_None);
	}
}

void ASideScrollingCharacter::PerformFallbackChargedStrike()
{
	if (!bUsingFallbackChargedAttack)
	{
		return;
	}

	bUsingFallbackChargedAttack = false;
	GetWorldTimerManager().ClearTimer(FallbackAttackStrikeTimer);

	if (bIsAttacking && CurrentHP > 0.0f)
	{
		if (bNativeCharacterAnimationActive)
		{
			PlayNativeCharacterAnimation(NativeChargedReleaseAnimation, false, FallbackChargedRecoveryTime);
		}
		DoAttackTrace(NAME_None);
		OnFallbackChargedAttackReleased(CurrentStrikeSerial);
	}

	GetWorldTimerManager().ClearTimer(FallbackAttackRecoveryTimer);
	GetWorldTimerManager().SetTimer(
		FallbackAttackRecoveryTimer,
		this,
		&ASideScrollingCharacter::FinishFallbackAttack,
		FallbackChargedRecoveryTime,
		false
	);
}

void ASideScrollingCharacter::ReleaseMikeChargedAttack()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (MikeChargedHoldMontage)
		{
			AnimInstance->Montage_Stop(0.05f, MikeChargedHoldMontage);
		}
		if (MikeChargedReleaseMontage && AnimInstance->Montage_Play(MikeChargedReleaseMontage, MikeChargedReleasePlayRate) > 0.0f)
		{
			ActiveMikeAttackMontage = MikeChargedReleaseMontage;
			bMikeChargedReleaseActive = true;
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, MikeChargedReleaseMontage);
			return;
		}
	}

	// Keep the charged strike playable if the montage or AnimBP is unavailable.
	bUsingFallbackChargedAttack = true;
	PerformFallbackChargedStrike();
}

void ASideScrollingCharacter::FinishFallbackAttack()
{
	bUsingFallbackChargedAttack = false;
	GetWorldTimerManager().ClearTimer(FallbackAttackStrikeTimer);
	GetWorldTimerManager().ClearTimer(FallbackAttackRecoveryTimer);
	NativeAnimationLockUntil = 0.0f;
	AttackMontageEnded(nullptr, false);
	UpdateNativeCharacterAnimation();
}

void ASideScrollingCharacter::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// Starting the next Mike montage can interrupt the outgoing one in the
	// same frame. Its stale callback must not clear the new attack's state.
	if (bMikeAnimationActive && Montage && Montage != ActiveMikeAttackMontage) return;
	ActiveMikeAttackMontage = nullptr;
	const bool bChainBufferedMikeCombo = bMikeAnimationActive && !bInterrupted &&
		!bIsChargingAttack && Montage != MikeChargedReleaseMontage &&
		MikeComboStage != INDEX_NONE && MikeComboStage < MikeComboMontages.Num() - 1 &&
		MikeBufferedComboInputs > 0;
	if (bChainBufferedMikeCombo) --MikeBufferedComboInputs;
	const bool bMikeCanChain = !bMikeAnimationActive || Montage == nullptr || bIsChargingAttack ||
		(bMikeComboWindowOpen && bComboAdvanceQueued) || Montage == MikeChargedReleaseMontage;
	bIsAttacking = false;
	bComboAdvanceQueued = false;
	bMikeComboWindowOpen = false;
	bMikeChargedReleaseActive = false;
	bHasLoopedChargedAttack = false;
	bHasReleasedChargedAttack = false;

	if (CurrentHP <= 0.0f || bInterrupted)
	{
		CachedAttackInputTime = -1000.0f;
		MikeBufferedComboInputs = 0;
		MikeComboStage = INDEX_NONE;
		return;
	}
	if (bChainBufferedMikeCombo)
	{
		ComboAttack();
		return;
	}
	MikeBufferedComboInputs = 0;
	MikeComboStage = INDEX_NONE;

	if (bMikeCanChain && CachedAttackInputTime >= 0.0f &&
		GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= AttackInputCacheTimeTolerance)
	{
		CachedAttackInputTime = -1000.0f;

		if (bIsChargingAttack)
		{
			ChargedAttack();
		}
		else
		{
			ComboAttack();
		}
	}
}

FVector ASideScrollingCharacter::ResolveAttackTraceStart(FName RequestedSource) const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (MeshComponent)
	{
		if (!RequestedSource.IsNone() &&
			(MeshComponent->DoesSocketExist(RequestedSource) || MeshComponent->GetBoneIndex(RequestedSource) != INDEX_NONE))
		{
			return MeshComponent->GetSocketLocation(RequestedSource);
		}

		for (const FName Candidate : AttackTraceFallbackSockets)
		{
			if (!Candidate.IsNone() &&
				(MeshComponent->DoesSocketExist(Candidate) || MeshComponent->GetBoneIndex(Candidate) != INDEX_NONE))
			{
				return MeshComponent->GetSocketLocation(Candidate);
			}
		}

		// Last-resort heuristic for third-party rigs whose exact bone names are
		// unknown at compile time. Prefer a hand/claw/buster-style bone.
		for (int32 BoneIndex = 0; BoneIndex < MeshComponent->GetNumBones(); ++BoneIndex)
		{
			const FName BoneName = MeshComponent->GetBoneName(BoneIndex);
			const FString LowerBoneName = BoneName.ToString().ToLower();
			if (LowerBoneName.Contains(TEXT("hand")) ||
				LowerBoneName.Contains(TEXT("claw")) ||
				LowerBoneName.Contains(TEXT("buster")))
			{
				return MeshComponent->GetSocketLocation(BoneName);
			}
		}
	}

	return GetActorLocation() + FVector(0.0f, 0.0f, 25.0f);
}

FName ASideScrollingCharacter::ResolvePhysicsAnchorBone() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return NAME_None;
	}

	if (!PelvisBoneName.IsNone() && MeshComponent->GetBoneIndex(PelvisBoneName) != INDEX_NONE)
	{
		return PelvisBoneName;
	}

	for (const FName Candidate : PhysicsAnchorBoneCandidates)
	{
		if (!Candidate.IsNone() && MeshComponent->GetBoneIndex(Candidate) != INDEX_NONE)
		{
			return Candidate;
		}
	}

	for (int32 BoneIndex = 0; BoneIndex < MeshComponent->GetNumBones(); ++BoneIndex)
	{
		const FName BoneName = MeshComponent->GetBoneName(BoneIndex);
		const FString LowerBoneName = BoneName.ToString().ToLower();
		if (LowerBoneName.Contains(TEXT("pelvis")) ||
			LowerBoneName.Contains(TEXT("hip")) ||
			LowerBoneName.Contains(TEXT("waist")))
		{
			return BoneName;
		}
	}

	return MeshComponent->GetNumBones() > 0 ? MeshComponent->GetBoneName(0) : NAME_None;
}

void ASideScrollingCharacter::DoAttackTrace(FName DamageSourceBone)
{
	TArray<FHitResult> OutHits;

	const FVector TraceStart = ResolveAttackTraceStart(DamageSourceBone);
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * MeleeTraceDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(MeleeTraceRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		for (const FHitResult& CurrentHit : OutHits)
		{
			AActor* HitActor = CurrentHit.GetActor();
			if (!IsValid(HitActor) || HitActorsThisStrike.Contains(HitActor)) continue;
			if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor))
			{
				HitActorsThisStrike.Add(HitActor);
				const ACombatEnemy* CombatEnemy = Cast<ACombatEnemy>(HitActor);
				const ASideScrollingCombatEnemy* SideEnemy = Cast<ASideScrollingCombatEnemy>(HitActor);
				const float HPBefore = CombatEnemy ? CombatEnemy->CurrentHP : 0.0f;
				const int32 StrikesBefore = SideEnemy ? SideEnemy->GetAcceptedStrikes() : 0;
				const float Knockback = bChargedDashAttack ? ChargedDashKnockbackMultiplier : 1.0f;
				const FVector Impulse = (CurrentHit.ImpactNormal * -MeleeKnockbackImpulse * Knockback) + (FVector::UpVector * MeleeLaunchImpulse);

				const float StrikeDamage = bMikeAnimationActive && MikeComboStage != INDEX_NONE &&
					!bIsChargingAttack && !bMikeChargedReleaseActive
					? MeleeDamage + MikeComboDamageStep * MikeComboStage : MeleeDamage;
				Damageable->ApplyDamage(StrikeDamage, this, CurrentHit.ImpactPoint, Impulse);
				if (!CombatEnemy || CombatEnemy->CurrentHP < HPBefore || (SideEnemy && SideEnemy->GetAcceptedStrikes() > StrikesBefore))
				{
					DealtDamage(StrikeDamage, CurrentHit.ImpactPoint);
					RegisterHitCombo();
				}
			}
		}
	}
}

void ASideScrollingCharacter::CheckCombo()
{
	if (bMikeAnimationActive)
	{
		bMikeComboWindowOpen = bIsAttacking && !bIsChargingAttack;
		return;
	}
	if (!bIsAttacking || bIsChargingAttack || !bComboAdvanceQueued || !ComboAttackMontage) return;
	const int32 NextSection = ComboCount + 1;
	if (!ComboSectionNames.IsValidIndex(NextSection)) return;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (!AnimInstance->Montage_IsPlaying(ComboAttackMontage) || AnimInstance->Montage_GetCurrentSection(ComboAttackMontage) != ComboSectionNames[ComboCount]) return;
		bComboAdvanceQueued = false;
		CachedAttackInputTime = -1000.0f;
		ComboCount = NextSection;
		++CurrentStrikeSerial;
		HitActorsThisStrike.Reset();
		NotifyEnemiesOfIncomingAttack();
		AnimInstance->Montage_JumpToSection(ComboSectionNames[ComboCount], ComboAttackMontage);
	}
}

void ASideScrollingCharacter::CheckChargedAttack()
{
	if (bMikeAnimationActive) return;
	bHasLoopedChargedAttack = true;
	bHasReleasedChargedAttack = !bIsChargingAttack;
	LoopOrResolveChargedAttack();
}

void ASideScrollingCharacter::LoopOrResolveChargedAttack()
{
	if (!ChargedAttackMontage)
	{
		return;
	}

	const FName TargetSection = bHasReleasedChargedAttack ? ChargeAttackSection : ChargeLoopSection;

	if (TargetSection.IsNone())
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_JumpToSection(TargetSection, ChargedAttackMontage);
	}
}

void ASideScrollingCharacter::NotifyEnemiesOfIncomingAttack()
{
	TArray<FHitResult> OutHits;

	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * DangerTraceDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(DangerTraceRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		for (const FHitResult& CurrentHit : OutHits)
		{
			if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(CurrentHit.GetActor()))
			{
				Damageable->NotifyDanger(GetActorLocation(), this);
			}
		}
	}
}

UAnimMontage* ASideScrollingCharacter::SelectMikeHitMontage(const AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) const
{
	FVector ToSource = DamageCauser ? DamageCauser->GetActorLocation() - GetActorLocation() : DamageLocation - GetActorLocation();
	if (ToSource.IsNearlyZero()) ToSource = -DamageImpulse;
	const float Forward = FVector::DotProduct(ToSource, GetActorForwardVector());
	const float Right = FVector::DotProduct(ToSource, GetActorRightVector());
	if (FMath::Abs(Forward) >= FMath::Abs(Right))
	{
		return Forward >= 0.0f ? MikeHitFrontMontage : MikeHitBackMontage;
	}
	return Right >= 0.0f ? MikeHitRightMontage : MikeHitLeftMontage;
}

void ASideScrollingCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	if (ActualDamage > 0.0f)
	{
		if (CurrentHP > 0.0f)
		{
			GetWorldTimerManager().ClearTimer(FallbackAttackStrikeTimer);
			GetWorldTimerManager().ClearTimer(FallbackAttackRecoveryTimer);
			bUsingFallbackChargedAttack = false;
			CachedAttackInputTime = -1000.0f;
			bComboAdvanceQueued = false;
			bMikeComboWindowOpen = false;
			bMikeChargedReleaseActive = false;
			MikeBufferedComboInputs = 0;
			MikeComboStage = INDEX_NONE;
			ActiveMikeAttackMontage = nullptr;
			bIsAttacking = false;
			bIsChargingAttack = false;
			ResetHitCombo();
			if (bIsDashing) EndDash();
			if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
			{
				AnimInstance->StopAllMontages(0.08f);
				if (bMikeAnimationActive)
				{
					if (UAnimMontage* HitMontage = SelectMikeHitMontage(DamageCauser, DamageLocation, DamageImpulse))
					{
						AnimInstance->Montage_Play(HitMontage);
					}
				}
			}
			if (bNativeCharacterAnimationActive)
			{
				PlayNativeCharacterAnimation(NativeHitReactionAnimation, false, 0.20f);
			}
			GetCharacterMovement()->AddImpulse(DamageImpulse, true);
		}
		else if (GetMesh()->IsSimulatingPhysics())
		{
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}
		else
		{
			PendingDeathImpulse = DamageImpulse;
			PendingDeathImpactPoint = DamageLocation;
		}

		ReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
	}
}

void ASideScrollingCharacter::HandleDeath()
{
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		if (Run->TryHandleBossFailure() && Run->GetOutcome() == EQuasicomboRunOutcome::Playing)
		{
			return;
		}
	}
	GetWorldTimerManager().ClearTimer(FallbackAttackStrikeTimer);
	GetWorldTimerManager().ClearTimer(FallbackAttackRecoveryTimer);
	bUsingFallbackChargedAttack = false;

	if (bIsDashing)
	{
		EndDash();
	}
	UpdateMikeJumpTrailState(false);

	bIsAttacking = false;
	bIsChargingAttack = false;
	bMikeComboWindowOpen = false;
	bMikeChargedReleaseActive = false;
	MikeBufferedComboInputs = 0;
	MikeComboStage = INDEX_NONE;
	ActiveMikeAttackMontage = nullptr;
	ResetHitCombo();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.15f);
	}

	GetCharacterMovement()->DisableMovement();
	GetMesh()->SetPhysicsBlendWeight(0.0f);
	bool bDeathMontagePlaying = false;
	if (bMikeAnimationActive && MikeDeathMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			const float MontageLength = AnimInstance->Montage_Play(MikeDeathMontage);
			if (MontageLength > 0.0f)
			{
				bDeathMontagePlaying = true;
				GetWorldTimerManager().SetTimer(DeathRagdollTimer, this, &ASideScrollingCharacter::StartDeathRagdoll,
					FMath::Clamp(MontageLength - 0.02f, 0.01f, 1.0f), false);
			}
		}
	}
	if (!bDeathMontagePlaying) StartDeathRagdoll();

	if (LifeBar)
	{
		LifeBar->SetHiddenInGame(true);
	}

	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		Run->EndRun(EQuasicomboRunOutcome::Defeat);
		Run->ExtendDefeatReload(2.6f);
	}
}

void ASideScrollingCharacter::StartDeathRagdoll()
{
	GetWorldTimerManager().ClearTimer(DeathRagdollTimer);
	if (GetMesh()->IsSimulatingPhysics()) return;
	GetMesh()->SetSimulatePhysics(true);
	if (!PendingDeathImpulse.IsNearlyZero())
	{
		GetMesh()->AddImpulseAtLocation(PendingDeathImpulse * GetMesh()->GetMass(), PendingDeathImpactPoint);
	}
}

void ASideScrollingCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	if (CurrentHP <= 0.0f || Healing <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Min(CurrentHP + Healing, MaxHP);
	UpdateLifeBar();
}

void ASideScrollingCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// Player-side hook intentionally left empty, matching the Combat variant.
}

void ASideScrollingCharacter::RespawnCharacter()
{
	// ASideScrollingPlayerController already respawns its CharacterClass when
	// the possessed pawn is destroyed.
	Destroy();
}

float ASideScrollingCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (const UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		if (Run->IsBossCombatLocked()) return 0.0f;
	}
	if (CurrentHP <= 0.0f || Damage <= 0.0f)
	{
		return 0.0f;
	}

	const float ActualDamage = FMath::Min(Damage, CurrentHP);
	LastDamageTakenTime = GetWorld()->GetTimeSeconds();
	bHasTakenDamage = true;
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>()) Run->RecordDamageTaken(ActualDamage);
	CurrentHP -= ActualDamage;

	if (CurrentHP <= 0.0f)
	{
		CurrentHP = 0.0f;
		UpdateLifeBar();
		HandleDeath();
	}
	else
	{
		UpdateLifeBar();

		// Resolve the partial-ragdoll anchor against the active rig instead of
		// assuming Manny's "pelvis" bone exists.
		if (!bMikeAnimationActive)
		{
			GetMesh()->SetPhysicsBlendWeight(0.5f);
			const FName PhysicsAnchor = ResolvePhysicsAnchorBone();
			if (!PhysicsAnchor.IsNone())
			{
				GetMesh()->SetBodySimulatePhysics(PhysicsAnchor, false);
			}
		}
	}

	return ActualDamage;
}
