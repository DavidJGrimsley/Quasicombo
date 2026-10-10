#include "QuasicomboBossApproachVolume.h"
#include "QuasicomboBoss.h"
#include "QuasicomboEncounterLightingComponent.h"
#include "QuasicomboMusicComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "SideScrollingGameMode.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AQuasicomboBossApproachVolume::AQuasicomboBossApproachVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ApproachTrigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(1550, 400, 1300));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetHiddenInGame(true);
}

void AQuasicomboBossApproachVolume::BeginPlay()
{
	Super::BeginPlay();
	if (!IsValid(Boss))
	{
		for (TActorIterator<AQuasicomboBoss> It(GetWorld()); It; ++It)
			if (!Boss || FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) <
				FVector::DistSquared(Boss->GetActorLocation(), GetActorLocation())) Boss = *It;
	}
	Trigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &AQuasicomboBossApproachVolume::HandleBeginOverlap);
}

bool AQuasicomboBossApproachVolume::ProjectProgress(const FVector& Position, const FVector& Start, const FVector& End, float& RawProgress)
{
	const FVector2D Direction(End.X - Start.X, End.Z - Start.Z);
	const double LengthSquared = Direction.SizeSquared();
	if (Position.ContainsNaN() || Start.ContainsNaN() || End.ContainsNaN() || LengthSquared <= UE_SMALL_NUMBER)
		return false;
	RawProgress = FVector2D::DotProduct(FVector2D(Position.X - Start.X, Position.Z - Start.Z), Direction) / LengthSquared;
	return FMath::IsFinite(RawProgress);
}

bool AQuasicomboBossApproachVolume::CanControlEffects()
{
	if (!IsValid(Boss) || !Boss->EncounterLighting) return false;
	bEncounterOwnsEffects = Boss->GetBossPhase() != EQuasicomboBossPhase::Waiting || Boss->EncounterLighting->HasEncounterControl();
	const auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	return !bEncounterOwnsEffects && (!Run || Run->GetOutcome() == EQuasicomboRunOutcome::Playing);
}

void AQuasicomboBossApproachVolume::ApplyProgress(float NewProgress)
{
	Progress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
	if (!CanControlEffects()) return;
	Boss->EncounterLighting->SetApproachProgress(Progress, StartIntensity, EndIntensity);
	if (auto* Mode = Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(this)))
		Mode->MusicController->SetApproachProgress(Progress);
	bAppliedApproach = true;
}

bool AQuasicomboBossApproachVolume::ApplyPlayerLocation(const FVector& Position)
{
	float RawProgress = 0.0f;
	if (!ProjectProgress(Position, GetActorTransform().TransformPosition(StartPoint),
		GetActorTransform().TransformPosition(EndPoint), RawProgress))
	{
		if (!bLoggedInvalidPoints)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s has invalid or coincident approach endpoints; effects were skipped"), *GetName());
			bLoggedInvalidPoints = true;
		}
		return false;
	}
	if (!CanControlEffects()) return false;
	ApplyProgress(RawProgress);
	return true;
}

void AQuasicomboBossApproachVolume::FinishOverlap(const FVector& Position)
{
	float RawProgress = 0.0f;
	if (ProjectProgress(Position, GetActorTransform().TransformPosition(StartPoint),
		GetActorTransform().TransformPosition(EndPoint), RawProgress))
	{
		if (RawProgress <= 0.0f) ApplyProgress(0.0f);
		else if (RawProgress >= 1.0f) ApplyProgress(1.0f);
		// Side exits retain the last progress. Re-entry resumes continuous position control.
	}
	bPlayerInside = false;
	TrackedPlayer.Reset();
	SetActorTickEnabled(false);
}

void AQuasicomboBossApproachVolume::HandleBeginOverlap(UPrimitiveComponent*, AActor* Other,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || Other != Player || !CanControlEffects()) return;
	TrackedPlayer = Player;
	bPlayerInside = true;
	SetActorTickEnabled(true);
	ApplyPlayerLocation(Player->GetActorLocation());
}

void AQuasicomboBossApproachVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!CanControlEffects()) { SetActorTickEnabled(false); return; }
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!bCheckedInitialOverlap)
	{
		bCheckedInitialOverlap = true;
		// Also handles players spawned/teleported inside before overlap delegates were bound.
		if (Player && Trigger->IsOverlappingActor(Player)) { TrackedPlayer = Player; bPlayerInside = true; }
	}
	if (!TrackedPlayer.IsValid()) { SetActorTickEnabled(false); return; }
	if (!Trigger->IsOverlappingActor(TrackedPlayer.Get())) { FinishOverlap(TrackedPlayer->GetActorLocation()); return; }
	ApplyPlayerLocation(TrackedPlayer->GetActorLocation());
}

void AQuasicomboBossApproachVolume::EndPlay(EEndPlayReason::Type Reason)
{
	if (bAppliedApproach && CanControlEffects())
	{
		Boss->EncounterLighting->Restore();
		if (auto* Mode = Cast<ASideScrollingGameMode>(UGameplayStatics::GetGameMode(this)))
			Mode->MusicController->SetApproachProgress(0.0f);
	}
	Super::EndPlay(Reason);
}
