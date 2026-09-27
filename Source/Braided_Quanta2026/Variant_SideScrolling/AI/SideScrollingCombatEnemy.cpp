#include "SideScrollingCombatEnemy.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "SideScrollingCharacter.h"

ASideScrollingCombatEnemy::ASideScrollingCombatEnemy()
{
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector::YAxisVector);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 180.0f;
}

void ASideScrollingCombatEnemy::BeginPlay()
{
	MaxHP = static_cast<float>(FMath::Max(1, RequiredHits));
	Super::BeginPlay();
	SpawnX = GetActorLocation().X;
	GetCharacterMovement()->SetPlaneConstraintOrigin(GetActorLocation());
	OnAttackCompleted.BindUObject(this, &ASideScrollingCombatEnemy::FinishSideAttack);
}

void ASideScrollingCombatEnemy::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTraceFallbackTimer);
	GetWorldTimerManager().ClearTimer(AttackTimeoutTimer);
	OnAttackCompleted.Unbind();
	Super::EndPlay(EndPlayReason);
}

bool ASideScrollingCombatEnemy::HasFloorAhead(float Direction) const
{
	const FVector Start = GetActorLocation() + FVector(Direction * 95.0f, 0.0f, -70.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SideEnemyLedge), false, this);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 180), ECC_Visibility, Params);
}

void ASideScrollingCombatEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CurrentHP <= 0 || IsCombatDefeated()) { CombatState = ESideCombatState::Dead; return; }
	if (bSideAttackActive) return;
	if ((CombatState == ESideCombatState::HitReaction || CombatState == ESideCombatState::Recovery) && GetWorld()->GetTimeSeconds() < StateEndsAt) return;
	AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(Player)) { CombatState = ESideCombatState::Idle; return; }
	const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	const float DistanceX = FMath::Abs(ToPlayer.X);
	if (FMath::Abs(ToPlayer.Z) > 180.0f || FMath::Abs(ToPlayer.Y) > 150.0f || DistanceX > AggroRange)
	{
		CombatState = ESideCombatState::Idle;
		return;
	}
	const float Direction = FMath::Sign(ToPlayer.X);
	SetActorRotation(FRotator(0, Direction >= 0 ? 0.0f : 180.0f, 0));
	if (DistanceX <= AttackRange)
	{
		if (GetWorld()->GetTimeSeconds() >= NextAttackTime)
		{
			NextAttackTime = GetWorld()->GetTimeSeconds() + AttackCooldown;
			StartSideAttack();
		}
		else CombatState = ESideCombatState::Recovery;
		return;
	}
	if (FMath::Abs(GetActorLocation().X + Direction * 80.0f - SpawnX) <= PatrolHalfWidth && HasFloorAhead(Direction))
	{
		CombatState = ESideCombatState::Approach;
		AddMovementInput(FVector(Direction, 0, 0), 1.0f);
	}
	else CombatState = ESideCombatState::Idle;
}

void ASideScrollingCombatEnemy::StartSideAttack()
{
	const bool bChargedAttack = ShouldUseChargedSideAttack() && ChargedAttackMontage;
	bSideAttackActive = true;
	bAttackTraceFired = false;
	CombatState = ESideCombatState::Attack;
	const float StrikeDelay = bChargedAttack ? FMath::Max(AttackWindup, ChargedAttackWindup) : AttackWindup;
	GetWorldTimerManager().SetTimer(AttackTraceFallbackTimer, this, &ASideScrollingCombatEnemy::FallbackAttackTrace, FMath::Max(0.01f, StrikeDelay), false);
	GetWorldTimerManager().SetTimer(AttackTimeoutTimer, this, &ASideScrollingCombatEnemy::TimeoutSideAttack, FMath::Max(0.1f, AttackTimeout), false);

	// The montage notify is the normal hit timing. A timed trace covers a missing
	// notify or slot without ever causing a second hit in this attack.
	if ((bChargedAttack ? ChargedAttackMontage : ComboAttackMontage) && GetMesh() && GetMesh()->GetAnimInstance())
	{
		bStartingSideAttack = true;
		if (bChargedAttack) DoAIChargedAttack();
		else
		{
			DoAIComboAttack();
			TargetComboCount = 1;
		}
		bStartingSideAttack = false;
		if (bIsAttacking) return;
	}
	// A mannequin without a working montage still attacks after its windup.
}

void ASideScrollingCombatEnemy::FallbackAttackTrace()
{
	if (!bSideAttackActive || CurrentHP <= 0.0f || IsCombatDefeated()) return;
	DoAttackTrace(NAME_None);
	if (!bIsAttacking) FinishSideAttack();
}

