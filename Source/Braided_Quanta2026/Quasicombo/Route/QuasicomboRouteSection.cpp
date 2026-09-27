#include "QuasicomboRouteSection.h"
#include "QuasicomboRouteLeg.h"
#include "QuasicomboRunSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AQuasicomboRouteSection::AQuasicomboRouteSection()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	RearGateBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("RearGateBlocker"));
	RearGateBlocker->SetupAttachment(Root);
	RearGateBlocker->SetVisibility(false);
	RearGateBlocker->SetHiddenInGame(true);
	RearGateBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AQuasicomboRouteSection::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);
	if (IsValid(RearGate))
	{
		UStaticMeshComponent* GateMesh = RearGate->GetStaticMeshComponent();
		if (IsValid(GateMesh) && IsValid(GateMesh->GetStaticMesh()))
		{
			RearGateLandingTransform = RearGate->GetActorTransform();
			GateMesh->UpdateBounds();
			RearGateClosedBounds = GateMesh->Bounds.GetBox();
			RearGateBlocker->SetBoxExtent(RearGateClosedBounds.GetExtent());
			RearGateBlocker->SetWorldLocation(RearGateClosedBounds.GetCenter());
			RearGateBlocker->SetCollisionProfileName(TEXT("BlockAll"));
			RearGateBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GateMesh->SetMobility(EComponentMobility::Movable);
			RearGate->SetActorEnableCollision(false);
			RearGate->SetActorHiddenInGame(true);
			bRearGateReady = true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Rear gate for section %d has no static mesh"), SectionNumber);
		}
	}
	for (AQuasicomboRouteLeg* Leg : Legs) if (IsValid(Leg)) Leg->Prepare();
}

void AQuasicomboRouteSection::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bRearClosurePending) TryCloseRearGate();
	if (bRearGateFalling && IsValid(RearGate))
	{
		RearGateFallElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(RearGateFallElapsed / FMath::Max(0.01f, RearGateFallDuration), 0.0f, 1.0f);
		const FVector Location = RearGateLandingTransform.GetLocation() + FVector::UpVector * FMath::Lerp(RearGateFallHeight, 0.0f, Alpha);
		RearGate->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		if (Alpha >= 1.0f) bRearGateFalling = false;
	}
	if (!bRearClosurePending && !bRearGateFalling) SetActorTickEnabled(false);
}

void AQuasicomboRouteSection::TryCloseRearGate()
{
	if (!bRearClosurePending || !bRearGateReady || bRearGateClosed) return;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(Player)) return;
	// Route progression is +X. Wait until the player's entire body clears the authored gate.
	if (Player->GetComponentsBoundingBox().Min.X <= RearGateClosedBounds.Max.X + RearGateClearance) return;

	bRearClosurePending = false;
	bRearGateClosed = true;
	RearGateBlocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RearGateFallElapsed = 0.0f;
	RearGate->SetActorLocation(RearGateLandingTransform.GetLocation() + FVector::UpVector * RearGateFallHeight,
		false, nullptr, ETeleportType::TeleportPhysics);
	RearGate->SetActorHiddenInGame(false);
	bRearGateFalling = RearGateFallHeight > 0.0f;
	if (!bRearGateFalling)
	{
		RearGate->SetActorLocation(RearGateLandingTransform.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool AQuasicomboRouteSection::CommitLane(EQuasicomboLane Lane)
{
	// A misplaced or unlisted trigger must not consume one of the three choices.
	bool bHasMatchingLeg = false;
	for (const AQuasicomboRouteLeg* Leg : Legs)
	{
		if (IsValid(Leg) && Leg->Section == this && Leg->Lane == Lane)
		{
			bHasMatchingLeg = true;
			break;
		}
	}
	if (!bHasMatchingLeg) return false;

	UQuasicomboRunSubsystem* Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (!Run || !Run->TryCommitRoute(SectionNumber, Lane)) return false;
	for (AQuasicomboRouteLeg* Leg : Legs)
	{
		if (!IsValid(Leg)) continue;
		if (Leg->Lane == Lane) Leg->Select(); else Leg->Reject();
	}
	if (bRearGateReady && !bRearGateClosed)
	{
		bRearClosurePending = true;
		SetActorTickEnabled(true);
		TryCloseRearGate();
	}
	return true;
}
