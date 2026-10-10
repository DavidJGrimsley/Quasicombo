#if WITH_DEV_AUTOMATION_TESTS
#include "QuasicomboBoss.h"
#include "QuasicomboBossRules.h"
#include "QuasicomboEvolutionComponent.h"
#include "QuasicomboEncounterLightingComponent.h"
#include "QuasicomboQuantumRules.h"
#include "QuasicomboRunSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "QuantumApiClient.h"
#include "QuantumApiSettings.h"
#include "HAL/PlatformProcess.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/BoxComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "SideScrollingCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvolutionMathTest, "Quasicombo.Evolution.SkinAndLocalState",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEvolutionMathTest::RunTest(const FString&)
{
	TestEqual(TEXT("Zero Tau uses first skin"), QuasicomboBossRules::SkinPalette(0.0), 0);
	TestEqual(TEXT("100 percent uses last skin"), QuasicomboBossRules::SkinPalette(1.0), 11);
	for (int32 I = 0; I < 12; ++I)
	{
		TestEqual(TEXT("Each palette is reachable"), QuasicomboBossRules::SkinPalette((I + 0.5) / 12.0), I);
		if (I > 0)
		{
			TestEqual(TEXT("Below boundary"), QuasicomboBossRules::SkinPalette(I / 12.0 - 1.e-8), I - 1);
			TestEqual(TEXT("At boundary"), QuasicomboBossRules::SkinPalette(I / 12.0), I);
		}
	}
	const double X[] = {0.3, 0.375, -0.5}, Z[] = {0.2, 0.34, 0.9};
	const double Expected[] = {0.022257303801529356, 0.3204050747485989, 0.9428294131338988};
	for (int32 I = 0; I < 3; ++I)
	{
		TArray<FQuantumApiComplexAmplitude> Initial;
		Initial.SetNum(2);
		if (I == 0)
			Initial[0].Real = 1.0;
		if (I == 1)
		{
			Initial[0].Real = FMath::Sqrt(0.5);
			Initial[1].Imag = FMath::Sqrt(0.5);
		}
		if (I == 2)
			Initial[1].Real = 1.0;
		TArray<FQuantumApiComplexAmplitude> Out;
		TestTrue(TEXT("Local evolution succeeds"), QuasicomboQuantumRules::EvolveLocally(Initial, X[I], Z[I], 0.5, Out));
		FQuasicomboValidatedQuantumState State;
		TestTrue(TEXT("Local state remains normalized"), QuasicomboQuantumRules::TryValidateState(Out, nullptr, State));
		TestTrue(TEXT("Matches Qiskit statevector reference"), FMath::IsNearlyEqual(State.TauProbability, Expected[I], 1.e-10));
		TestTrue(TEXT("Zero Hamiltonian succeeds"), QuasicomboQuantumRules::EvolveLocally(Initial, 0.0, 0.0, 0.5, Out));
		TestEqual(TEXT("Identity preserves real phase"), Out[0].Real, Initial[0].Real);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvolutionLifecycleTest, "Quasicombo.Evolution.AssemblyShieldBreakAndCancel",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEvolutionLifecycleTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld())
		return false;
	auto* W = Fixture.GetTestWorld();
	auto* Run = W->GetSubsystem<UQuasicomboRunSubsystem>();
	auto* B = W->SpawnActor<AQuasicomboBoss>();
	B->Quantum->bUseLiveQuantumApi = false;
	Run->TryCommitRoute(1, EQuasicomboLane::A);
	Run->TryCommitRoute(2, EQuasicomboLane::A);
	Run->TryCommitRoute(3, EQuasicomboLane::A);
	B->FirstArmorThreshold = B->SecondArmorThreshold = 0.0;
	B->Evolution->DurationScale = 0.001f;
	B->StartEncounter(nullptr);
	B->HitReactionAnimation =
		LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Quasicombo/Boss/Lizard/Animations/Lizard_MM_HitReact_Front_Med_01"));
	TestNotNull(TEXT("Retargeted boss reaction loads"), B->HitReactionAnimation.Get());
	auto* Skin = B->GetMesh()->GetMaterial(0);
	for (int32 I = 0; I < B->BaseHealthHits - B->EvolutionTriggerHealth; ++I)
		B->ApplyDamage(1, nullptr, B->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("One HP locks combat for evolution"), B->GetBossPhase(), EQuasicomboBossPhase::Evolution);
	for (int32 I = 0; I < 12; ++I)
		Fixture.TickTestWorld(0.02f);
	TestEqual(TEXT("Presentation returns to combat"), B->GetBossPhase(), EQuasicomboBossPhase::Combat);
	TestEqual(TEXT("Evolution refills base health with full shields"), B->GetBaseHealthFraction(), 1.0f);
	TestEqual(TEXT("Remaining hit budget includes the refill and both shields"), B->RequiredHits-B->GetAcceptedStrikes(), B->BaseHealthHits+2*B->HitsPerArmorBar);
	TestEqual(TEXT("Starting skin stays fixed"), B->GetMesh()->GetMaterial(0), Skin);
	TestEqual(TEXT("Two shields have full stagger resistance"), B->GetResistance(), 1.0f);
	if (auto* Anim = B->GetMesh()->GetAnimInstance())
	{
		auto* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Quasicombo/Boss/Lizard/Animations/Lizard_AM_ComboAttack"));
		if (TestNotNull(TEXT("Boss punch montage loads"), Montage))
		{
			Anim->Montage_Play(Montage);
			B->CombatState = ESideCombatState::Attack;
			B->ApplyDamage(1, nullptr, B->GetActorLocation(), FVector::ZeroVector);
			TestTrue(TEXT("Full armor hit preserves punch montage"), Anim->Montage_IsPlaying(Montage));
			TestEqual(TEXT("Full armor preserves attack state"), B->CombatState, ESideCombatState::Attack);
		}
	}
	for (int32 I = 0; I < 3; ++I)
		B->ApplyDamage(1, nullptr, B->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Outer shield depleted first"), B->GetArmorBars(), 1);
	TestTrue(TEXT("Broken armor remains as debris"), B->Evolution->GetDebrisCount() > 0);
	TestEqual(TEXT("Armor hits preserve refilled base health"), B->GetBaseHealthFraction(), 1.0f);
	TestEqual(TEXT("Resistance follows remaining shield"), B->GetResistance(), 0.8f);
	TestTrue(TEXT("Full-profile left shoulder survives outer break"), B->GetEvolutionArmor()[7]->IsVisible());
	TestFalse(TEXT("No replacement light shoulder pops in"), B->GetEvolutionArmor()[4]->IsVisible());
	TestEqual(TEXT("Outer break uses resistance before the hit"), B->CombatState, ESideCombatState::Attack);
	B->ApplyDamage(1, nullptr, B->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Next hit can stagger after outer break"), B->CombatState, ESideCombatState::HitReaction);
	Fixture.TickTestWorld(0.001f);
	TestTrue(TEXT("Partial armor reduces reaction strength"), FMath::IsNearlyEqual(B->GetReactionAnimationWeight(), 0.2f, 1.e-5f));
	for (int32 I = 0; I < 3; ++I)
		B->ApplyDamage(1, nullptr, B->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Empty shields collapse"), B->GetArmorBars(), 0);
	TestEqual(TEXT("All accepted armor pieces remain as posed debris"), B->Evolution->GetDebrisCount(), 12);
	TestEqual(TEXT("No armor leaves baseline resistance"), B->GetResistance(), 0.6f);
	B->Evolution->ClearDebris();
	TestEqual(TEXT("Debris cleans up"), B->Evolution->GetDebrisCount(), 0);
	Fixture.ForwardErrorMessages(this);
	return true;
}
namespace
{
class FHeldEvolutionTransport : public IQuantumApiTransport
{
public:
	FQuantumApiTransportCompletion Pending;
	float Timeout = 0.0f;
	void Send(const FQuantumApiTransportRequest& Request, FQuantumApiTransportCompletion Completion) override
	{
		Timeout = Request.TimeoutSeconds;
		Pending = MoveTemp(Completion);
	}
};
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvolutionTimeoutTest, "Quasicombo.Evolution.DeadlineRejectsLateHTTP",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEvolutionTimeoutTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld())
		return false;
	auto* B = Fixture.GetTestWorld()->SpawnActor<AQuasicomboBoss>();
	B->Quantum->bUseLiveQuantumApi = false;
	B->StartEncounter(nullptr);
	auto Held = MakeShared<FHeldEvolutionTransport>();
	auto* Settings = NewObject<UQuantumApiSettings>();
	Settings->AuthMode = EQuantumApiAuthMode::BackendProxy;
	Settings->BackendProxyUrl = TEXT("https://example.test/quantum/v1");
	B->Quantum->SetClientForTesting(MakeShared<FQuantumApiClient>(Settings, Held));
	B->Quantum->bUseLiveQuantumApi = true;
	B->Quantum->BossRequestTimeoutSeconds = 2.0f;
	B->Quantum->EvolveState();
	TestEqual(TEXT("Boss timeout reaches HTTP"), Held->Timeout, 2.0f);
	FPlatformProcess::Sleep(2.02f);
	Fixture.TickTestWorld(0.02f);
	TestTrue(TEXT("Deadline resolves without HTTP"), B->Quantum->HasEvolutionResolved());
	TestEqual(TEXT("Deadline uses local evolution"), B->Quantum->GetEvolutionSource(), EQuasicomboEvolutionSource::Local);
	const double Tau = B->Quantum->GetTauProbability();
	FQuantumApiTransportResponse Late;
	Late.bConnectedSuccessfully = true;
	Late.StatusCode = 200;
	Late.Body = TEXT("{\"final_statevector\":[{\"real\":0,\"imag\":0},{\"real\":1,\"imag\":0}],\"final_probabilities\":[0,1],\"variant\":"
					 "\"trotter_qrte\",\"provider\":\"qiskit-algorithms\",\"backend_mode\":\"statevector_estimator\"}");
	Held->Pending.ExecuteIfBound(Late);
	Held->Pending.Unbind();
	TestEqual(TEXT("Late response cannot replace local state"), B->Quantum->GetTauProbability(), Tau);
	Fixture.ForwardErrorMessages(this);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvolutionRestoreTest, "Quasicombo.Evolution.InterruptionRetryAndAggression",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEvolutionRestoreTest::RunTest(const FString&)
{
	for (double Tau : {0.2, 0.8})
	{
		FTestWorldWrapper Fixture;
		if (!Fixture.CreateTestWorld(EWorldType::Game))
			return false;
		auto* W = Fixture.GetTestWorld();
		auto* Floor = W->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(2500, 1000, 20));
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0, 0, -20));
		auto* B = W->SpawnActor<AQuasicomboBoss>(FVector(0, 0, 92), FRotator::ZeroRotator);
		UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(
			nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
		auto* Player =
			PlayerClass ? W->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(420, 0, 92), FRotator::ZeroRotator) : nullptr;
		if (!TestNotNull(TEXT("Mike loads"), Player) || !Fixture.BeginPlayInTestWorld())
			return false;
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		B->Quantum->InitializePreview(Tau);
		B->Quantum->BaseX = B->Quantum->BaseZ = 0.0;
		B->FirstArmorThreshold = B->SecondArmorThreshold = 0.0;
		B->Evolution->DurationScale = 0.001f;
		B->StartEncounter(Player);
		const auto Ground = B->GetActorTransform();
		for (int32 I = 0; I < B->BaseHealthHits - B->EvolutionTriggerHealth; ++I)
			B->ApplyDamage(1, nullptr, B->GetActorLocation(), FVector::ZeroVector);
		if (Tau < 0.3)
		{
			for (int32 I=0; I<3; ++I) Fixture.TickTestWorld(0.02f);
			TestTrue(TEXT("Rise elevates boss"), B->GetActorLocation().Z > Ground.GetLocation().Z + 100);
			TestEqual(TEXT("Presentation freezes player movement"), Player->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
			B->Evolution->Cancel();
			TestTrue(TEXT("Cancellation restores boss transform"), B->GetActorTransform().Equals(Ground, 0.1f));
			TestEqual(TEXT("Cancellation restores player movement"), Player->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
			TestEqual(TEXT("Cancellation returns combat"), B->GetBossPhase(), EQuasicomboBossPhase::Combat);
			B->HandleEncounterFailure();
			FPlatformProcess::Sleep(1.02f);
			for (int32 I = 0; I < 20; ++I)
				Fixture.TickTestWorld(0.02f);
			TestEqual(TEXT("Retry returns combat"), B->GetBossPhase(), EQuasicomboBossPhase::Combat);
			TestEqual(TEXT("Low aggression has no charge opening"), B->IsOpeningChargeActive(), false);
		}
		else
		{
			for (int32 I = 0; I < 10; ++I)
				Fixture.TickTestWorld(0.02f);
			TestTrue(TEXT("High aggression starts charge windup"), B->IsOpeningChargeActive());
			const float X = B->GetActorLocation().X;
			for (int32 I = 0; I < 90; ++I)
				Fixture.TickTestWorld(0.02f);
			TestTrue(TEXT("Charge closes distance along the arena"), B->GetActorLocation().X > X + 100);
			TestFalse(TEXT("Opening finishes and returns normal AI"), B->IsOpeningChargeActive());
			TestTrue(TEXT("Opening punch damages Mike"), Player->GetCurrentHP() < Player->GetMaxHP());
		}
		Fixture.ForwardErrorMessages(this);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterLightingTest, "Quasicombo.Boss.LightingFadesAndRestores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEncounterLightingTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* W = Fixture.GetTestWorld();
	auto* Sun = W->SpawnActor<ADirectionalLight>()->GetLightComponent();
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetIntensity(20.0f);
	auto* B = W->SpawnActor<AQuasicomboBoss>();
	B->Quantum->bUseLiveQuantumApi = false;
	B->Evolution->DurationScale = 0.001f;
	B->StartEncounter(nullptr);
	TestEqual(TEXT("Combat starts from authored sunlight"), Sun->Intensity, 20.0f);
	Fixture.TickTestWorld(0.25f);
	TestTrue(TEXT("Combat lighting fades smoothly"), Sun->Intensity > 4.0f && Sun->Intensity < 20.0f);
	// The test world's maximum delta clamps long ticks; advance in normal frames.
	for (int32 I = 0; I < 8; ++I) Fixture.TickTestWorld(0.1f);
	TestEqual(TEXT("Combat reaches four lux"), Sun->Intensity, 4.0f);
	for (int32 I = 0; I < B->BaseHealthHits - B->EvolutionTriggerHealth; ++I)
		B->ApplyDamage(1, nullptr, B->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("One HP starts evolution lighting"), B->GetBossPhase(), EQuasicomboBossPhase::Evolution);
	TestEqual(TEXT("Evolution starts from current sunlight"), Sun->Intensity, 4.0f);
	Fixture.TickTestWorld(0.25f);
	TestTrue(TEXT("Evolution lighting fades smoothly"), Sun->Intensity > 0.2f && Sun->Intensity < 4.0f);
	for (int32 I = 0; I < 12; ++I) Fixture.TickTestWorld(0.1f);
	TestEqual(TEXT("Presentation completes"), B->GetBossPhase(), EQuasicomboBossPhase::Combat);
	for (int32 I = 0; I < 11; ++I) Fixture.TickTestWorld(0.1f);
	TestEqual(TEXT("Sunlight returns to one lux after evolution"), Sun->Intensity, 1.0f);
	B->HandleEncounterFailure();
	FPlatformProcess::Sleep(1.02f);
	Fixture.TickTestWorld(0.02f);
	TestEqual(TEXT("Retry returns to combat"), B->GetBossPhase(), EQuasicomboBossPhase::Combat);
	for (int32 I = 0; I < 11; ++I) Fixture.TickTestWorld(0.1f);
	TestEqual(TEXT("Retry preserves evolved darkness"), Sun->Intensity, 1.0f);
	W->GetSubsystem<UQuasicomboRunSubsystem>()->EndRun(EQuasicomboRunOutcome::Defeat);
	TestEqual(TEXT("Run end restores authored sunlight"), Sun->Intensity, 20.0f);
	B->EncounterLighting->BeginCombat();
	Fixture.TickTestWorld(0.25f);
	TestTrue(TEXT("New fade can begin after restore"), Sun->Intensity > 4.0f && Sun->Intensity < 20.0f);
	B->Destroy();
	TestEqual(TEXT("Actor removal restores sunlight during a fade"), Sun->Intensity, 20.0f);
	Fixture.ForwardErrorMessages(this);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvolutionPerformanceTest, "Quasicombo.Evolution.HealthTriggerAndPerformance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEvolutionPerformanceTest::RunTest(const FString&)
{
	for (int32 Health : {10, 12})
	{
		FTestWorldWrapper Fixture;
		if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
		auto* B = Fixture.GetTestWorld()->SpawnActor<AQuasicomboBoss>();
		TestEqual(TEXT("Native default is ten base HP"), B->BaseHealthHits, 10);
		TestEqual(TEXT("Native evolution threshold leaves one hit"), B->EvolutionTriggerHealth, 1);
		B->BaseHealthHits = Health;
		B->Quantum->bUseLiveQuantumApi = false;
		B->FirstArmorThreshold = B->SecondArmorThreshold = 0.0;
		auto* Attachment = B->Evolution->AttachmentAnimation.LoadSynchronous();
		if (!TestNotNull(TEXT("Attachment clip loads"), Attachment)) return false;
		TestTrue(TEXT("Attachment retains its additive reference after reload"), Attachment->IsValidAdditive());
		if (!Fixture.BeginPlayInTestWorld()) return false;
		B->StartEncounter(nullptr);
		for (int32 Hit=1; Hit<Health-1; ++Hit)
		{
			B->ApplyDamage(1,nullptr,B->GetActorLocation(),FVector::ZeroVector);
			TestEqual(TEXT("Health above one stays in combat"),B->GetBossPhase(),EQuasicomboBossPhase::Combat);
		}
		B->ApplyDamage(1,nullptr,B->GetActorLocation(),FVector::ZeroVector);
		TestEqual(TEXT("Hit reaching one HP starts evolution"),B->GetBossPhase(),EQuasicomboBossPhase::Evolution);
		TestEqual(TEXT("Evolution starts one hit before death"),B->GetBaseHealthFraction(),1.0f/Health);
		B->ApplyDamage(1,nullptr,B->GetActorLocation(),FVector::ZeroVector);
		TestEqual(TEXT("Cinematic rejects damage"),B->GetAcceptedStrikes(),Health-1);
		TArray<EQuasicomboEvolutionStage> Stages;
		int32 EyeReveals=0;bool bWasAwarded=false;float Elapsed=0;
		float EyeTransitionAt=0.0f,FirstEmissive=1.0f,LastEmissive=0.0f;
		float FaceHoldElapsed=0.0f;
		bool bFinalEyesHeld=true;
		TArray<float> NewEyeDurations,OldEyeDurations;
		bool bBoneScalesValid=true, bBonePositionsValid=true;
		bool bStayedAloft=true, bFinishedImpacts=true, bDescendedDuringRoar=false, bNoSmirk=true;
		bool bFirstPoseThroughAssembly=true;
		const float GroundZ=B->GetActorLocation().Z;
		const auto InitialEyes=B->GetMesh()->GetMaterial(1);
		while (B->GetBossPhase()==EQuasicomboBossPhase::Evolution && Elapsed<12.0f)
		{
			const auto Stage=B->Evolution->GetStage();
			if (Stages.IsEmpty() || Stages.Last()!=Stage) Stages.Add(Stage);
			Fixture.TickTestWorld(1.0f/60.0f);Elapsed+=1.0f/60.0f;
			const auto AfterStage=B->Evolution->GetStage();
			if (AfterStage==EQuasicomboEvolutionStage::ArmorAssembly)
			{
				bStayedAloft &= B->GetActorLocation().Z>=GroundZ+B->Evolution->LiftHeight-0.1f;
				if (auto* Montage=B->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage())
					bFirstPoseThroughAssembly &= Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()==B->Evolution->HoverAnimation.Get();
			}
			if (AfterStage==EQuasicomboEvolutionStage::Roar)
			{
				bFinishedImpacts &= B->Evolution->GetAttachedCount()==B->Evolution->GetAssemblyPieceCount() && B->Evolution->GetAttachmentAge()>=0.28f;
				bDescendedDuringRoar |= B->GetActorLocation().Z>GroundZ+1 && B->GetActorLocation().Z<GroundZ+B->Evolution->LiftHeight-1;
			}
			bNoSmirk &= AfterStage!=EQuasicomboEvolutionStage::Smirk && B->GetMesh()->GetMorphTarget(TEXT("EvolutionSmirk"))==0.0f;
			for (const TCHAR* Bone : {TEXT("root"), TEXT("pelvis"), TEXT("head"), TEXT("hand_l"), TEXT("hand_r")})
			{
				const auto Transform=B->GetMesh()->GetSocketTransform(Bone,RTS_Component);
				bBoneScalesValid &= Transform.GetScale3D().Equals(FVector::OneVector,0.01f);
				bBonePositionsValid &= Transform.GetLocation().Size()<350.0f;
			}
			const bool bAwarded=B->GetMesh()->GetMaterial(1)!=InitialEyes;
			if(bAwarded && !bWasAwarded)
			{
				if(EyeReveals>0)OldEyeDurations.Add(Elapsed-EyeTransitionAt);
				++EyeReveals;
				EyeTransitionAt=Elapsed;
			}
			if(!bAwarded && bWasAwarded)
			{
				NewEyeDurations.Add(Elapsed-EyeTransitionAt);
				EyeTransitionAt=Elapsed;
			}
			if(bAwarded)
				if(auto* Material=Cast<UMaterialInstanceDynamic>(B->GetMesh()->GetMaterial(1)))
				{
					const float Emissive=Material->K2_GetScalarParameterValue(TEXT("EvolutionEmissiveScale"));
					if(LastEmissive==0.0f)FirstEmissive=Emissive;
					TestTrue(TEXT("New eye brightness never decreases"),Emissive>=LastEmissive-0.001f);
					LastEmissive=Emissive;
				}
			if(AfterStage==EQuasicomboEvolutionStage::FaceReveal && B->Evolution->GetStageAge()>=B->Evolution->FaceRevealSeconds)
			{
				FaceHoldElapsed+=1.0f/60.0f;
				bFinalEyesHeld &= bAwarded && FMath::IsNearlyEqual(LastEmissive,1.0f);
			}
			bWasAwarded=bAwarded;
		}
		const TArray<EQuasicomboEvolutionStage> Expected={EQuasicomboEvolutionStage::Stumble,EQuasicomboEvolutionStage::IdleRecovery,
			EQuasicomboEvolutionStage::Rise,EQuasicomboEvolutionStage::Hover,EQuasicomboEvolutionStage::FaceReveal,
			EQuasicomboEvolutionStage::ArmorAssembly,EQuasicomboEvolutionStage::Roar,EQuasicomboEvolutionStage::Return};
		TestTrue(TEXT("Stumble, idle, charge, eyes, armor, descending roar, return"),Stages==Expected);
		TestTrue(TEXT("Hold first charge pose during armor assembly"),bFirstPoseThroughAssembly);
		TestTrue(TEXT("Stay aloft until the armor finishes"),bStayedAloft && bFinishedImpacts);
		TestTrue(TEXT("Roar plays while descending"),bDescendedDuringRoar);
		TestTrue(TEXT("Smirk is absent"),bNoSmirk);
		TestEqual(TEXT("Fifteen reveals end with the new eyes staying on"),EyeReveals,15);
		if(TestEqual(TEXT("Fourteen brief flashes before the final reveal"),NewEyeDurations.Num(),14) &&
			TestEqual(TEXT("Fourteen old-color gaps"),OldEyeDurations.Num(),14))
		{
			for(float Seconds:NewEyeDurations)
				TestTrue(TEXT("Every brief new-color flash lasts 60ms"),FMath::Abs(Seconds-0.06f)<0.02f);
			TestTrue(TEXT("Old-color gaps reach one eighth of their initial duration"),
				FMath::Abs(OldEyeDurations.Last()-OldEyeDurations[0]/8.0f)<0.025f);
			const float GapRatio=FMath::Pow(0.125f,1.0f/13.0f);
			for(int32 I=1;I<14;++I)
			{
				TestTrue(TEXT("Old-color gaps decay exponentially"),FMath::Abs(OldEyeDurations[I]-OldEyeDurations[I-1]*GapRatio)<0.035f);
			}
		}
		TestTrue(TEXT("Emissive starts low and ends at normal strength"),FirstEmissive<0.15f && LastEmissive==1.0f);
		TestTrue(TEXT("Full-bright eyes hold for 0.7 seconds before armor assembly"),bFinalEyesHeld && FMath::Abs(FaceHoldElapsed-0.7f)<0.035f);
		TestTrue(TEXT("Extended face reveal finishes within twelve seconds"),Elapsed<12.0f);
		TestTrue(TEXT("Attachment impacts preserve bone scale"),bBoneScalesValid);
		TestTrue(TEXT("All cinematic bones stay within the body"),bBonePositionsValid);
		TestEqual(TEXT("Performance restores combat"),B->GetBossPhase(),EQuasicomboBossPhase::Combat);
		TestEqual(TEXT("Evolution restores configured maximum base health"),B->GetBaseHealthFraction(),1.0f);
		const int32 HealedPhaseHits=Health+B->GetArmorBars()*B->HitsPerArmorBar;
		for(int32 Hit=1;Hit<HealedPhaseHits;++Hit)
		{
			B->ApplyDamage(1,nullptr,B->GetActorLocation(),FVector::ZeroVector);
			TestEqual(TEXT("Healed boss stays in combat without evolving twice"),B->GetBossPhase(),EQuasicomboBossPhase::Combat);
		}
		TestEqual(TEXT("Full healed hit budget remains damageable down to one HP"),B->GetBaseHealthFraction(),1.0f/Health);
		TestEqual(TEXT("Smirk cleaned up"),B->GetMesh()->GetMorphTarget(TEXT("EvolutionSmirk")),0.0f);
		TestEqual(TEXT("Eyelids cleaned up"),B->GetMesh()->GetMorphTarget(TEXT("EvolutionEyeNarrow")),0.0f);
		Fixture.ForwardErrorMessages(this);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvolutionComboTimerTest, "Quasicombo.Evolution.ComboCountdownHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEvolutionComboTimerTest::RunTest(const FString&)
{
	enum class EExit { Complete, Cancel, Destroy, ExistingQTEHold, ReleaseQTEDuringEvolution, HitDuringEvolution };
	for (EExit Exit : {EExit::Complete, EExit::Cancel, EExit::Destroy, EExit::ExistingQTEHold,
		EExit::ReleaseQTEDuringEvolution, EExit::HitDuringEvolution})
	{
		FTestWorldWrapper Fixture;
		if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
		auto* World = Fixture.GetTestWorld();
		auto* Floor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(2500, 1000, 20));
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0, 0, -20));
		auto* Boss = World->SpawnActor<AQuasicomboBoss>(FVector(0, 0, 92), FRotator::ZeroRotator);
		Boss->AutoPossessAI = EAutoPossessAI::Disabled;
		auto* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
			TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
		auto* Player = PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(420, 0, 92), FRotator::ZeroRotator) : nullptr;
		if (!TestNotNull(TEXT("Countdown test loads Mike"), Player) || !Fixture.BeginPlayInTestWorld()) return false;
		Boss->Quantum->InitializePreview(0.2);
		Boss->Quantum->BaseX = Boss->Quantum->BaseZ = 0.0;
		Boss->Evolution->DurationScale = Exit == EExit::Complete ? 1.0f : 10.0f;
		Player->RegisterHitCombo();
		Player->RegisterHitCombo();
		for (int32 I = 0; I < 12; ++I) Fixture.TickTestWorld(0.1f);
		float Remaining = Player->GetHitComboTimeRemaining();
		TestTrue(TEXT("Countdown has already spent gameplay time"), Remaining > 0 && Remaining < 6.0f);
		if (Exit == EExit::ExistingQTEHold) Player->SetQTEComboHeld(true);
		Boss->StartEncounter(Player);
		for (int32 I = 0; I < Boss->BaseHealthHits - Boss->EvolutionTriggerHealth; ++I)
			Boss->ApplyDamage(1, nullptr, Boss->GetActorLocation(), FVector::ZeroVector);
		TestEqual(TEXT("Accepted threshold strike starts evolution"), Boss->GetBossPhase(), EQuasicomboBossPhase::Evolution);
		if (Exit == EExit::ReleaseQTEDuringEvolution)
		{
			Player->SetQTEComboHeld(true);
			Player->SetQTEComboHeld(false);
		}
		if (Exit == EExit::HitDuringEvolution)
		{
			// The hit that triggers evolution can register its combo after the boss starts the cutscene.
			Player->RegisterHitCombo();
			Remaining = Player->GetHitComboTimeRemaining();
			TestEqual(TEXT("A triggering hit refreshes the held countdown to seven seconds"), Remaining, 7.0f);
		}
		const int32 Combo = Player->GetHitComboCount();
		float CutsceneSeconds = 0;
		bool bCountdownStayedFrozen = true;
		while (Boss->GetBossPhase() == EQuasicomboBossPhase::Evolution && CutsceneSeconds < (Exit == EExit::Complete ? 15.0f : 8.0f))
		{
			Fixture.TickTestWorld(0.05f);
			CutsceneSeconds += 0.05f;
			if (Boss->GetBossPhase() == EQuasicomboBossPhase::Evolution)
				bCountdownStayedFrozen &= FMath::IsNearlyEqual(Player->GetHitComboTimeRemaining(), Remaining, 0.001f);
		}
		TestTrue(TEXT("Countdown remains frozen throughout a cutscene longer than the expiry window"),
			bCountdownStayedFrozen && CutsceneSeconds > 7.0f);
		TestEqual(TEXT("Cutscene preserves the current combo"), Player->GetHitComboCount(), Combo);
		if (Exit == EExit::Destroy) Boss->Destroy();
		else if (Exit != EExit::Complete) Boss->Evolution->Cancel();
		else TestEqual(TEXT("Complete cutscene returns to combat"), Boss->GetBossPhase(), EQuasicomboBossPhase::Combat);
		TestTrue(TEXT("Completion and interruption retain the pre-cutscene time budget"),
			FMath::IsNearlyEqual(Player->GetHitComboTimeRemaining(), Remaining, 0.1f));
		if (Exit == EExit::ExistingQTEHold)
		{
			for (int32 I = 0; I < 80; ++I) Fixture.TickTestWorld(0.1f);
			TestTrue(TEXT("Evolution cleanup cannot release an existing QTE hold"),
				FMath::IsNearlyEqual(Player->GetHitComboTimeRemaining(), Remaining, 0.001f));
			Player->SetQTEComboHeld(false);
		}
		// Cleanup between fixture ticks queues an unpaused timer for the next frame.
		Fixture.TickTestWorld(0.01f);
		const float ResumedRemaining = Player->GetHitComboTimeRemaining();
		for (int32 I = 0; I < 5; ++I) Fixture.TickTestWorld(0.1f);
		TestTrue(TEXT("Countdown resumes instead of restarting at seven seconds"),
			FMath::IsNearlyEqual(Player->GetHitComboTimeRemaining(), ResumedRemaining - 0.5f, 0.01f) && Player->GetHitComboCount() == Combo);
		const int32 ExpiryTicks = FMath::CeilToInt((Player->GetHitComboTimeRemaining() + 0.2f) / 0.1f);
		for (int32 I = 0; I < ExpiryTicks; ++I) Fixture.TickTestWorld(0.1f);
		TestEqual(TEXT("Resumed countdown eventually expires"), Player->GetHitComboCount(), 0);
		Fixture.ForwardErrorMessages(this);
	}
	return true;
}
#endif
