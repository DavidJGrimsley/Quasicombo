#include "QuasicomboBoss.h"
#include "QuantumBossComponent.h"
#include "QuasicomboQTEComponent.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "SideScrollingCharacter.h"
#include "CombatLifeBar.h"
#include "QuasicomboRunSubsystem.h"

AQuasicomboBoss::AQuasicomboBoss()
{
	RequiredHits = 6;
	MaxHP = 6.0f;
	AggroRange = 800.0f;
	AttackCooldown = 1.8f;
	MinChargeLoops = 1;
	MaxChargeLoops = 2;
	Quantum = CreateDefaultSubobject<UQuantumBossComponent>(TEXT("QuantumState"));
	QTE = CreateDefaultSubobject<UQuasicomboQTEComponent>(TEXT("FinalQTE"));
}

void AQuasicomboBoss::BeginPlay()
{
	Super::BeginPlay();
	Quantum->OnQuantumStateReady.AddDynamic(this, &AQuasicomboBoss::ApplyQuantumTuning);
	if (Quantum->HasQuantumState())
	{
		ApplyQuantumTuning(Quantum->GetVacuumProbability(), Quantum->GetTauProbability(), Quantum->UsedFallback());
	}
}

void AQuasicomboBoss::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreCameraAndTime();
	Super::EndPlay(EndPlayReason);
}

void AQuasicomboBoss::ApplyQuantumTuning(double, double Tau, bool)
{
	// Exact channel probability changes approach, warning, and recovery timing.
	const float Aggression = static_cast<float>(Tau);
	AttackCooldown = FMath::Lerp(2.0f, 0.8f, Aggression);
	AttackWindup = FMath::Lerp(0.42f, 0.16f, Aggression);
	ChargedAttackWindup = FMath::Lerp(1.1f, 0.7f, Aggression);
	RecoveryDuration = FMath::Lerp(0.65f, 0.25f, Aggression);
	GetCharacterMovement()->MaxWalkSpeed = FMath::Lerp(145.0f, 250.0f, Aggression);
}

void AQuasicomboBoss::HandleRequiredHitsReached(AActor*, const FVector&, const FVector&)
{
	CombatState = ESideCombatState::Dead;
	CurrentHP = 0.0f;
	if (LifeBarWidget) LifeBarWidget->SetLifePercentage(0.0f);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.1f);
	}
	BeginFinisher();
}

void AQuasicomboBoss::OnStrikeAccepted(int32 StrikeCount)
{
	if (StrikeCount == 3) Quantum->EvolveState();
}

bool AQuasicomboBoss::ShouldUseChargedSideAttack() const
{
	return !bFinisherStarted && ChargedAttackMontage && Quantum &&
		FMath::FRand() < Quantum->GetTauProbability();
}

void AQuasicomboBoss::BeginFinisher()
{
	if (bFinisherStarted) return;
	bFinisherStarted = true;
	const UQuasicomboRunSubsystem* Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (!Run || Run->GetOutcome() != EQuasicomboRunOutcome::Playing) return;
	Quantum->FreezeForFinisher();
	const bool bTauPattern = FMath::FRand() < Quantum->GetTauProbability();
	if (ASideScrollingCharacter* Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->PrepareForFinisher();
		FrozenPlayer = Player;
		if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
		{
			PreviousMovementMode = Movement->MovementMode;
			Movement->DisableMovement();
			bMovementFrozen = true;
		}
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PreviousTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
		UGameplayStatics::SetGlobalTimeDilation(this, FinisherTimeDilation);
		bCameraOverride = true;
		PreviousViewTarget = PC->GetViewTarget();
		if (CloseupCamera) PC->SetViewTargetWithBlend(CloseupCamera, 0.2f);
	}
	OnFinisherStarted(bTauPattern);
	QTE->BeginQTE(bTauPattern);
}

bool AQuasicomboBoss::SubmitQTEInput(EQuasicomboQTEInput Input)
{
	return QTE && QTE->SubmitPrompt(Input);
}

void AQuasicomboBoss::ShowImpactCamera()
{
	if (bCameraOverride && ImpactCamera)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetViewTargetWithBlend(ImpactCamera, 0.15f);
	}
}

void AQuasicomboBoss::RestoreCameraAndTime()
{
	if (bMovementFrozen)
	{
		bMovementFrozen = false;
		if (ASideScrollingCharacter* Player = FrozenPlayer.Get())
		{
			if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
			{
				Movement->SetMovementMode(PreviousMovementMode);
			}
		}
		FrozenPlayer.Reset();
	}
	if (!bCameraOverride || !GetWorld()) return;
	bCameraOverride = false;
	UGameplayStatics::SetGlobalTimeDilation(this, PreviousTimeDilation);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		AActor* RestoreTarget = PreviousViewTarget.Get();
		if (!IsValid(RestoreTarget)) RestoreTarget = PC->GetPawn();
		if (IsValid(RestoreTarget)) PC->SetViewTargetWithBlend(RestoreTarget, 0.25f);
	}
	PreviousViewTarget.Reset();
}
