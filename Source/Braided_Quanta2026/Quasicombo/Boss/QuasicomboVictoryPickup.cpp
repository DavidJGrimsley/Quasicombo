#include "QuasicomboVictoryPickup.h"
#include "QuasicomboBoss.h"
#include "SideScrollingCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AQuasicomboVictoryPickup::AQuasicomboVictoryPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	SetRootComponent(Trigger);
	Trigger->InitSphereRadius(68.0f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AQuasicomboVictoryPickup::HandleOverlap);

	Orb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VictoryOrb"));
	Orb->SetupAttachment(Trigger);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RadicalBuster(
		TEXT("/Game/RadicalMike/Mesh/SM_RadicalBuster.SM_RadicalBuster"));
	if (RadicalBuster.Succeeded()) Orb->SetStaticMesh(RadicalBuster.Object);
	Orb->SetRelativeScale3D(FVector(1.08f));
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Orb->SetRenderCustomDepth(true);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
	Label->SetupAttachment(Trigger);
	Label->SetText(FText::FromString(TEXT("Buster Cannon")));
	Label->SetTextRenderColor(FColor(75, 235, 255));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(25.0f);
	Label->SetRelativeLocation(FVector(0, 0, 82));
	Label->SetRelativeRotation(FRotator(0, 90, 0));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("PickupGlow"));
	Glow->SetupAttachment(Trigger);
	Glow->SetLightColor(FLinearColor(0.1f, 0.8f, 1.0f));
	Glow->SetIntensity(2800.0f);
	Glow->SetAttenuationRadius(250.0f);
}

void AQuasicomboVictoryPickup::Initialize(AQuasicomboBoss* InBoss, ASideScrollingCharacter* InPlayer)
{
	Boss = InBoss;
	Player = InPlayer;
}

void AQuasicomboVictoryPickup::BeginPlay()
{
	Super::BeginPlay();
	// A pickup is spawned at runtime, so apply the authored mesh to the spawned
	// component as well as the class default. Never leave a stale sphere visible.
	UStaticMesh* BusterMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/RadicalMike/Mesh/SM_RadicalBuster.SM_RadicalBuster"));
	if (!BusterMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Quasicombo: SM_RadicalBuster failed to load for victory pickup"));
	}
	Orb->SetStaticMesh(BusterMesh);
	ArmAt = GetWorld()->GetTimeSeconds() + 0.6f;
}

void AQuasicomboVictoryPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Orb->SetRelativeLocation(FVector(0, 0, FMath::Sin(Age * 3.0f) * 9.0f));
	Orb->AddLocalRotation(FRotator(0, DeltaSeconds * 70.0f, 0));
	if (!bCollected && GetWorld()->GetTimeSeconds() >= ArmAt && Player.IsValid() && Trigger->IsOverlappingActor(Player.Get()))
		TryCollect(Player.Get());
}

void AQuasicomboVictoryPickup::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	TryCollect(OtherActor);
}

void AQuasicomboVictoryPickup::TryCollect(AActor* OtherActor)
{
	if (bCollected || !GetWorld() || GetWorld()->GetTimeSeconds() < ArmAt || OtherActor != Player.Get()) return;
	if (AQuasicomboBoss* OwnerBoss = Boss.Get(); OwnerBoss && OwnerBoss->ClaimVictory())
	{
		bCollected = true;
		SetActorEnableCollision(false);
		Destroy();
	}
}
