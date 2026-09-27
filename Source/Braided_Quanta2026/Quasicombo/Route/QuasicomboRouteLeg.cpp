#include "QuasicomboRouteLeg.h"
#include "QuasicomboRouteSection.h"
#include "QuasicomboBarrier.h"
#include "CombatEnemy.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AQuasicomboRouteLeg::AQuasicomboRouteLeg()
{
	EntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryTrigger"));
	SetRootComponent(EntryTrigger);
	EntryTrigger->SetBoxExtent(FVector(85, 100, 145));
	EntryTrigger->SetCollisionProfileName(TEXT("Trigger"));
}

void AQuasicomboRouteLeg::BeginPlay()
{
	Super::BeginPlay();
	EntryTrigger->OnComponentBeginOverlap.AddDynamic(this, &AQuasicomboRouteLeg::HandleOverlap);
}

void AQuasicomboRouteLeg::Prepare()
{
	if (ForwardBarrier) ForwardBarrier->SetClosed(true);
	SetEnemiesActive(false);
}

void AQuasicomboRouteLeg::SetEnemiesActive(bool bActive)
{
	for (ACombatEnemy* Enemy : Enemies)
	{
		if (!IsValid(Enemy)) continue;
		Enemy->SetActorHiddenInGame(!bActive);
		Enemy->SetActorEnableCollision(bActive);
		Enemy->SetActorTickEnabled(bActive);
		if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		{
			if (bActive) Movement->SetMovementMode(MOVE_Walking); else Movement->DisableMovement();
		}
		if (AAIController* AI = Cast<AAIController>(Enemy->GetController()))
		{
			AI->StopMovement();
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				if (bActive) Brain->RestartLogic(); else Brain->StopLogic(TEXT("Inactive route"));
			}
		}
	}
}

void AQuasicomboRouteLeg::Select()
{
	bSelected = true;
	if (ForwardBarrier) ForwardBarrier->SetClosed(true);
	SetEnemiesActive(true);
	for (ACombatEnemy* Enemy : Enemies)
	{
		if (!IsValid(Enemy)) continue;
		Enemy->OnEnemyDied.AddUniqueDynamic(this, &AQuasicomboRouteLeg::CheckEncounter);
		Enemy->OnDestroyed.AddUniqueDynamic(this, &AQuasicomboRouteLeg::HandleEnemyDestroyed);
	}
	CheckEncounter();
}

void AQuasicomboRouteLeg::Reject()
{
	if (ForwardBarrier) ForwardBarrier->SetClosed(true);
	SetEnemiesActive(false);
}

bool AQuasicomboRouteLeg::IsCleared() const
{
	for (ACombatEnemy* Enemy : Enemies)
	{
		if (IsValid(Enemy) && !RemovedEnemies.Contains(TWeakObjectPtr<AActor>(Enemy)) && Enemy->CurrentHP > 0.0f) return false;
	}
	return true;
}

void AQuasicomboRouteLeg::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	RemovedEnemies.Add(TWeakObjectPtr<AActor>(DestroyedActor));
	CheckEncounter();
}

void AQuasicomboRouteLeg::CheckEncounter()
{
	if (!bSelected || bClearNotified || !IsCleared()) return;
	bClearNotified = true;
	if (ForwardBarrier) ForwardBarrier->SetClosed(false);
	OnEncounterCleared();
}

void AQuasicomboRouteLeg::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (Other && Other->ActorHasTag(TEXT("Player")) && Section) Section->CommitLane(Lane);
}
