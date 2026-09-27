#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SideScrollingCombatEnemy.h"
#include "SideScrollingCharacter.h"
#include "QuasicomboBoss.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboCombatStrikesTest,
	"Quasicombo.Combat.AcceptedStrikes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboCombatStrikesTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
		.AllowAudioPlayback(false)
		.RequiresHitProxies(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.CreateFXSystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("QuasicomboCombatTest"), nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Transient combat world"), World)) return false;

	auto MakeEnemy = [World](int32 RequiredHits, float X) -> ASideScrollingCombatEnemy*
	{
		ASideScrollingCombatEnemy* Enemy = World->SpawnActor<ASideScrollingCombatEnemy>(ASideScrollingCombatEnemy::StaticClass(), FVector(X, 0.0f, 100.0f), FRotator::ZeroRotator);
		if (Enemy)
		{
			Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
			Enemy->RequiredHits = RequiredHits;
			Enemy->DispatchBeginPlay();
		}
		return Enemy;
	};

	ASideScrollingCombatEnemy* Light = MakeEnemy(3, 0.0f);
	ASideScrollingCombatEnemy* Heavy = MakeEnemy(4, 1000.0f);
	ASideScrollingCombatEnemy* SixHitProxy = MakeEnemy(6, 2000.0f);
	UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	ASideScrollingCharacter* Player = PlayerClass
		? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(-1000.0f, 0.0f, 100.0f), FRotator::ZeroRotator)
		: nullptr;
	const bool bReady = TestNotNull(TEXT("Light enemy"), Light)
		&& TestNotNull(TEXT("Heavy enemy"), Heavy)
		&& TestNotNull(TEXT("Six-hit enemy"), SixHitProxy)
		&& TestNotNull(TEXT("Side-scroller player Blueprint"), Player);
	if (!bReady)
	{
		World->DestroyWorld(false);
		return false;
	}

	// A spawned actor in this test world has not begun play automatically.
	// The player's combat HP is initialized in BeginPlay, so start it before input.
	Player->DispatchBeginPlay();
	Player->DoComboAttackStart();
	TestTrue(TEXT("Starting a combo assigns a physical strike serial"), Player->GetCurrentStrikeSerial() > 0);
	const FVector Impact = Light->GetActorLocation();
	Light->ApplyDamage(99.0f, Player, Impact, FVector::ZeroVector);
	TestEqual(TEXT("A charged-scale damage amount still consumes one strike"), Light->GetAcceptedStrikes(), 1);
	Light->ApplyDamage(99.0f, Player, Impact, FVector::ZeroVector);
	TestEqual(TEXT("A second callback from the same player strike is ignored"), Light->GetAcceptedStrikes(), 1);
	Light->ApplyDamage(0.0f, nullptr, Impact, FVector::ZeroVector);
	TestEqual(TEXT("Zero damage cannot consume a strike"), Light->GetAcceptedStrikes(), 1);
	Light->ApplyDamage(1.0f, nullptr, Impact, FVector::ZeroVector);
	TestFalse(TEXT("A light enemy survives its second accepted strike"), Light->IsCombatDefeated());
	Light->ApplyDamage(1.0f, nullptr, Impact, FVector::ZeroVector);
	TestTrue(TEXT("A light enemy falls on its third accepted strike"), Light->IsCombatDefeated());
	TestEqual(TEXT("No light-enemy strikes remain"), Light->GetRemainingStrikes(), 0);
	Light->ApplyDamage(1.0f, nullptr, Impact, FVector::ZeroVector);
	TestEqual(TEXT("Post-death callbacks cannot add strikes"), Light->GetAcceptedStrikes(), 3);

	for (int32 Strike = 1; Strike <= 4; ++Strike)
	{
		Heavy->ApplyDamage(25.0f, nullptr, Heavy->GetActorLocation(), FVector::ZeroVector);
		TestEqual(FString::Printf(TEXT("Heavy enemy accepts strike %d"), Strike), Heavy->GetAcceptedStrikes(), Strike);
		TestEqual(FString::Printf(TEXT("Heavy enemy has %d strikes left"), 4 - Strike), Heavy->GetRemainingStrikes(), 4 - Strike);
		TestEqual(FString::Printf(TEXT("Heavy enemy defeat after strike %d"), Strike), Heavy->IsCombatDefeated(), Strike == 4);
	}

	TestEqual(TEXT("Boss Blueprint base requires six hits"), GetDefault<AQuasicomboBoss>()->RequiredHits, 6);
	if (UClass* LightClass = LoadClass<ASideScrollingCombatEnemy>(nullptr, TEXT("/Game/Quasicombo/Enemies/BP_QC_LightEnemy.BP_QC_LightEnemy_C")))
	{
		TestEqual(TEXT("Placed light enemy Blueprint requires three hits"), LightClass->GetDefaultObject<ASideScrollingCombatEnemy>()->RequiredHits, 3);
	}
	else AddError(TEXT("BP_QC_LightEnemy could not be loaded"));
	if (UClass* HeavyClass = LoadClass<ASideScrollingCombatEnemy>(nullptr, TEXT("/Game/Quasicombo/Enemies/BP_QC_HeavyEnemy.BP_QC_HeavyEnemy_C")))
	{
		TestEqual(TEXT("Placed heavy enemy Blueprint requires four hits"), HeavyClass->GetDefaultObject<ASideScrollingCombatEnemy>()->RequiredHits, 4);
	}
	else AddError(TEXT("BP_QC_HeavyEnemy could not be loaded"));
	for (int32 Strike = 1; Strike <= 6; ++Strike)
	{
		SixHitProxy->ApplyDamage(1.0f, nullptr, SixHitProxy->GetActorLocation(), FVector::ZeroVector);
		TestEqual(FString::Printf(TEXT("Shared boss threshold after strike %d"), Strike), SixHitProxy->IsCombatDefeated(), Strike == 6);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboSideEnemyAttackTest,
	"Quasicombo.Combat.SideEnemyApproachAndAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboSideEnemyAttackTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();

	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	AStaticMeshActor* Floor = FloorMesh ? World->SpawnActor<AStaticMeshActor>(FVector(3000.0f, 0.0f, 0.0f), FRotator::ZeroRotator) : nullptr;
	ASideScrollingCombatEnemy* Enemy = World->SpawnActor<ASideScrollingCombatEnemy>(ASideScrollingCombatEnemy::StaticClass(), FVector(3000.0f, 0.0f, 140.0f), FRotator::ZeroRotator);
	ASideScrollingCharacter* Player = PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(3300.0f, 0.0f, 140.0f), FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Collision floor"), Floor) || !TestNotNull(TEXT("Native side enemy"), Enemy) || !TestNotNull(TEXT("Side-scroller player"), Player))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}

	Floor->GetStaticMeshComponent()->SetStaticMesh(FloorMesh);
	Floor->SetActorScale3D(FVector(20.0f, 3.0f, 1.0f));
	Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Enemy->AutoPossessAI = EAutoPossessAI::Disabled;
	if (!Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Player controller"), Controller)) return false;
	Controller->Possess(Player);
	TestTrue(TEXT("Enemy can resolve the possessed player"), UGameplayStatics::GetPlayerPawn(Enemy, 0) == Player);
	const float HealthBefore = Player->GetCurrentHP();

	static_cast<AActor*>(Enemy)->Tick(0.016f);
	TestEqual(TEXT("Enemy approaches on a supported lane"), Enemy->CombatState, ESideCombatState::Approach);
	Player->SetActorLocation(FVector(3080.0f, 0.0f, 140.0f));
	static_cast<AActor*>(Enemy)->Tick(0.016f);
	TestEqual(TEXT("Enemy starts an attack inside melee range"), Enemy->CombatState, ESideCombatState::Attack);
	for (int32 Step = 0; Step < 4; ++Step) Fixture.TickTestWorld(0.1f);
	TestTrue(TEXT("Enemy's guarded melee trace reduces player health"), Player->GetCurrentHP() < HealthBefore);
	TestEqual(TEXT("Enemy recovers after its attack"), Enemy->CombatState, ESideCombatState::Recovery);

	Fixture.ForwardErrorMessages(this);
	return !Fixture.HasFailed();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboPlayerHealthRegenerationTest,
	"Quasicombo.Combat.PlayerHealthRegeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboPlayerHealthRegenerationTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	ASideScrollingCharacter* Player = PlayerClass
		? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator)
		: nullptr;
	if (!TestNotNull(TEXT("Side-scroller player"), Player)) return false;
	const float AuthoredMax = Player->GetMaxHP();
	if (!Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	Player->GetCharacterMovement()->DisableMovement();
	const float Max = Player->GetMaxHP();
	TestTrue(TEXT("Player max health rises by 20%"), FMath::IsNearlyEqual(Max, AuthoredMax * 1.2f, 0.001f));
	TestTrue(TEXT("Player starts at the increased maximum"), FMath::IsNearlyEqual(Player->GetCurrentHP(), Max, 0.001f));

	Player->TakeDamage(2.0f, FDamageEvent(), nullptr, nullptr);
	const float DamagedHP = Player->GetCurrentHP();
	TestTrue(TEXT("Damage reduces actual health"), FMath::IsNearlyEqual(DamagedHP, Max - 2.0f, 0.001f));
	for (int32 TickIndex = 0; TickIndex < 29; ++TickIndex) Fixture.TickTestWorld(0.1f);
	TestTrue(TEXT("No regeneration before three seconds"), FMath::IsNearlyEqual(Player->GetCurrentHP(), DamagedHP, 0.001f));
	Fixture.TickTestWorld(0.2f);
	const float FirstRecovery = Player->GetCurrentHP() - DamagedHP;
	TestTrue(TEXT("Regeneration begins after three seconds at 10% of max health per second"),
		FMath::IsNearlyEqual(FirstRecovery, Max * 0.1f * 0.1f, 0.02f));

	Player->TakeDamage(1.0f, FDamageEvent(), nullptr, nullptr);
	const float SecondDamagedHP = Player->GetCurrentHP();
	for (int32 TickIndex = 0; TickIndex < 29; ++TickIndex) Fixture.TickTestWorld(0.1f);
	TestTrue(TEXT("Later damage restarts the regeneration delay"),
		FMath::IsNearlyEqual(Player->GetCurrentHP(), SecondDamagedHP, 0.001f));
	for (int32 TickIndex = 0; TickIndex < 60; ++TickIndex) Fixture.TickTestWorld(0.1f);
	TestTrue(TEXT("Regeneration stops at the increased maximum"),
		FMath::IsNearlyEqual(Player->GetCurrentHP(), Max, 0.001f));
	Fixture.ForwardErrorMessages(this);
	return !Fixture.HasFailed();
}

#endif
