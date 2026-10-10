#if WITH_DEV_AUTOMATION_TESTS

#include "QuasicomboQuantumRules.h"
#include "QuasicomboBraidFixtures.h"
#include "QuantumBossComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQuantumRulesTest,
	"Quasicombo.Quantum.CallbackAndFallbackRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboQuantumRulesTest::RunTest(const FString& Parameters)
{
	using namespace QuasicomboQuantumRules;
	TestTrue(TEXT("Current braid response is accepted during active run"), CanAcceptBraid(2, 2, false, false, true));
	TestFalse(TEXT("Superseded braid response is ignored"), CanAcceptBraid(1, 2, false, false, true));
	TestFalse(TEXT("Braid response cannot replace state after evolution starts"), CanAcceptBraid(2, 2, false, true, true));
	TestFalse(TEXT("Braid response is ignored after run ends"), CanAcceptBraid(2, 2, false, false, false));
	TestFalse(TEXT("Braid response is ignored once finisher freezes state"), CanAcceptBraid(2, 2, true, false, true));
	TestTrue(TEXT("Current evolution response is accepted during active fight"), CanAcceptEvolution(3, 3, false, true));
	TestFalse(TEXT("Late evolution cannot overwrite sampled finisher state"), CanAcceptEvolution(3, 4, true, true));
	TestFalse(TEXT("Evolution response is ignored after run ends"), CanAcceptEvolution(3, 3, false, false));

	const TArray<EQuasicomboLane> History = {EQuasicomboLane::A, EQuasicomboLane::C, EQuasicomboLane::B};
	FQuasicomboBraidFixture Fixture;
	TestTrue(TEXT("Three committed choices select a fixture"), TryGetQuasicomboBraidFixture(History, Fixture));
	TestEqual(TEXT("ACB selects route index seven"), Fixture.VacuumReal, GQuasicomboBraidFixtures[7].VacuumReal);
	const TArray<EQuasicomboLane> ShortHistory = {EQuasicomboLane::A, EQuasicomboLane::C};
	TestFalse(TEXT("Incomplete history has no fallback"),
		TryGetQuasicomboBraidFixture(ShortHistory, Fixture));
	const TArray<EQuasicomboLane> InvalidHistory =
		{EQuasicomboLane::A, static_cast<EQuasicomboLane>(255), EQuasicomboLane::B};
	TestFalse(TEXT("Invalid lane cannot index past fixture table"),
		TryGetQuasicomboBraidFixture(InvalidHistory, Fixture));

	TArray<FQuantumApiComplexAmplitude> State;
	FQuantumApiComplexAmplitude Vacuum;
	Vacuum.Real = GQuasicomboBraidFixtures[7].VacuumReal;
	Vacuum.Imag = GQuasicomboBraidFixtures[7].VacuumImag;
	FQuantumApiComplexAmplitude Tau;
	Tau.Real = GQuasicomboBraidFixtures[7].TauReal;
	Tau.Imag = GQuasicomboBraidFixtures[7].TauImag;
	State = {Vacuum, Tau};
	FQuasicomboValidatedQuantumState Validated;
	TestTrue(TEXT("Fixture preserves normalized complex state"), TryValidateState(State, nullptr, Validated));
	TestTrue(TEXT("Fixture has the expected nonzero tau channel"), Validated.TauProbability > 0.5);
	const TArray<double> Matching = {Validated.VacuumProbability, Validated.TauProbability};
	TestTrue(TEXT("Aligned API probabilities are accepted"), TryValidateState(State, &Matching, Validated));
	const TArray<double> Swapped = {Matching[1], Matching[0]};
	TestFalse(TEXT("Misaligned API probabilities trigger fallback"), TryValidateState(State, &Swapped, Validated));
	const TArray<FQuantumApiComplexAmplitude> TruncatedState = {Vacuum};
	TestFalse(TEXT("Truncated complex response triggers fallback"), TryValidateState(TruncatedState, nullptr, Validated));
	State[1].Real = 2.0;
	TestFalse(TEXT("Nonunit complex response triggers fallback"), TryValidateState(State, nullptr, Validated));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQuantumEndedRunFixtureTest,
	"Quasicombo.Quantum.EndedRunDoesNotApplyFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboQuantumEndedRunFixtureTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Run subsystem"), Run) || !TestNotNull(TEXT("Component owner"), Owner)) return false;
	UQuantumBossComponent* Quantum = NewObject<UQuantumBossComponent>(Owner);
	Owner->AddInstanceComponent(Quantum);
	Quantum->RegisterComponent();
	Quantum->bUseLiveQuantumApi = false;
	if (!Run->HasRunStarted()) Run->StartRun();
	Run->EndRun(EQuasicomboRunOutcome::Defeat);
	TestEqual(TEXT("Run ended before the boss encounter"), Run->GetOutcome(), EQuasicomboRunOutcome::Defeat);
	Quantum->FreezeForFinisher();
	Quantum->EnsureState();
	Quantum->ResumeAfterRetry();
	TestFalse(TEXT("An ended run cannot initialize a fallback state"), Quantum->HasQuantumState());
	TestFalse(TEXT("An ended run does not report a fallback update"), Quantum->UsedFallback());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQuantumActiveRunFixtureTest,
	"Quasicombo.Quantum.ActiveRunFixtureSurvivesFreezeAndRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboQuantumActiveRunFixtureTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Run subsystem"), Run) || !TestNotNull(TEXT("Component owner"), Owner)) return false;
	UQuantumBossComponent* Quantum = NewObject<UQuantumBossComponent>(Owner);
	Owner->AddInstanceComponent(Quantum);
	Quantum->RegisterComponent();
	Quantum->bUseLiveQuantumApi = false;
	if (!Run->HasRunStarted()) Run->StartRun();
	Quantum->EnsureState();
	TestTrue(TEXT("An active run initializes its validated fixture"), Quantum->HasQuantumState());
	TestTrue(TEXT("The local fixture reports fallback use"), Quantum->UsedFallback());
	const double Tau = Quantum->GetTauProbability();
	Quantum->FreezeForFinisher();
	Quantum->EnsureState();
	Quantum->ResumeAfterRetry();
	TestTrue(TEXT("Retry retains an initialized quantum state"), Quantum->HasQuantumState());
	TestEqual(TEXT("Freeze and retry preserve the captured probability"), Quantum->GetTauProbability(), Tau);
	return true;
}

#endif
