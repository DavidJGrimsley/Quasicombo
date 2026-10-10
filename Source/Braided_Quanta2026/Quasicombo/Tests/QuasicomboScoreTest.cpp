#if WITH_DEV_AUTOMATION_TESTS

#include "QuasicomboRunSubsystem.h"
#include "Misc/AutomationTest.h"

namespace
{
TArray<FQuasicomboBraidOperation> Word(std::initializer_list<TPair<int32, int32>> Crossings)
{
	TArray<FQuasicomboBraidOperation> Result;
	for (const TPair<int32, int32>& Crossing : Crossings)
	{
		FQuasicomboBraidOperation& Operation = Result.AddDefaulted_GetRef();
		Operation.Generator = Crossing.Key;
		Operation.Power = Crossing.Value;
	}
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboScoreTest, "Quasicombo.Score.Breakdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboScoreTest::RunTest(const FString& Parameters)
{
	// AAB: the two crossings are inverses. ACA: five crossings remain.
	const TArray<FQuasicomboBraidOperation> AAB = Word({{1, -1}, {1, 1}});
	const TArray<FQuasicomboBraidOperation> ACA = Word({{1, -1}, {1, -1}, {2, -1}, {2, -1}, {1, -1}});
	const FQuasicomboScoreBreakdown Cancelled = UQuasicomboRunSubsystem::CalculateScore(90.0f, 4, AAB,
		EQuasicomboMeasuredOutcome::Vacuum);
	const FQuasicomboScoreBreakdown Braided = UQuasicomboRunSubsystem::CalculateScore(90.0f, 4, ACA,
		EQuasicomboMeasuredOutcome::Vacuum);
	TestEqual(TEXT("AAB has no net crossings"), Cancelled.NetCrossings, 0);
	TestEqual(TEXT("AAB has no braid points"), Cancelled.BraidPoints, 0);
	TestEqual(TEXT("ACA keeps five crossings"), Braided.NetCrossings, 5);
	TestEqual(TEXT("ACA earns 1750 braid points"), Braided.BraidPoints, 1750);
	TestEqual(TEXT("Time points at 90 seconds"), Braided.TimePoints, 5000);
	TestEqual(TEXT("Four-hit best combo"), Braided.ComboPoints, 800);

	const FQuasicomboScoreBreakdown Tau = UQuasicomboRunSubsystem::CalculateScore(90.0f, 4, ACA,
		EQuasicomboMeasuredOutcome::Tau);
	TestEqual(TEXT("Tau adds 2000 points"), Tau.Total - Braided.Total, 2000);
	const FQuasicomboScoreBreakdown Unknown = UQuasicomboRunSubsystem::CalculateScore(90.0f, 4, ACA,
		EQuasicomboMeasuredOutcome::Unavailable);
	TestEqual(TEXT("An unrecorded outcome gives no quantum points"), Unknown.QuantumPoints, 0);
	const FQuasicomboScoreBreakdown Faster = UQuasicomboRunSubsystem::CalculateScore(45.0f, 4, ACA,
		EQuasicomboMeasuredOutcome::Tau);
	TestTrue(TEXT("Faster time scores higher with all other inputs equal"), Faster.Total > Tau.Total);
	TestEqual(TEXT("Total equals four displayed parts"), Tau.Total,
		Tau.TimePoints + Tau.ComboPoints + Tau.BraidPoints + Tau.QuantumPoints);
	return true;
}

#endif
