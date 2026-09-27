#if WITH_DEV_AUTOMATION_TESTS

#include "QuasicomboBraidFixtures.h"
#include "Misc/AutomationTest.h"

namespace
{
struct FTestAmplitude
{
	double Real = 0.0;
	double Imag = 0.0;

	FTestAmplitude operator+(const FTestAmplitude& Other) const { return {Real + Other.Real, Imag + Other.Imag}; }
	FTestAmplitude operator-(const FTestAmplitude& Other) const { return {Real - Other.Real, Imag - Other.Imag}; }
	FTestAmplitude operator*(const FTestAmplitude& Other) const
	{
		return {Real * Other.Real - Imag * Other.Imag, Real * Other.Imag + Imag * Other.Real};
	}
	FTestAmplitude Conjugate() const { return {Real, -Imag}; }
	double Probability() const { return Real * Real + Imag * Imag; }
};

FTestAmplitude Phase(double Angle)
{
	return {FMath::Cos(Angle), FMath::Sin(Angle)};
}

void ApplyCrossing(int32 Generator, int32 Power, FTestAmplitude& Vacuum, FTestAmplitude& Tau)
{
	// Independent implementation of the backend's fixed Fibonacci F and R convention.
	constexpr double Pi = 3.14159265358979323846;
	const double Phi = (1.0 + FMath::Sqrt(5.0)) / 2.0;
	const double F00 = 1.0 / Phi;
	const double F01 = 1.0 / FMath::Sqrt(Phi);
	const FTestAmplitude R0 = Phase(-4.0 * Pi / 5.0);
	const FTestAmplitude R1 = FTestAmplitude{-1.0, 0.0} * Phase(-2.0 * Pi / 5.0);
	FTestAmplitude M00 = R0;
	FTestAmplitude M01;
	FTestAmplitude M10;
	FTestAmplitude M11 = R1;
	if (Generator == 2)
	{
		M00 = FTestAmplitude{F00 * F00, 0.0} * R0 + FTestAmplitude{F01 * F01, 0.0} * R1;
		M01 = FTestAmplitude{F00 * F01, 0.0} * (R0 - R1);
		M10 = M01;
		M11 = FTestAmplitude{F01 * F01, 0.0} * R0 + FTestAmplitude{F00 * F00, 0.0} * R1;
	}
	if (Power < 0)
	{
		M00 = M00.Conjugate();
		M11 = M11.Conjugate();
		const FTestAmplitude OffDiagonal = M01.Conjugate();
		M01 = M10.Conjugate();
		M10 = OffDiagonal;
	}
	const FTestAmplitude NextVacuum = M00 * Vacuum + M01 * Tau;
	const FTestAmplitude NextTau = M10 * Vacuum + M11 * Tau;
	Vacuum = NextVacuum;
	Tau = NextTau;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBraidFixturesTest, "Quasicombo.Quantum.AllRouteFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQuasicomboBraidFixturesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("There is one fixture per route history"), static_cast<int32>(UE_ARRAY_COUNT(GQuasicomboBraidFixtures)), 27);
	TArray<double> DistinctVacuumProbabilities;
	for (int32 RouteIndex = 0; RouteIndex < 27; ++RouteIndex)
	{
		FTestAmplitude Vacuum{1.0, 0.0};
		FTestAmplitude Tau;
		int32 PreviousLane = 1; // Start on B.
		for (int32 Section = 1; Section <= 3; ++Section)
		{
			const int32 Lane = Section == 1 ? RouteIndex / 9 : Section == 2 ? (RouteIndex / 3) % 3 : RouteIndex % 3;
			const int32 ForwardPower = Section == 2 ? -1 : 1;
			for (int32 Position = PreviousLane; Position < Lane; ++Position)
			{
				ApplyCrossing(Position + 1, ForwardPower, Vacuum, Tau);
			}
			for (int32 Position = PreviousLane; Position > Lane; --Position)
			{
				ApplyCrossing(Position, -ForwardPower, Vacuum, Tau);
			}
			PreviousLane = Lane;
		}
		const FQuasicomboBraidFixture& Fixture = GQuasicomboBraidFixtures[RouteIndex];
		const double Error = FMath::Max3(FMath::Abs(Vacuum.Real - Fixture.VacuumReal),
			FMath::Abs(Vacuum.Imag - Fixture.VacuumImag), FMath::Abs(Tau.Real - Fixture.TauReal));
		TestTrue(FString::Printf(TEXT("Route %d has exact complex amplitudes"), RouteIndex),
			Error < 1e-12 && FMath::Abs(Tau.Imag - Fixture.TauImag) < 1e-12);
		TestTrue(FString::Printf(TEXT("Route %d preserves unit norm"), RouteIndex),
			FMath::Abs(Vacuum.Probability() + Tau.Probability() - 1.0) < 1e-12);
		if (!DistinctVacuumProbabilities.ContainsByPredicate([&Vacuum](double Existing)
			{ return FMath::Abs(Existing - Vacuum.Probability()) < 1e-8; }))
		{
			DistinctVacuumProbabilities.Add(Vacuum.Probability());
		}
	}
	TestEqual(TEXT("The 27 routes yield four distinct vacuum probabilities"), DistinctVacuumProbabilities.Num(), 4);
	return true;
}

#endif
