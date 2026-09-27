#include "QuasicomboBarrier.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AQuasicomboBarrier::AQuasicomboBarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	SetRootComponent(Blocker);
	Blocker->SetBoxExtent(FVector(45, 120, 170));
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallingVisual"));
	Visual->SetupAttachment(Blocker);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Visual->SetStaticMesh(Cube.Object);
	Visual->SetRelativeScale3D(FVector(0.9f, 2.4f, 3.4f));
}

void AQuasicomboBarrier::BeginPlay()
{
	Super::BeginPlay();
	// A route section can Prepare before this actor receives BeginPlay. Keep its
	// decision and the mesh's original landing location in either begin order.
	LandingLocation = FVector::ZeroVector;
	if (!bConfigured) SetClosed(bInitiallyClosed);
}

void AQuasicomboBarrier::SetClosed(bool bClosed)
{
	if (bIsClosed == bClosed && bConfigured) return;
	bIsClosed = bClosed;
	bConfigured = true;
	Blocker->SetCollisionEnabled(bClosed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Visual->SetHiddenInGame(!bClosed);
	if (bClosed)
	{
		FallElapsed = 0.0f;
		Visual->SetRelativeLocation(LandingLocation + FVector(0, 0, FallHeight));
		SetActorTickEnabled(true);
	}
	else
	{
		SetActorTickEnabled(false);
	}
}

void AQuasicomboBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FallElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(FallElapsed / FMath::Max(0.01f, FallDuration), 0.0f, 1.0f);
	Visual->SetRelativeLocation(FMath::Lerp(LandingLocation + FVector(0, 0, FallHeight), LandingLocation, Alpha));
	if (Alpha >= 1.0f) SetActorTickEnabled(false);
}
