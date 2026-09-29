#if WITH_DEV_AUTOMATION_TESTS
#include "QuasicomboBoss.h"
#include "QuasicomboBossRules.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "SideScrollingCharacter.h"
#include "QuasicomboBossAnimInstance.h"
#include "QuasicomboVictoryPickup.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimSequence.h"
#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"

namespace
{
void TickBossWorld(FTestWorldWrapper& Fixture, int32 Steps = 5)
{
	for (int32 Index = 0; Index < Steps; ++Index) Fixture.TickTestWorld(0.02f);
}
void PrepareBoss(AQuasicomboBoss* Boss, UQuasicomboRunSubsystem* Run, ASideScrollingCharacter* Player = nullptr)
{
	Boss->Quantum->bUseLiveQuantumApi = false;
	Run->TryCommitRoute(1, EQuasicomboLane::A);
	Run->TryCommitRoute(2, EQuasicomboLane::A);
	Run->TryCommitRoute(3, EQuasicomboLane::A);
	Boss->EvolutionMinimumSeconds = 0.0f;
	Boss->EvolutionTimeoutSeconds = 0.0f;
	Boss->RetryDelaySeconds = 0.01f;
	Boss->QTE->SuccessBeatSeconds = 0.0f;
	Boss->StartEncounter(Player);
}
void HitBoss(AQuasicomboBoss* Boss)
{
	Boss->ApplyDamage(20.0f, nullptr, Boss->GetActorLocation(), FVector::ZeroVector);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossRulesTest, "Quasicombo.Boss.ArmorAndUncertainty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossRulesTest::RunTest(const FString&)
{
	TestEqual(TEXT("Below first armor threshold"), QuasicomboBossRules::ArmorBars(0.0499), 0);
	TestEqual(TEXT("At first armor threshold"), QuasicomboBossRules::ArmorBars(0.05), 1);
	TestEqual(TEXT("Below second armor threshold"), QuasicomboBossRules::ArmorBars(-0.1499), 1);
	TestEqual(TEXT("At second armor threshold"), QuasicomboBossRules::ArmorBars(-0.15), 2);
	TestEqual(TEXT("Vacuum is one round"), QuasicomboBossRules::QTERounds(0), 1);
	TestEqual(TEXT("Tau is one round"), QuasicomboBossRules::QTERounds(1), 1);
	TestEqual(TEXT("Quarter is two rounds"), QuasicomboBossRules::QTERounds(0.25), 2);
	TestEqual(TEXT("Balanced is three rounds"), QuasicomboBossRules::QTERounds(0.5), 3);
	for (int32 Rounds = 1; Rounds <= 3; ++Rounds)
	{
		for (bool bTau : {false, true})
		{
			FQuasicomboQTESequence Sequence;
			Sequence.Begin(bTau, 100.0, 4.0, Rounds);
			for (int32 Prompt = 0; Prompt < Rounds * 3; ++Prompt)
			{
				TestEqual(TEXT("Correct round"), Sequence.GetRoundNumber(), Prompt / 3 + 1);
				const auto Result = Sequence.Submit(Sequence.GetExpectedInput(), 100.1 + Prompt*0.1, false);
				TestEqual(TEXT("Only final prompt succeeds"), Result, Prompt == Rounds*3-1 ? EQuasicomboQTEProgress::Succeeded : EQuasicomboQTEProgress::Advanced);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossApproachTest, "Quasicombo.Boss.ApproachStartsCombat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossApproachTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
	UWorld* World = Fixture.GetTestWorld();
	UClass* BossClass = LoadClass<AQuasicomboBoss>(nullptr, TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C"));
	UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	AQuasicomboBoss* Boss = BossClass ? World->SpawnActor<AQuasicomboBoss>(BossClass, FVector(0, 0, 200), FRotator::ZeroRotator) : nullptr;
	ASideScrollingCharacter* Player = PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(PlayerClass,
		FVector(-900, 0, 200), FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Placed lizard class loads"), Boss) || !TestNotNull(TEXT("Player class loads"), Player)) return false;
	Boss->Quantum->bUseLiveQuantumApi = false;
	if (!Fixture.BeginPlayInTestWorld()) return false;
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->Possess(Player);
	TickBossWorld(Fixture);
	TestEqual(TEXT("Boss waits before the player reaches the arena"), Boss->GetBossPhase(), EQuasicomboBossPhase::Waiting);
	Player->SetActorLocation(Boss->GetActorLocation() + FVector(-300, 0, 0), false);
	TickBossWorld(Fixture);
	TestEqual(TEXT("Reaching the placed boss starts combat"), Boss->GetBossPhase(), EQuasicomboBossPhase::Combat);
	UWidgetComponent* Bar = Boss->FindComponentByClass<UWidgetComponent>();
	TestTrue(TEXT("Boss over-head bar is visible"), Bar && !Bar->bHiddenInGame);
	TestTrue(TEXT("Lizard boss melee damage is 2 HP"), FMath::IsNearlyEqual(Boss->GetMeleeDamage(), 2.0f));
	TestTrue(TEXT("Lizard boss tail sweep damage is 4 HP"), FMath::IsNearlyEqual(Boss->GetTailDamage(), 4.0f));
	Boss->ApplyDamage(1.0f, Player, Boss->GetActorLocation(), FVector::ZeroVector);
	TestTrue(TEXT("Active boss accepts a player strike"), Boss->GetBaseHealthFraction() < 1.0f);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossFinisherTest, "Quasicombo.Boss.BaseHealthToVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossFinisherTest::RunTest(const FString&)
{
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game) || !TestWorld.BeginPlayInTestWorld()) return false;
	UWorld* World = TestWorld.GetTestWorld();
	AQuasicomboBoss* Boss = World->SpawnActor<AQuasicomboBoss>();
	UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	ASideScrollingCharacter* Player = PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(
		PlayerClass, FVector(-1000.0f, 0.0f, 200.0f), FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Player for reward walk"), Player)) return false;
	auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	PrepareBoss(Boss, Run, Player);
	const float Dilation = UGameplayStatics::GetGlobalTimeDilation(World);
	for (int32 Strike=1; Strike<=3; ++Strike) HitBoss(Boss);
	TestEqual(TEXT("Third hit enters evolution"), Boss->GetBossPhase(), EQuasicomboBossPhase::Evolution);
	HitBoss(Boss);
	TestEqual(TEXT("Evolution rejects hits"), Boss->GetAcceptedStrikes(), 3);
	TickBossWorld(TestWorld);
	TestEqual(TEXT("Offline evolution adds no armor"), Boss->GetArmorBars(), 0);
	TestEqual(TEXT("Base health is not healed"), Boss->GetBaseHealthFraction(), 0.25f);
	const EMovementMode BeforeFinisherMovement = Player->GetCharacterMovement()->MovementMode;
	HitBoss(Boss);
	TestTrue(TEXT("Fourth accepted hit enters QTE"), Boss->QTE->IsQTEActive());
	HitBoss(Boss);
	TestEqual(TEXT("QTE rejects damage"), Boss->GetAcceptedStrikes(), 4);
	for (int32 Prompt=0; Prompt<3; ++Prompt) Boss->SubmitQTEInput(Boss->QTE->GetExpectedInput());
	TestEqual(TEXT("QTE defeats boss without ending run"), Boss->GetBossPhase(), EQuasicomboBossPhase::AwaitPickup);
	TestEqual(TEXT("Run waits for the reward"), Run->GetOutcome(), EQuasicomboRunOutcome::Playing);
	TestFalse(TEXT("Player movement is unlocked to collect reward"), Run->IsBossCombatLocked());
	TestEqual(TEXT("Player movement mode is restored after QTE"), Player->GetCharacterMovement()->MovementMode, BeforeFinisherMovement);
	TestEqual(TEXT("Boss capsule stops blocking movement"), Boss->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Boss corpse remains visible"), Boss->IsHidden());
	TestFalse(TEXT("Cannot claim reward before it appears"), Boss->ClaimVictory());
	TickBossWorld(TestWorld, 100);
	AQuasicomboVictoryPickup* Pickup = Cast<AQuasicomboVictoryPickup>(
		UGameplayStatics::GetActorOfClass(World, AQuasicomboVictoryPickup::StaticClass()));
	if (!TestNotNull(TEXT("Death spawns victory pickup"), Pickup)) return false;
	UStaticMeshComponent* PickupMesh = Pickup->FindComponentByClass<UStaticMeshComponent>();
	if (!TestNotNull(TEXT("Victory pickup has a mesh component"), PickupMesh)) return false;
	UStaticMesh* Mesh = PickupMesh->GetStaticMesh().Get();
	if (!TestNotNull(TEXT("Victory pickup has a mesh"), Mesh)) return false;
	TestEqual(TEXT("Victory pickup uses the Radical Buster mesh"), Mesh->GetPathName(),
		FString(TEXT("/Game/RadicalMike/Mesh/SM_RadicalBuster.SM_RadicalBuster")));
	TestFalse(TEXT("Pickup does not hide the corpse"), Boss->IsHidden());
	TestTrue(TEXT("Corpse starts physics after the death clip"), Boss->GetMesh()->IsSimulatingPhysics());
	TestEqual(TEXT("Corpse ignores the player"), Boss->GetMesh()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestTrue(TEXT("Collecting reward wins"), Boss->ClaimVictory());
	TestEqual(TEXT("Pickup claim ends run"), Run->GetOutcome(), EQuasicomboRunOutcome::Victory);
	TestFalse(TEXT("Corpse remains visible on victory"), Boss->IsHidden());
	TestTrue(TEXT("Dilation restored"), FMath::IsNearlyEqual(Dilation, UGameplayStatics::GetGlobalTimeDilation(World)));
	TestWorld.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossArmorRetryTest, "Quasicombo.Boss.ArmorAndSharedRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossArmorRetryTest::RunTest(const FString&)
{
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game) || !TestWorld.BeginPlayInTestWorld()) return false;
	UWorld* World = TestWorld.GetTestWorld();
	AQuasicomboBoss* Boss = World->SpawnActor<AQuasicomboBoss>();
	UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	ASideScrollingCharacter* Player = PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(
		PlayerClass, FVector(-1000.0f, 0.0f, 200.0f), FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Playable character loaded"), Player)) return false;
	auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	// Force both armor awards without depending on a live API or editor-only overrides.
	Boss->FirstArmorThreshold = Boss->SecondArmorThreshold = 0.0;
	PrepareBoss(Boss, Run, Player);
	for (int32 Strike=0; Strike<3; ++Strike) HitBoss(Boss);
	TickBossWorld(TestWorld);
	TestEqual(TEXT("Two armor bars awarded"), Boss->GetArmorBars(), 2);
	TestEqual(TEXT("Armor does not heal base"), Boss->GetBaseHealthFraction(), 0.25f);
	for (int32 Strike=0; Strike<4; ++Strike) HitBoss(Boss);
	TestEqual(TEXT("Breaking first armor removes tier"), Boss->GetArmorBars(), 1);
	TestEqual(TEXT("Armor absorbs all four hits"), Boss->GetBaseHealthFraction(), 0.25f);
	Player->ApplyDamage(100.0f, Boss, Player->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Combat death retries"), Boss->GetBossPhase(), EQuasicomboBossPhase::Retry);
	TestTrue(TEXT("Repeated failure is absorbed"), Run->TryHandleBossFailure());
	TickBossWorld(TestWorld);
	TestEqual(TEXT("Retry keeps run alive"), Run->GetOutcome(), EQuasicomboRunOutcome::Playing);
	TestEqual(TEXT("Retry restores full armor award"), Boss->GetArmorBars(), 2);
	TestEqual(TEXT("Retry heals base health"), Boss->GetBaseHealthFraction(), 1.0f);
	TestEqual(TEXT("Retry heals player"), Player->GetCurrentHP(), Player->GetMaxHP());
	TestEqual(TEXT("Only one retry consumed"), Boss->GetRetriesRemaining(), 0);
	TestTrue(TEXT("Evolution retained"), Boss->Quantum->HasEvolutionResolved());
	for (int32 Strike=0; Strike<12; ++Strike) HitBoss(Boss);
	TestTrue(TEXT("Retry can reach QTE"), Boss->QTE->IsQTEActive());
	Boss->SubmitQTEInput(EQuasicomboQTEInput::Invalid);
	TestEqual(TEXT("Second failure from QTE loses run"), Run->GetOutcome(), EQuasicomboRunOutcome::Defeat);
	TestFalse(TEXT("Failure releases QTE input"), Boss->QTE->IsQTEActive());
	TestWorld.ForwardErrorMessages(this);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossRigTest, "Quasicombo.Boss.LizardRigAndTailAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuasicomboBossRigTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
	UWorld* World = Fixture.GetTestWorld();
	UClass* BossClass = LoadClass<AQuasicomboBoss>(nullptr, TEXT("/Game/Quasicombo/Boss/BP_QC_Boss.BP_QC_Boss_C"));
	AQuasicomboBoss* Boss = BossClass ? World->SpawnActor<AQuasicomboBoss>(BossClass) : nullptr;
	if (!TestNotNull(TEXT("Boss Blueprint loads"), Boss)) return false;
	Boss->Quantum->bUseLiveQuantumApi = false;
	if (!Fixture.BeginPlayInTestWorld()) return false;
	Boss->GetCharacterMovement()->DisableMovement();
	USkeletalMeshComponent* Mesh = Boss->GetMesh();
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	if (!TestTrue(TEXT("Native lizard graph is active"), Anim && Anim->IsA<UQuasicomboBossAnimInstance>())) return false;
	TickBossWorld(Fixture, 10);
	TestTrue(FString::Printf(TEXT("Lizard stands upright (head %.1f, foot %.1f, rotation %s)"), Mesh->GetSocketLocation(TEXT("head")).Z, Mesh->GetSocketLocation(TEXT("foot_l")).Z, *Mesh->GetRelativeRotation().ToString()), Mesh->GetSocketLocation(TEXT("head")).Z > Mesh->GetSocketLocation(TEXT("foot_l")).Z + 100.0f);
	const FBoxSphereBounds BodyBounds = Mesh->GetSkeletalMeshAsset()->GetBounds();
	const float BodyBottom = Mesh->GetRelativeLocation().Z + BodyBounds.Origin.Z - BodyBounds.BoxExtent.Z;
	const float CapsuleBottom = -Boss->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	TestTrue(TEXT("Imported claw geometry is above capsule bottom in Blueprint pose"),
		BodyBottom >= CapsuleBottom && BodyBottom <= CapsuleBottom + 10.0f);
	for (int32 Index=1; Index<=7; ++Index)
		TestTrue(TEXT("Tail joint preserved"), Mesh->GetBoneIndex(FName(*FString::Printf(TEXT("u_Tail_%02d"), Index))) != INDEX_NONE);
	const FVector TailBase = Mesh->GetSocketLocation(TEXT("u_Tail_01"));
	const FVector TailTip = Mesh->GetSocketLocation(TEXT("u_Tail_07"));
	TestTrue(TEXT("Idle tail has length"), FVector::Distance(TailBase, TailTip) > 40.0f);
	if (!TestNotNull(TEXT("Tail sweep asset assigned"), Boss->TailAttackAnimation.Get())) return false;
	TestNotNull(TEXT("Hit stumble asset assigned"), Boss->HitReactionAnimation.Get());
	TestNotNull(TEXT("QTE daze asset assigned"), Boss->DazedAnimation.Get());
	TestNotNull(TEXT("Victory fall asset assigned"), Boss->DefeatAnimation.Get());
	UAnimMontage* Montage = Anim->PlaySlotAnimationAsDynamicMontage(Boss->TailAttackAnimation, TEXT("DefaultSlot"), 0.0f, 0.0f);
	TestNotNull(TEXT("Tail plays through graph slot"), Montage);
	TickBossWorld(Fixture, 45);
	TestTrue(TEXT("Tail sweep moves the tip"), FVector::Distance(TailTip, Mesh->GetSocketLocation(TEXT("u_Tail_07"))) > 40.0f);
	TestTrue(TEXT("Tail keeps its length during sweep"), FVector::Distance(Mesh->GetSocketLocation(TEXT("u_Tail_01")), Mesh->GetSocketLocation(TEXT("u_Tail_07"))) > 40.0f);
	TArray<USkeletalMeshComponent*> Parts;
	Boss->GetComponents(Parts);
	TestEqual(TEXT("Body and thirteen armor parts"), Parts.Num(), 14);
	for (USkeletalMeshComponent* Part : Parts)
		if (Part != Mesh) TestTrue(TEXT("Armor follows body pose"), Part->LeaderPoseComponent.Get() == Mesh);
	PrepareBoss(Boss, World->GetSubsystem<UQuasicomboRunSubsystem>());
	TickBossWorld(Fixture, 30);
	const float StandingHead = Mesh->GetSocketLocation(TEXT("head")).Z;
	for (int32 Strike = 0; Strike < 3; ++Strike) HitBoss(Boss);
	TickBossWorld(Fixture);
	HitBoss(Boss);
	TestEqual(TEXT("Boss enters QTE daze"), Boss->GetBossPhase(), EQuasicomboBossPhase::QTE);
	TickBossWorld(Fixture, 15);
	AddInfo(FString::Printf(TEXT("Boss head: standing %.1f dazed %.1f"), StandingHead, Mesh->GetSocketLocation(TEXT("head")).Z));
	TestTrue(TEXT("Daze visibly lowers the head"), Mesh->GetSocketLocation(TEXT("head")).Z < StandingHead - 4.0f);
	for (int32 Prompt = 0; Prompt < 3; ++Prompt) Boss->SubmitQTEInput(Boss->QTE->GetExpectedInput());
	TestEqual(TEXT("QTE enters death and reward beat"), Boss->GetBossPhase(), EQuasicomboBossPhase::AwaitPickup);
	TickBossWorld(Fixture, 90);
	AddInfo(FString::Printf(TEXT("Boss head: standing %.1f victory %.1f"), StandingHead, Mesh->GetSocketLocation(TEXT("head")).Z));
	TestTrue(TEXT("Death animation reaches a low pose"),
		Mesh->GetSocketLocation(TEXT("head")).Z < StandingHead - 75.0f);
	TestFalse(TEXT("Body remains after the fall"), Boss->IsHidden());
	TestEqual(TEXT("Fallen capsule no longer blocks the player"), Boss->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("Fallen mesh ignores the player"), Boss->GetMesh()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestNotNull(TEXT("Pickup appears beside the fallen body"), UGameplayStatics::GetActorOfClass(World, AQuasicomboVictoryPickup::StaticClass()));
	Fixture.ForwardErrorMessages(this);
	return true;
}
#endif

