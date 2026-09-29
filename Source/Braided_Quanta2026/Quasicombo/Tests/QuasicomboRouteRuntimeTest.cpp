#if WITH_DEV_AUTOMATION_TESTS

#include "Quasicombo/Route/QuasicomboArenaEntry.h"
#include "Quasicombo/Route/QuasicomboBarrier.h"
#include "Quasicombo/Route/QuasicomboKillVolume.h"
#include "Quasicombo/Route/QuasicomboRouteLeg.h"
#include "Quasicombo/Route/QuasicomboRouteSection.h"
#include "Quasicombo/Route/QuasicomboRunSubsystem.h"
#include "Variant_SideScrolling/AI/SideScrollingCombatEnemy.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace
{
void EnterTrigger(AActor* TriggerActor, AActor* Other)
{
	UBoxComponent* Box = TriggerActor->FindComponentByClass<UBoxComponent>();
	UPrimitiveComponent* OtherComponent = Other->FindComponentByClass<UPrimitiveComponent>();
	check(Box && OtherComponent);
	Box->OnComponentBeginOverlap.Broadcast(Box, Other, OtherComponent, 0, false, FHitResult());
}

template <typename T>
T* Spawn(UWorld* World, float X)
{
	return World->SpawnActor<T>(T::StaticClass(), FVector(X, 0.0f, 200.0f), FRotator::ZeroRotator);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboRouteRuntimeTest,
	"Quasicombo.Route.RuntimeGatesPitArena",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboRouteRuntimeTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	AQuasicomboRouteSection* Section = Spawn<AQuasicomboRouteSection>(World, 0.0f);
	Section->SectionNumber = 1;
	AStaticMeshActor* RearGate = Spawn<AStaticMeshActor>(World, 1800.0f);
	RearGate->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	RearGate->SetActorScale3D(FVector(1.0f, 3.0f, 3.0f));
	const FTransform AuthoredRearGateTransform = RearGate->GetActorTransform();
	Section->RearGate = RearGate;
	AQuasicomboRouteLeg* Legs[3] = {};
	ASideScrollingCombatEnemy* Enemies[3] = {};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float X = 1000.0f + Index * 1000.0f;
		AQuasicomboRouteLeg* Leg = Spawn<AQuasicomboRouteLeg>(World, X);
		Leg->Lane = static_cast<EQuasicomboLane>(Index);
		Leg->Section = Section;
		Leg->ForwardBarrier = Spawn<AQuasicomboBarrier>(World, X + 350.0f);
		Enemies[Index] = Spawn<ASideScrollingCombatEnemy>(World, X + 150.0f);
		Leg->Enemies.Add(Enemies[Index]);
		Section->Legs.Add(Leg);
		Legs[Index] = Leg;
	}
	ASideScrollingCombatEnemy* SecondBEnemy = Spawn<ASideScrollingCombatEnemy>(World, 2250.0f);
	Legs[1]->Enemies.Add(SecondBEnemy);
	AQuasicomboArenaEntry* Arena = Spawn<AQuasicomboArenaEntry>(World, 9000.0f);
	Arena->Lane = EQuasicomboLane::B;
	Arena->FinalLeg = Legs[1];
	Arena->ArrivalLocation = FVector(10000.0f, 0.0f, 200.0f);
	AQuasicomboKillVolume* Pit = Spawn<AQuasicomboKillVolume>(World, 12000.0f);
	if (!Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}

	ACharacter* Player = Spawn<ACharacter>(World, 1750.0f);
	Player->Tags.AddUnique(TEXT("Player"));
	Player->GetCharacterMovement()->DisableMovement();
	APlayerController* Controller = Spawn<APlayerController>(World, 0.0f);
	Controller->Possess(Player);
	TestTrue(TEXT("Test player is the gameplay player pawn"), UGameplayStatics::GetPlayerPawn(World, 0) == Player);
	UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	if (!TestNotNull(TEXT("Game world has a run subsystem"), Run)) return false;

	TestFalse(TEXT("Shared rear gate starts open"), Section->IsRearGateClosed());
	TestTrue(TEXT("Open rear gate visual is hidden"), RearGate->IsHidden());
	UBoxComponent* RearBlocker = Section->FindComponentByClass<UBoxComponent>();
	TestTrue(TEXT("Open rear gate has no gameplay collision"),
		RearBlocker && RearBlocker->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(FString::Printf(TEXT("Lane %d forward starts closed"), Index), Legs[Index]->ForwardBarrier->IsClosed());
		TestTrue(FString::Printf(TEXT("Lane %d enemy starts hidden"), Index), Enemies[Index]->IsHidden());
		TestFalse(FString::Printf(TEXT("Lane %d enemy collision starts disabled"), Index), Enemies[Index]->GetActorEnableCollision());
	}

	EnterTrigger(Legs[1], Player);
	TestEqual(TEXT("B1 overlap commits one route"), Run->GetCurrentSection(), 1);
	TestTrue(TEXT("B1 enemy activates"), !Enemies[1]->IsHidden() && Enemies[1]->GetActorEnableCollision());
	TestTrue(TEXT("B1 second enemy activates"), !SecondBEnemy->IsHidden() && SecondBEnemy->GetActorEnableCollision());
	TestTrue(TEXT("Unchosen exits seal"),
		Legs[0]->ForwardBarrier->IsClosed() && Legs[2]->ForwardBarrier->IsClosed());
	TestFalse(TEXT("Shared rear waits while player overlaps it"), Section->IsRearGateClosed());
	TestTrue(TEXT("Selected exit stays locked with live enemies"), Legs[1]->ForwardBarrier->IsClosed());
	EnterTrigger(Legs[0], Player);
	TestEqual(TEXT("Alternate trigger cannot replace B1"), Run->GetCurrentSection(), 1);

	Player->SetActorLocation(FVector(2000.0f, 0.0f, 200.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Fixture.TickTestWorld(0.01f);
	TestTrue(TEXT("Shared rear closes after the player clears it"), Section->IsRearGateClosed());
	TestFalse(TEXT("Closed rear gate visual is visible"), RearGate->IsHidden());
	TestTrue(TEXT("Shared rear has fixed gameplay collision while its visual falls"),
		RearBlocker && RearBlocker->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
	Fixture.TickTestWorld(0.4f);
	TestTrue(TEXT("Shared gate lands at its authored transform"), RearGate->GetActorTransform().Equals(AuthoredRearGateTransform, 0.1f));
	const FVector LandedLocation = RearGate->GetActorLocation();
	EnterTrigger(Legs[1], Player);
	Fixture.TickTestWorld(0.01f);
	TestTrue(TEXT("Duplicate overlap does not restart the rear gate fall"), RearGate->GetActorLocation().Equals(LandedLocation, 0.1f));
	Enemies[1]->CurrentHP = 0.0f;
	Enemies[1]->OnEnemyDied.Broadcast();
	TestTrue(TEXT("Exit stays locked until every assigned enemy is gone"), Legs[1]->ForwardBarrier->IsClosed());
	SecondBEnemy->Destroy();
	TestFalse(TEXT("Destroyed enemy clears the remaining encounter count"), Legs[1]->ForwardBarrier->IsClosed());
	TestTrue(TEXT("Selected encounter is cleared"), Legs[1]->IsCleared());

	EnterTrigger(Arena, Player);
	TestTrue(TEXT("Arena transfer refuses an incomplete route"), Player->GetActorLocation().X < 9000.0f);
	TestTrue(TEXT("B2 commits after B1"), Run->TryCommitRoute(2, EQuasicomboLane::B));
	TestTrue(TEXT("B3 commits after B2"), Run->TryCommitRoute(3, EQuasicomboLane::B));
	EnterTrigger(Arena, Player);
	TestEqual(TEXT("Cleared final lane transfers to arena"), Player->GetActorLocation().X, 10000.0);

	EnterTrigger(Pit, Enemies[0]);
	TestEqual(TEXT("Pit defeats an enemy"), Enemies[0]->CurrentHP, 0.0f);
	EnterTrigger(Pit, Player);
	TestTrue(TEXT("Pit ends the run in defeat"), Run->GetOutcome() == EQuasicomboRunOutcome::Defeat);
	Run->EndRun(EQuasicomboRunOutcome::Victory);
	TestTrue(TEXT("Run outcome cannot change after defeat"), Run->GetOutcome() == EQuasicomboRunOutcome::Defeat);

	Fixture.ForwardErrorMessages(this);
	return !Fixture.HasFailed();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboEmptyEncounterTest,
	"Quasicombo.Route.EmptyEncounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboEmptyEncounterTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	AQuasicomboRouteSection* Section = Spawn<AQuasicomboRouteSection>(World, 0.0f);
	Section->SectionNumber = 1;
	AQuasicomboRouteLeg* Leg = Spawn<AQuasicomboRouteLeg>(World, 1000.0f);
	Leg->Lane = EQuasicomboLane::A;
	Leg->Section = Section;
	Leg->ForwardBarrier = Spawn<AQuasicomboBarrier>(World, 1350.0f);
	Section->Legs.Add(Leg);
	if (!Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	ACharacter* Player = Spawn<ACharacter>(World, 1000.0f);
	Player->Tags.AddUnique(TEXT("Player"));
	APlayerController* Controller = Spawn<APlayerController>(World, 0.0f);
	Controller->Possess(Player);
	TestTrue(TEXT("Empty encounter starts behind a closed exit"), Leg->ForwardBarrier->IsClosed());
	EnterTrigger(Leg, Player);
	TestTrue(TEXT("Empty encounter clears on selection"), Leg->IsCleared());
	TestFalse(TEXT("Empty encounter opens its exit immediately"), Leg->ForwardBarrier->IsClosed());
	Fixture.ForwardErrorMessages(this);
	return !Fixture.HasFailed();
}

#endif
