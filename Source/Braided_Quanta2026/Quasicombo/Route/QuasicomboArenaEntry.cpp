#include "QuasicomboArenaEntry.h"
#include "QuasicomboRouteLeg.h"
#include "QuasicomboRunSubsystem.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "QuasicomboBoss.h"
#include "SideScrollingCharacter.h"
#include "Kismet/GameplayStatics.h"

AQuasicomboArenaEntry::AQuasicomboArenaEntry()
{
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaEntryTrigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(100, 150, 180));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
}

void AQuasicomboArenaEntry::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AQuasicomboArenaEntry::HandleOverlap);
}

void AQuasicomboArenaEntry::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!Other || !Other->ActorHasTag(TEXT("Player")) || !FinalLeg || !FinalLeg->IsCleared()) return;
	UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	if (!Run || Run->GetOutcome() != EQuasicomboRunOutcome::Playing || Run->GetCurrentSection() != 3 ||
		Run->GetRouteHistory().Last() != Lane) return;
	if (ACharacter* Character = Cast<ACharacter>(Other))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->SetActorLocation(ArrivalLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(UGameplayStatics::GetActorOfClass(this, AQuasicomboBoss::StaticClass())))
			Boss->StartEncounter(Cast<ASideScrollingCharacter>(Character));
	}
}
