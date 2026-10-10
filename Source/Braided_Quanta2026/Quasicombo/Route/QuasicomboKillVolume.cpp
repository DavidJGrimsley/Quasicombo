#include "QuasicomboKillVolume.h"
#include "QuasicomboRunSubsystem.h"
#include "CombatEnemy.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"

AQuasicomboKillVolume::AQuasicomboKillVolume()
{
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("KillTrigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(500, 150, 100));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
}

void AQuasicomboKillVolume::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AQuasicomboKillVolume::HandleOverlap);
}

void AQuasicomboKillVolume::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!Other) return;
	if (Other->ActorHasTag(TEXT("Player")))
	{
		if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
			if (!Run->TryHandleBossFailure()) Run->EndRun(EQuasicomboRunOutcome::Defeat);
	}
	else if (ACombatEnemy* Enemy = Cast<ACombatEnemy>(Other))
	{
		if (Enemy->CurrentHP > 0.0f) Enemy->TakeDamage(Enemy->CurrentHP, FDamageEvent(), nullptr, this);
	}
}