void ASideScrollingCombatEnemy::TimeoutSideAttack()
{
	if (!bSideAttackActive) return;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.05f, ComboAttackMontage);
		AnimInstance->Montage_Stop(0.05f, ChargedAttackMontage);
	}
	bIsAttacking = false;
	FinishSideAttack();
}

void ASideScrollingCombatEnemy::FinishSideAttack()
{
	if (!bSideAttackActive) return;
	// Montage_Play can fail synchronously. Keep the windup timer in that case.
	if (bStartingSideAttack) return;
	if (CombatState != ESideCombatState::Dead && CombatState != ESideCombatState::HitReaction && !bAttackTraceFired)
	{
		DoAttackTrace(NAME_None);
	}
	GetWorldTimerManager().ClearTimer(AttackTraceFallbackTimer);
	GetWorldTimerManager().ClearTimer(AttackTimeoutTimer);
	bSideAttackActive = false;
	if (CombatState == ESideCombatState::Attack)
	{
		CombatState = ESideCombatState::Recovery;
		StateEndsAt = GetWorld()->GetTimeSeconds() + RecoveryDuration;
	}
}

void ASideScrollingCombatEnemy::DoAttackTrace(FName DamageSourceBone)
{
	if (!bSideAttackActive || bAttackTraceFired || CurrentHP <= 0.0f || IsCombatDefeated()) return;
	bAttackTraceFired = true;

	const FVector TraceStart = GetActorLocation() + FVector(0.0f, 0.0f, 25.0f);
	const FVector TraceEnd = TraceStart + GetActorForwardVector() * FMath::Max(MeleeTraceDistance, AttackRange - 35.0f);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SideEnemyAttack), false, this);
	FCollisionShape Shape = FCollisionShape::MakeSphere(MeleeTraceRadius);
	TArray<FHitResult> Hits;
	TSet<AActor*> DamagedActors;
	if (GetWorld()->SweepMultiByObjectType(Hits, TraceStart, TraceEnd, FQuat::Identity, Objects, Shape, Params))
	{
		for (const FHitResult& Hit : Hits)
		{
			AActor* Target = Hit.GetActor();
			if (!IsValid(Target) || !Target->ActorHasTag(TEXT("Player")) || DamagedActors.Contains(Target)) continue;
			if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target))
			{
				DamagedActors.Add(Target);
				const FVector Impulse = GetActorForwardVector() * MeleeKnockbackImpulse + FVector::UpVector * MeleeLaunchImpulse;
				Damageable->ApplyDamage(MeleeDamage, this, Hit.ImpactPoint, Impulse);
			}
		}
	}
}

void ASideScrollingCombatEnemy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (Damage <= 0.0f || CurrentHP <= 0.0f || AcceptedStrikes >= RequiredHits || IsCombatDefeated()) return;
	if (const ASideScrollingCharacter* Player = Cast<ASideScrollingCharacter>(DamageCauser))
	{
		const int64 IncomingSerial = Player->GetCurrentStrikeSerial();
		if (IncomingSerial > 0 && LastStrikeCauser.Get() == Player && LastAcceptedStrikeSerial == IncomingSerial) return;
		LastStrikeCauser = DamageCauser;
		LastAcceptedStrikeSerial = IncomingSerial;
	}
	bSideAttackActive = false;
	GetWorldTimerManager().ClearTimer(AttackTraceFallbackTimer);
	GetWorldTimerManager().ClearTimer(AttackTimeoutTimer);
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.05f, ComboAttackMontage);
		AnimInstance->Montage_Stop(0.05f, ChargedAttackMontage);
	}
	bIsAttacking = false;
	++AcceptedStrikes;
	if (AcceptedStrikes >= RequiredHits)
	{
		HandleRequiredHitsReached(DamageCauser, DamageLocation, DamageImpulse);
	}
	else
	{
		CombatState = ESideCombatState::HitReaction;
		StateEndsAt = GetWorld()->GetTimeSeconds() + HitReactionDuration;
		Super::ApplyDamage(1.0f, DamageCauser, DamageLocation, DamageImpulse);
	}
	OnAcceptedStrike(AcceptedStrikes, RequiredHits);
	OnStrikeAccepted(AcceptedStrikes);
}

void ASideScrollingCombatEnemy::HandleRequiredHitsReached(AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	CombatState = ESideCombatState::Dead;
	Super::ApplyDamage(FMath::Max(1.0f, CurrentHP), DamageCauser, DamageLocation, DamageImpulse);
}

void ASideScrollingCombatEnemy::OnStrikeAccepted(int32)
{
}

bool ASideScrollingCombatEnemy::ShouldUseChargedSideAttack() const
{
	return false;
}
