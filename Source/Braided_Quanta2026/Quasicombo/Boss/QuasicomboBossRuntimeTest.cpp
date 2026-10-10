#if WITH_DEV_AUTOMATION_TESTS
#include "QuasicomboBoss.h"
#include "QuasicomboBossRules.h"
#include "QuasicomboEvolutionComponent.h"
#include "QuasicomboEncounterLightingComponent.h"
#include "BusterCannonComponent.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboQTEDirector.h"
#include "QuasicomboQTERules.h"
#include "QuasicomboRunSubsystem.h"
#include "SideScrollingCharacter.h"
#include "QuasicomboBossAnimInstance.h"
#include "QuasicomboVictoryPickup.h"
#include "QuasicomboEnemy.h"
#include "QuasicomboBackgroundCreature.h"
#include "QuasicomboLarvaeTrap.h"
#include "QuasicomboArenaEntry.h"
#include "QuasicomboRouteLeg.h"
#include "QuasicomboBarrier.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimSequence.h"
#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"

namespace
{
void TickBossWorld(FTestWorldWrapper &Fixture, int32 Steps = 12)
{
	for (int32 Index = 0; Index < Steps; ++Index)
		Fixture.TickTestWorld(0.02f);
}
void PrepareBoss(AQuasicomboBoss *Boss, UQuasicomboRunSubsystem *Run, ASideScrollingCharacter *Player = nullptr)
{
	Boss->Quantum->bUseLiveQuantumApi = false;
	Run->TryCommitRoute(1, EQuasicomboLane::A);
	Run->TryCommitRoute(2, EQuasicomboLane::A);
	Run->TryCommitRoute(3, EQuasicomboLane::A);
	Boss->Quantum->BaseX = Boss->Quantum->BaseZ = Boss->Quantum->ComboXPerHit = Boss->Quantum->DamageZPerHP = 0.0;
	Boss->Evolution->DurationScale = 0.001f;
	Boss->EvolutionMinimumSeconds = 0.0f;
	Boss->EvolutionTimeoutSeconds = 0.0f;
	Boss->QTE->SuccessBeatSeconds = 0.0f;
	Boss->StartEncounter(Player);
}
void HitBoss(AQuasicomboBoss *Boss)
{
	Boss->ApplyDamage(20.0f, nullptr, Boss->GetActorLocation(), FVector::ZeroVector);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossRulesTest, "Quasicombo.Boss.ArmorAndUncertainty",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossRulesTest::RunTest(const FString &)
{
	TestEqual(TEXT("Below first armor threshold"), QuasicomboBossRules::ArmorBars(0.0499), 0);
	TestEqual(TEXT("At first armor threshold"), QuasicomboBossRules::ArmorBars(0.05), 1);
	TestEqual(TEXT("Below second armor threshold"), QuasicomboBossRules::ArmorBars(-0.1499), 1);
	TestEqual(TEXT("At second armor threshold"), QuasicomboBossRules::ArmorBars(-0.15), 2);
	TestEqual(TEXT("Vacuum is one round"), QuasicomboBossRules::QTERounds(0), 1);
	TestEqual(TEXT("Tau is one round"), QuasicomboBossRules::QTERounds(1), 1);
	TestEqual(TEXT("Quarter is two rounds"), QuasicomboBossRules::QTERounds(0.25), 2);
	TestEqual(TEXT("Balanced is three rounds"), QuasicomboBossRules::QTERounds(0.5), 3);
    TestEqual(TEXT("Three rounds build toward five"),QuasicomboQTERules::RoundPromptCount(5,1,3),3);
    TestEqual(TEXT("Middle round builds toward five"),QuasicomboQTERules::RoundPromptCount(5,2,3),4);
    for(int32 Rounds=1;Rounds<=3;++Rounds) for(bool Tau:{false,true}) for(int32 Round=1;Round<=Rounds;++Round)
    {
        FQuasicomboQTESequence Sequence;
        Sequence.Begin(Tau,100.,4.,Rounds,QuasicomboQTERules::RoundPromptCount(5,Round,Rounds),Round);
        TestEqual(TEXT("Explicit combat round is preserved"),Sequence.GetRoundNumber(),Round);
        const int32 Count=Sequence.GetPromptCount();
        for(int32 Prompt=0;Prompt<Count;++Prompt)
            TestEqual(TEXT("Only the selected prefix's last prompt succeeds"),
                Sequence.Submit(Sequence.GetExpectedInput(),100.1+Prompt*.1,false),
                Prompt==Count-1?EQuasicomboQTEProgress::Succeeded:EQuasicomboQTEProgress::Advanced);
    }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossCleanupTest, "Quasicombo.Boss.EncounterClearsCombatEnemies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossCleanupTest::RunTest(const FString&)
{
	for (int32 EntryPath = 0; EntryPath < 3; ++EntryPath)
	{
		FTestWorldWrapper Fixture;
		if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
		UWorld* World = Fixture.GetTestWorld();
		UClass* EnemyClass = LoadClass<AQuasicomboEnemy>(nullptr, TEXT("/Game/Quasicombo/Enemies/BP_QC_Enemy.BP_QC_Enemy_C"));
		UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
			TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
		if (!TestNotNull(TEXT("Placed combat enemy class"), EnemyClass) || !TestNotNull(TEXT("Player class"), PlayerClass)) return false;
		auto* Boss = World->SpawnActor<AQuasicomboBoss>();
		auto* OtherBoss = World->SpawnActor<AQuasicomboBoss>(AQuasicomboBoss::StaticClass(), FVector(50000, 0, 200), FRotator::ZeroRotator);
		Boss->Quantum->bUseLiveQuantumApi = OtherBoss->Quantum->bUseLiveQuantumApi = false;
		auto* Player = World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(-3000, 0, 200), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("Player spawns"), Player)) return false;
		auto* Wood = World->SpawnActor<AQuasicomboEnemy>(EnemyClass, FVector(6000, 0, 200), FRotator::ZeroRotator);
		auto* Golem = World->SpawnActor<AQuasicomboEnemy>(EnemyClass, FVector(7000, 0, 200), FRotator::ZeroRotator);
		Golem->Species = EQuasicomboEnemySpecies::StoneGolem;
		Golem->Tier = EQuasicomboEnemyTier::Heavy;
		auto* Hidden = World->SpawnActor<AQuasicomboEnemy>(EnemyClass, FVector(8000, 0, 200), FRotator::ZeroRotator);
		auto* AlreadyDead = World->SpawnActor<AQuasicomboEnemy>(EnemyClass, FVector(9000, 0, 200), FRotator::ZeroRotator);
		auto* Scenery = World->SpawnActor<AQuasicomboBackgroundCreature>();
		Scenery->bIdleOnly = true;
		auto* Trap = World->SpawnActor<AQuasicomboLarvaeTrap>(AQuasicomboLarvaeTrap::StaticClass(), FVector(40000, 0, 200), FRotator::ZeroRotator);
		auto* Leg = World->SpawnActor<AQuasicomboRouteLeg>();
		Leg->ForwardBarrier = World->SpawnActor<AQuasicomboBarrier>();
		Leg->Enemies = {Wood, Golem};
		auto* FinalLeg = World->SpawnActor<AQuasicomboRouteLeg>();
		auto* Arena = World->SpawnActor<AQuasicomboArenaEntry>();
		Arena->FinalLeg = FinalLeg;
		Arena->Lane = EQuasicomboLane::A;
		Arena->ArrivalLocation = FVector(-800, 0, 200);
		if (!Fixture.BeginPlayInTestWorld()) return false;
		auto* Controller = World->SpawnActor<APlayerController>();
		Controller->Possess(Player);
		auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
		const FString Prefix = FString::Printf(TEXT("Entry path %d: "), EntryPath);
		const float WoodHP = Wood->CurrentHP;
		Run->HoldForFrontend();
		Boss->StartEncounter(Player);
		TestEqual(Prefix + TEXT("rejected start preserves enemies"), Wood->CurrentHP, WoodHP);
		TestEqual(Prefix + TEXT("rejected start leaves boss waiting"), Boss->GetBossPhase(), EQuasicomboBossPhase::Waiting);
		Run->StartRun();
		for (int32 Section = 1; Section <= 3; ++Section) Run->TryCommitRoute(Section, EQuasicomboLane::A);
		Leg->Select();
		Hidden->SetActorHiddenInGame(true);
		Hidden->SetActorEnableCollision(false);
		Hidden->SetActorTickEnabled(false);
		AlreadyDead->TakeDamage(AlreadyDead->CurrentHP, FDamageEvent(), nullptr, nullptr);
		TestTrue(Prefix + TEXT("route exit starts closed"), Leg->ForwardBarrier->IsClosed());
		if (EntryPath == 0) Boss->StartEncounter(Player);
		else if (EntryPath == 1)
		{
			auto* Box = Arena->FindComponentByClass<UBoxComponent>();
			Box->OnComponentBeginOverlap.Broadcast(Box, Player, Player->GetCapsuleComponent(), 0, false, FHitResult());
		}
		else
		{
			Player->SetActorLocation(FVector(-1200, 0, 250), false);
			TickBossWorld(Fixture);
		}
		TestEqual(Prefix + TEXT("boss starts combat"), Boss->GetBossPhase(), EQuasicomboBossPhase::Combat);
		for (AQuasicomboEnemy* Enemy : {Wood, Golem, Hidden, AlreadyDead})
		{
			TestEqual(Prefix + Enemy->GetName() + TEXT(" has no HP"), Enemy->CurrentHP, 0.0f);
			TestEqual(Prefix + Enemy->GetName() + TEXT(" cannot attack through its capsule"),
				Enemy->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
		TestFalse(Prefix + TEXT("death notifications open the selected exit"), Leg->ForwardBarrier->IsClosed());
		TestTrue(Prefix + TEXT("hidden enemy stays hidden"), Hidden->IsHidden());
		TestTrue(Prefix + TEXT("wood uses its ordinary ragdoll death"), Wood->GetMesh()->IsSimulatingPhysics());
		TestTrue(Prefix + TEXT("boss retains full health"), FMath::IsNearlyEqual(Boss->GetBaseHealthFraction(), 1.0f));
		TestTrue(Prefix + TEXT("other boss survives"), OtherBoss->CurrentHP > 0.0f);
		TestTrue(Prefix + TEXT("player survives"), Player->GetCurrentHP() > 0.0f);
		TestTrue(Prefix + TEXT("background creature and larvae trap survive"), IsValid(Scenery) && IsValid(Trap));
		TestEqual(Prefix + TEXT("cleanup grants no combo points"), Player->GetHitComboCount(), 0);
		auto* LateEnemy = World->SpawnActor<AQuasicomboEnemy>(EnemyClass, FVector(10000, 0, 200), FRotator::ZeroRotator);
		const float LateHP = LateEnemy->CurrentHP;
		Boss->StartEncounter(Player);
		TestEqual(Prefix + TEXT("repeated start does not run cleanup again"), LateEnemy->CurrentHP, LateHP);
		for (int32 Step = 0; Step < 60; ++Step) Fixture.TickTestWorld(0.1f);
		TestTrue(Prefix + TEXT("normal death timer removes enemy"), Wood->IsActorBeingDestroyed());
		TestTrue(Prefix + TEXT("normal death timer removes hidden enemy"), Hidden->IsActorBeingDestroyed());
		Fixture.ForwardErrorMessages(this);
		if (Fixture.HasFailed()) return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossApproachTest, "Quasicombo.Boss.ApproachStartsCombat",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossApproachTest::RunTest(const FString &)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
		return false;
	UWorld *World = Fixture.GetTestWorld();
	UClass *BossClass = LoadClass<AQuasicomboBoss>(nullptr, TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C"));
	UClass *PlayerClass = LoadClass<ASideScrollingCharacter>(
		nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	AQuasicomboBoss *Boss = BossClass ? World->SpawnActor<AQuasicomboBoss>(BossClass, FVector(0, 0, 200), FRotator::ZeroRotator) : nullptr;
	ASideScrollingCharacter *Player =
		PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(-3000, 0, 200), FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Placed lizard class loads"), Boss) || !TestNotNull(TEXT("Player class loads"), Player))
		return false;
	Boss->Quantum->bUseLiveQuantumApi = false;
	if (!Fixture.BeginPlayInTestWorld())
		return false;
	APlayerController *Controller = World->SpawnActor<APlayerController>();
	Controller->Possess(Player);
	TickBossWorld(Fixture);
	TestEqual(TEXT("Boss waits before the player reaches the arena"), Boss->GetBossPhase(), EQuasicomboBossPhase::Waiting);
	Player->SetActorLocation(Boss->GetActorLocation() + FVector(-1200, 0, 900), false);
	TickBossWorld(Fixture);
	TestEqual(TEXT("Boss waits while the player is above the arena"), Boss->GetBossPhase(), EQuasicomboBossPhase::Waiting);
	Player->SetActorLocation(Boss->GetActorLocation() + FVector(-1200, 0, 250), false);
	TickBossWorld(Fixture);
	TestEqual(TEXT("Dropping into the boss's extended awareness starts combat"), Boss->GetBossPhase(), EQuasicomboBossPhase::Combat);
	TestTrue(TEXT("Boss can pursue beyond the old 450 cm leash"), Boss->PatrolHalfWidth >= 1200.0f);
	UWidgetComponent *Bar = Boss->FindComponentByClass<UWidgetComponent>();
	TestTrue(TEXT("Boss health is HUD-only"), Bar && Bar->bHiddenInGame);
	TestTrue(TEXT("Lizard boss melee damage is 2 HP"), FMath::IsNearlyEqual(Boss->GetMeleeDamage(), 2.0f));
	TestTrue(TEXT("Lizard boss tail sweep damage is 4 HP"), FMath::IsNearlyEqual(Boss->GetTailDamage(), 4.0f));
	Boss->ApplyDamage(1.0f, Player, Boss->GetActorLocation(), FVector::ZeroVector);
	TestTrue(TEXT("Active boss accepts a player strike"), Boss->GetBaseHealthFraction() < 1.0f);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossFinisherTest, "Quasicombo.Boss.BaseHealthToVictory",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossFinisherTest::RunTest(const FString &)
{
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game) || !TestWorld.BeginPlayInTestWorld())
		return false;
	UWorld *World = TestWorld.GetTestWorld();
	ULightComponent* Sun = World->SpawnActor<ADirectionalLight>()->GetLightComponent();
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetIntensity(20.0f);
	AQuasicomboBoss *Boss = World->SpawnActor<AQuasicomboBoss>();
	Boss->EncounterLighting->CombatFadeSeconds = Boss->EncounterLighting->EvolutionFadeSeconds = 0.0f;
	UClass *PlayerClass = LoadClass<ASideScrollingCharacter>(
		nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	ASideScrollingCharacter *Player =
		PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(-1000.0f, 0.0f, 200.0f), FRotator::ZeroRotator)
					: nullptr;
	if (!TestNotNull(TEXT("Player for reward walk"), Player))
		return false;
	auto *Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	PrepareBoss(Boss, Run, Player);
	const float Dilation = UGameplayStatics::GetGlobalTimeDilation(World);
	for (int32 Strike = 0; Strike < Boss->BaseHealthHits - Boss->EvolutionTriggerHealth; ++Strike)
		HitBoss(Boss);
	TestEqual(TEXT("One HP enters evolution"), Boss->GetBossPhase(), EQuasicomboBossPhase::Evolution);
	HitBoss(Boss);
	TestEqual(TEXT("Evolution rejects hits"), Boss->GetAcceptedStrikes(), Boss->BaseHealthHits - 1);
	TickBossWorld(TestWorld);
	TestEqual(TEXT("Offline evolution adds no armor"), Boss->GetArmorBars(), 0);
	TestEqual(TEXT("Evolution heals base health fully"), Boss->GetBaseHealthFraction(), 1.0f);
	const EMovementMode BeforeFinisherMovement = Player->GetCharacterMovement()->MovementMode;
	for (int32 Strike = 0; Strike < Boss->BaseHealthHits; ++Strike) HitBoss(Boss);
	TestTrue(TEXT("Depleting base health enters QTE"), Boss->QTE->IsQTEActive());
	HitBoss(Boss);
	TestEqual(TEXT("QTE rejects damage"), Boss->GetAcceptedStrikes(), 2 * Boss->BaseHealthHits - 1);
	AQuasicomboQTEDirector* Director=Run->GetQTEDirector();
	if(!TestNotNull(TEXT("Independent presentation owner"),Director)) return false;
	Director->AdvanceToPromptForTesting();
	const int32 Count=Director->QTE->GetPromptCount();
	for(int32 I=0;I<Count;++I)
	{
		Director->Submit(Director->QTE->GetExpectedInput());
		Director->AdvanceForTesting(2.2f);
	}
	Director->AdvanceForTesting(2.f);

	TestEqual(TEXT("QTE defeats boss without ending run"), Boss->GetBossPhase(), EQuasicomboBossPhase::AwaitPickup);
	TestEqual(TEXT("Run waits for the reward"), Run->GetOutcome(), EQuasicomboRunOutcome::Playing);
	TestFalse(TEXT("Player movement is unlocked to collect reward"), Run->IsBossCombatLocked());
	TestEqual(TEXT("Player movement mode is restored after QTE"), Player->GetCharacterMovement()->MovementMode, BeforeFinisherMovement==MOVE_None?MOVE_Walking:BeforeFinisherMovement);
	TestEqual(TEXT("Boss capsule stops blocking movement"), Boss->GetCapsuleComponent()->GetCollisionEnabled(),
			  ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Boss corpse remains visible"), Boss->IsHidden());
	TestFalse(TEXT("Cannot claim reward before it appears"), Boss->ClaimVictory());
	TickBossWorld(TestWorld, 100);
	AQuasicomboVictoryPickup *Pickup =
		Cast<AQuasicomboVictoryPickup>(UGameplayStatics::GetActorOfClass(World, AQuasicomboVictoryPickup::StaticClass()));
	if (!TestNotNull(TEXT("Death spawns victory pickup"), Pickup))
		return false;
	UStaticMeshComponent *PickupMesh = Pickup->FindComponentByClass<UStaticMeshComponent>();
	if (!TestNotNull(TEXT("Victory pickup has a mesh component"), PickupMesh))
		return false;
	UStaticMesh *Mesh = PickupMesh->GetStaticMesh().Get();
	if (!TestNotNull(TEXT("Victory pickup has a mesh"), Mesh))
		return false;
	TestEqual(TEXT("Victory pickup uses the Radical Buster mesh"), Mesh->GetPathName(),
			  FString(TEXT("/Game/RadicalMike/Mesh/SM_RadicalBuster.SM_RadicalBuster")));
    for(int32 Slot=0;Slot<Player->GetBusterCannon()->CannonMesh->GetNumMaterials();++Slot)
        TestEqual(TEXT("Pickup matches the current robot acquisition palette"),PickupMesh->GetMaterial(Slot),Player->GetBusterCannon()->CannonMesh->GetMaterial(Slot));
	TestFalse(TEXT("Pickup does not hide the corpse"), Boss->IsHidden());
	TestTrue(TEXT("Corpse starts physics after the death clip"), Boss->GetMesh()->IsSimulatingPhysics());
	TestEqual(TEXT("Corpse ignores the player"), Boss->GetMesh()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TickBossWorld(TestWorld, 35); // Let the spawned pickup's overlap guard arm.
	Player->SetActorLocation(Pickup->GetActorLocation(), false);
	TickBossWorld(TestWorld, 1);
	TestEqual(TEXT("Pickup claim ends run"), Run->GetOutcome(), EQuasicomboRunOutcome::Victory);
	TestFalse(TEXT("Corpse remains visible on victory"), Boss->IsHidden());
	TestTrue(TEXT("Dilation restored"), FMath::IsNearlyEqual(Dilation, UGameplayStatics::GetGlobalTimeDilation(World)));
	TestEqual(TEXT("Pickup collection preserves one-lux sunlight"), Sun->Intensity, 1.0f);
	auto* Cannon = Player->GetBusterCannon();
	if (!TestNotNull(TEXT("Mike's pickup presentation"), Cannon)) return false;
	TestEqual(TEXT("Pickup starts Mike's lift"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::Lift);
	bool bStayedDark = true;
	for (int32 I = 0; I < 2000 && Cannon->AcquisitionPhase != EBusterAcquisitionPhase::Complete; ++I)
	{
		TestWorld.TickTestWorld(0.02f);
		bStayedDark &= FMath::IsNearlyEqual(Sun->Intensity, 1.0f);
	}
	TestEqual(TEXT("Full pickup animation completes"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::Complete);
	TestTrue(TEXT("Sunlight stays dim throughout pickup animation"), bStayedDark);
	TestTrue(TEXT("Victory screen is released"), Run->IsVictoryPresentationReady());
	TestEqual(TEXT("Victory screen retains dark lighting"), Sun->Intensity, 1.0f);
	Boss->Destroy();
	TestEqual(TEXT("Encounter removal still restores authored sunlight"), Sun->Intensity, 20.0f);
	TestWorld.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossRewardEligibilityTest, "Quasicombo.Boss.RewardEligibilityAfterDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossRewardEligibilityTest::RunTest(const FString&)
{
    for(bool bEquipped : {false,true})
    {
        FTestWorldWrapper Fixture;
        if(!Fixture.CreateTestWorld(EWorldType::Game)||!Fixture.BeginPlayInTestWorld()) return false;
        auto* World=Fixture.GetTestWorld();
        auto* Run=World->GetSubsystem<UQuasicomboRunSubsystem>();
        auto* Boss=World->SpawnActor<AQuasicomboBoss>();
        auto* Player=World->SpawnActor<ASideScrollingCharacter>(LoadClass<ASideScrollingCharacter>(nullptr,
            TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C")),FVector(-1000,0,200),FRotator::ZeroRotator);
        if(!TestNotNull(TEXT("Reward player"),Player)) return false;
        if(bEquipped) Player->GetBusterCannon()->Equip();
        PrepareBoss(Boss,Run,Player);
        Boss->HandleQTEResult(true);
        const float DeathSeconds=Boss->DefeatAnimation?Boss->DefeatAnimation->GetPlayLength():1.5f;
        TickBossWorld(Fixture,FMath::Max(1,FMath::FloorToInt(DeathSeconds/.02f)));
        TestEqual(TEXT("Death clip cannot end the level early"),Run->GetOutcome(),EQuasicomboRunOutcome::Playing);
        TestNull(TEXT("Death clip cannot spawn an early pickup"),UGameplayStatics::GetActorOfClass(World,AQuasicomboVictoryPickup::StaticClass()));
        TickBossWorld(Fixture,10);
        TestEqual(TEXT("Legacy death spawns only missing equipment"),UGameplayStatics::GetActorOfClass(World,AQuasicomboVictoryPickup::StaticClass())!=nullptr,!bEquipped);
        TestEqual(TEXT("Legacy equipped victory follows the full death animation"),Run->GetOutcome(),bEquipped?EQuasicomboRunOutcome::Victory:EQuasicomboRunOutcome::Playing);
        TickBossWorld(Fixture,300);
        TestTrue(TEXT("Legacy corpse remains a physical ragdoll"),Boss->GetMesh()->IsSimulatingPhysics());
        TestTrue(TEXT("Legacy corpse retains gravity after six seconds"),Boss->GetMesh()->IsGravityEnabled());
        Fixture.ForwardErrorMessages(this);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossArmorRetryTest, "Quasicombo.Boss.ArmorAndSharedRetry",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossArmorRetryTest::RunTest(const FString &)
{
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game) || !TestWorld.BeginPlayInTestWorld())
		return false;
	UWorld *World = TestWorld.GetTestWorld();
	AQuasicomboBoss *Boss = World->SpawnActor<AQuasicomboBoss>();
	UClass *PlayerClass = LoadClass<ASideScrollingCharacter>(
		nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	ASideScrollingCharacter *Player =
		PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(-1000.0f, 0.0f, 200.0f), FRotator::ZeroRotator)
					: nullptr;
	if (!TestNotNull(TEXT("Playable character loaded"), Player))
		return false;
	auto *Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	// Force both armor awards without depending on a live API or editor-only overrides.
	Boss->FirstArmorThreshold = Boss->SecondArmorThreshold = 0.0;
	PrepareBoss(Boss, Run, Player);
	for (int32 Strike = 0; Strike < Boss->BaseHealthHits - Boss->EvolutionTriggerHealth; ++Strike)
		HitBoss(Boss);
	TickBossWorld(TestWorld);
	TestEqual(TEXT("Two armor bars awarded"), Boss->GetArmorBars(), 2);
	TestEqual(TEXT("Evolution heals base while awarding armor"), Boss->GetBaseHealthFraction(), 1.0f);
	for (int32 Strike = 0; Strike < 4; ++Strike)
		HitBoss(Boss);
	TestEqual(TEXT("Breaking first armor removes tier"), Boss->GetArmorBars(), 1);
	TestEqual(TEXT("Armor absorbs all four hits"), Boss->GetBaseHealthFraction(), 1.0f);
	Player->ApplyDamage(100.0f, Boss, Player->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Combat death retries"), Boss->GetBossPhase(), EQuasicomboBossPhase::Retry);
	TestTrue(TEXT("Repeated failure is absorbed"), Run->TryHandleBossFailure());
	TickBossWorld(TestWorld);
	TestEqual(TEXT("Retry waits for a real-time second"), Boss->GetBossPhase(), EQuasicomboBossPhase::Retry);
	FPlatformProcess::Sleep(1.02f);
	TickBossWorld(TestWorld);
	TestEqual(TEXT("Retry keeps run alive"), Run->GetOutcome(), EQuasicomboRunOutcome::Playing);
	TestEqual(TEXT("Retry restores full armor award"), Boss->GetArmorBars(), 2);
	TestEqual(TEXT("Retry heals base health"), Boss->GetBaseHealthFraction(), 1.0f);
	TestEqual(TEXT("Retry heals player"), Player->GetCurrentHP(), Player->GetMaxHP());
	TestEqual(TEXT("Only one retry consumed"), Boss->GetRetriesRemaining(), 0);
	TestTrue(TEXT("Evolution retained"), Boss->Quantum->HasEvolutionResolved());
	for (int32 Strike = 0; Strike < Boss->BaseHealthHits + 2 * Boss->HitsPerArmorBar; ++Strike)
		HitBoss(Boss);
	TestTrue(TEXT("Retry can reach QTE"), Boss->QTE->IsQTEActive());
    if(auto* Director=Run->GetQTEDirector()) Director->AdvanceToPromptForTesting();
    TestTrue(TEXT("An external encounter interruption is handled during QTE"),Run->TryHandleBossFailure());
	TestEqual(TEXT("Second encounter failure during QTE loses run"), Run->GetOutcome(), EQuasicomboRunOutcome::Defeat);
	TestFalse(TEXT("Failure releases QTE input"), Boss->QTE->IsQTEActive());
	TestWorld.ForwardErrorMessages(this);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossRigTest, "Quasicombo.Boss.LizardRigAndTailAnimation",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossRigTest::RunTest(const FString &)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
		return false;
	UWorld *World = Fixture.GetTestWorld();
	UClass *BossClass = LoadClass<AQuasicomboBoss>(nullptr, TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C"));
	AQuasicomboBoss *Boss = BossClass ? World->SpawnActor<AQuasicomboBoss>(BossClass) : nullptr;
	if (!TestNotNull(TEXT("Boss Blueprint loads"), Boss))
		return false;
	Boss->Quantum->bUseLiveQuantumApi = false;
	if (!Fixture.BeginPlayInTestWorld())
		return false;
	Boss->GetCharacterMovement()->DisableMovement();
	USkeletalMeshComponent *Mesh = Boss->GetMesh();
	UAnimInstance *Anim = Mesh->GetAnimInstance();
	if (!TestTrue(TEXT("Native lizard graph is active"), Anim && Anim->IsA<UQuasicomboBossAnimInstance>()))
		return false;
	TickBossWorld(Fixture, 10);
	TestTrue(FString::Printf(TEXT("Lizard stands upright (head %.1f, foot %.1f, rotation %s)"), Mesh->GetSocketLocation(TEXT("head")).Z,
							 Mesh->GetSocketLocation(TEXT("foot_l")).Z, *Mesh->GetRelativeRotation().ToString()),
			 Mesh->GetSocketLocation(TEXT("head")).Z > Mesh->GetSocketLocation(TEXT("foot_l")).Z + 100.0f);
	const FBoxSphereBounds BodyBounds = Mesh->GetSkeletalMeshAsset()->GetBounds();
	const float BodyBottom = Mesh->GetRelativeLocation().Z + BodyBounds.Origin.Z - BodyBounds.BoxExtent.Z;
	const float CapsuleBottom = -Boss->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	TestTrue(
		FString::Printf(TEXT("Imported claw geometry is above capsule bottom in Blueprint pose (mesh %.1f, capsule %.1f, offset %.1f)"),
						BodyBottom, CapsuleBottom, Mesh->GetRelativeLocation().Z),
		BodyBottom >= CapsuleBottom && BodyBottom <= CapsuleBottom + 10.0f);
	for (int32 Index = 1; Index <= 7; ++Index)
		TestTrue(TEXT("Tail joint preserved"), Mesh->GetBoneIndex(FName(*FString::Printf(TEXT("u_Tail_%02d"), Index))) != INDEX_NONE);
	const FVector TailBase = Mesh->GetSocketLocation(TEXT("u_Tail_01"));
	const FVector TailTip = Mesh->GetSocketLocation(TEXT("u_Tail_07"));
	TestTrue(TEXT("Idle tail has length"), FVector::Distance(TailBase, TailTip) > 40.0f);
	if (!TestNotNull(TEXT("Tail sweep asset assigned"), Boss->TailAttackAnimation.Get()))
		return false;
	TestNotNull(TEXT("Hit stumble asset assigned"), Boss->HitReactionAnimation.Get());
	TestNotNull(TEXT("QTE daze asset assigned"), Boss->DazedAnimation.Get());
	TestNotNull(TEXT("Victory fall asset assigned"), Boss->DefeatAnimation.Get());
	UAnimMontage *Montage = Anim->PlaySlotAnimationAsDynamicMontage(Boss->TailAttackAnimation, TEXT("DefaultSlot"), 0.0f, 0.0f);
	TestNotNull(TEXT("Tail plays through graph slot"), Montage);
	TickBossWorld(Fixture, 45);
	TestTrue(TEXT("Tail sweep moves the tip"), FVector::Distance(TailTip, Mesh->GetSocketLocation(TEXT("u_Tail_07"))) > 40.0f);
	TestTrue(TEXT("Tail keeps its length during sweep"),
			 FVector::Distance(Mesh->GetSocketLocation(TEXT("u_Tail_01")), Mesh->GetSocketLocation(TEXT("u_Tail_07"))) > 40.0f);
	TArray<USkeletalMeshComponent *> Parts;
	Boss->GetComponents(Parts);
	TestEqual(TEXT("Body and thirteen armor parts"), Parts.Num(), 14);
	for (USkeletalMeshComponent *Part : Parts)
		if (Part != Mesh)
			TestTrue(TEXT("Armor follows body pose"), Part->LeaderPoseComponent.Get() == Mesh);
    UClass* PlayerClass=LoadClass<ASideScrollingCharacter>(nullptr,TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
    auto* Player=World->SpawnActor<ASideScrollingCharacter>(PlayerClass,FVector(-500,0,200),FRotator::ZeroRotator);
	PrepareBoss(Boss, World->GetSubsystem<UQuasicomboRunSubsystem>(),Player);
	TickBossWorld(Fixture, 30);
	const float StandingHead = Mesh->GetSocketLocation(TEXT("head")).Z;
	for (int32 Strike = 0; Strike < Boss->BaseHealthHits - Boss->EvolutionTriggerHealth; ++Strike)
		HitBoss(Boss);
	TickBossWorld(Fixture);
	for (int32 Strike = 0; Strike < Boss->BaseHealthHits; ++Strike) HitBoss(Boss);
	TestEqual(TEXT("Boss enters QTE daze"), Boss->GetBossPhase(), EQuasicomboBossPhase::QTE);
	TickBossWorld(Fixture, 15);
	AddInfo(FString::Printf(TEXT("Boss head: standing %.1f dazed %.1f"), StandingHead, Mesh->GetSocketLocation(TEXT("head")).Z));
	TestTrue(TEXT("Daze visibly lowers the head"), Mesh->GetSocketLocation(TEXT("head")).Z < StandingHead - 4.0f);
	AQuasicomboQTEDirector* Director=World->GetSubsystem<UQuasicomboRunSubsystem>()->GetQTEDirector();
	if(!TestNotNull(TEXT("Independent presentation owner"),Director)) return false;
	Director->AdvanceToPromptForTesting();
	const int32 Count=Director->QTE->GetPromptCount();
	for(int32 I=0;I<Count;++I)
	{
		Director->Submit(Director->QTE->GetExpectedInput());
		Director->AdvanceForTesting(2.2f);
	}
	Director->AdvanceForTesting(2.f);

	TestEqual(TEXT("QTE enters death and reward beat"), Boss->GetBossPhase(), EQuasicomboBossPhase::AwaitPickup);
	TickBossWorld(Fixture, 90);
	AddInfo(FString::Printf(TEXT("Boss head: standing %.1f victory %.1f"), StandingHead, Mesh->GetSocketLocation(TEXT("head")).Z));
	TestTrue(TEXT("Death animation reaches a low pose"), Mesh->GetSocketLocation(TEXT("head")).Z < StandingHead - 75.0f);
	TestFalse(TEXT("Body remains after the fall"), Boss->IsHidden());
	TestEqual(TEXT("Fallen capsule no longer blocks the player"), Boss->GetCapsuleComponent()->GetCollisionEnabled(),
			  ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Fallen mesh ignores the player"), Boss->GetMesh()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestNotNull(TEXT("Pickup appears beside the fallen body"),
				UGameplayStatics::GetActorOfClass(World, AQuasicomboVictoryPickup::StaticClass()));
	Fixture.ForwardErrorMessages(this);
	return true;
}
#endif
