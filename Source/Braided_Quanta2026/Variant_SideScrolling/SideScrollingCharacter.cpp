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
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "QuasicomboBoss.h"
#include "QuasicomboRunSubsystem.h"
#include "CombatEnemy.h"
#include "SideScrollingCombatEnemy.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool SubmitFinisherInput(AActor* WorldContext, EQuasicomboQTEInput Input)
{
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

	// Combat template actors identify the player by this tag.
	Tags.AddUnique(FName(TEXT("Player")));
}

void ASideScrollingCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Increase the effective health of this player by 20% even when its
	// Blueprint supplies a different starting MaxHP value.
	MaxHP = FMath::Max(0.0f, MaxHP * 1.2f);

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

	SetJumpTrailState(false);

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
		CachedAttackInputTime = -1000.0f;
		bIsChargingAttack = false;
		bIsAttacking = false;
		HitActorsThisStrike.Reset();
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance()) AnimInstance->Montage_Stop(0.05f, ChargedAttackMontage);
		OnChargedDashStarted();
	}

	bIsDashing = true;
	bHasDashed = true;

	PreDashGravityScale = GetCharacterMovement()->GravityScale;
	PreDashVelocity = GetCharacterMovement()->Velocity;
	GetCharacterMovement()->GravityScale = 0.0f;
	const float DashDirection = !FMath::IsNearlyZero(ActionValueY) ? FMath::Sign(ActionValueY) : FMath::Sign(GetActorForwardVector().X);
	GetCharacterMovement()->Velocity = FVector(DashDirection * DashHorizontalSpeed, 0.0f, 0.0f);

	SetJumpTrailState(true);
	if (bChargedDashAttack)
	{
		GetWorldTimerManager().SetTimer(ChargedDashStrikeTimer, this, &ASideScrollingCharacter::PerformChargedDashStrike, FMath::Max(0.01f, ChargedDashStrikeDelay), false);
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

	// Never leave the character floating if the montage could not play.
	if (bChargedDashAttack) PerformChargedDashStrike();
	EndDash();
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
	GetWorldTimerManager().ClearTimer(ChargedDashStrikeTimer);
	bChargedDashAttack = false;
	if (GetCharacterMovement()->IsFalling())
	{
		// Root motion can leave the movement velocity at zero when a dash ends.
		// Retain the jump's rise or fall, and any horizontal speed it had gained.
		FVector ExitVelocity = GetCharacterMovement()->Velocity;
		if (FMath::IsNearlyZero(ExitVelocity.X)) ExitVelocity.X = PreDashVelocity.X;
		ExitVelocity.Z = PreDashVelocity.Z;
		GetCharacterMovement()->Velocity = ExitVelocity;
	}
	SetJumpTrailState(false);

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
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance()) AnimInstance->Montage_Stop(0.05f, DashMontage);
		EndDash();
	}

	if (bIsAttacking)
	{
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();
		if (!bIsChargingAttack) bComboAdvanceQueued = true;
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
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance()) AnimInstance->Montage_Stop(0.05f, DashMontage);
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
		}
		else if (!bHasDoubleJumped)
		{
			bHasDoubleJumped = true;
			Jump();
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
	CachedAttackInputTime = -1000.0f;
	bIsAttacking = false;
	bIsChargingAttack = false;
	bHasLoopedChargedAttack = false;
	bHasReleasedChargedAttack = false;
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

	NotifyEnemiesOfIncomingAttack();

	if (ComboAttackMontage)
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

	bIsAttacking = false;
}

void ASideScrollingCharacter::ChargedAttack()
{
	++CurrentStrikeSerial;
	HitActorsThisStrike.Reset();
	bIsAttacking = true;
	bComboAdvanceQueued = false;
	bHasLoopedChargedAttack = false;
	bHasReleasedChargedAttack = false;

	NotifyEnemiesOfIncomingAttack();

	if (ChargedAttackMontage)
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

	bIsAttacking = false;
}

void ASideScrollingCharacter::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bIsAttacking = false;
	bComboAdvanceQueued = false;
	bHasLoopedChargedAttack = false;
	bHasReleasedChargedAttack = false;

	if (CurrentHP <= 0.0f || bInterrupted)
	{
		CachedAttackInputTime = -1000.0f;
		return;
	}

	if (CachedAttackInputTime >= 0.0f && GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= AttackInputCacheTimeTolerance)
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

void ASideScrollingCharacter::DoAttackTrace(FName DamageSourceBone)
{
	TArray<FHitResult> OutHits;

	const FVector TraceStart = DamageSourceBone.IsNone() ? GetActorLocation() + FVector(0.0f, 0.0f, 25.0f) : GetMesh()->GetSocketLocation(DamageSourceBone);
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

				Damageable->ApplyDamage(MeleeDamage, this, CurrentHit.ImpactPoint, Impulse);
				if (!CombatEnemy || CombatEnemy->CurrentHP < HPBefore || (SideEnemy && SideEnemy->GetAcceptedStrikes() > StrikesBefore))
				{
					DealtDamage(MeleeDamage, CurrentHit.ImpactPoint);
					RegisterHitCombo();
				}
			}
		}
	}
}

void ASideScrollingCharacter::CheckCombo()
{
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

void ASideScrollingCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	if (ActualDamage > 0.0f)
	{
		CachedAttackInputTime = -1000.0f;
		bComboAdvanceQueued = false;
		bIsAttacking = false;
		bIsChargingAttack = false;
		ResetHitCombo();
		if (bIsDashing) EndDash();
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.1f, ComboAttackMontage);
			AnimInstance->Montage_Stop(0.1f, ChargedAttackMontage);
			AnimInstance->Montage_Stop(0.1f, DashMontage);
		}
		GetCharacterMovement()->AddImpulse(DamageImpulse, true);

		if (GetMesh()->IsSimulatingPhysics())
		{
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}

		ReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
	}
}

void ASideScrollingCharacter::HandleDeath()
{
	if (bIsDashing)
	{
		EndDash();
	}

	bIsAttacking = false;
	bIsChargingAttack = false;
	ResetHitCombo();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.15f);
	}

	GetCharacterMovement()->DisableMovement();
	GetMesh()->SetSimulatePhysics(true);

	if (LifeBar)
	{
		LifeBar->SetHiddenInGame(true);
	}

	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		Run->EndRun(EQuasicomboRunOutcome::Defeat);
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

		// Match the Combat template's partial hit reaction while preserving
		// the SideScroller movement capsule.
		GetMesh()->SetPhysicsBlendWeight(0.5f);
		GetMesh()->SetBodySimulatePhysics(PelvisBoneName, false);
	}

	return ActualDamage;
}
