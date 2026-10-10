#pragma once

#include "CoreMinimal.h"
#include "QuantumApiTypes.h"

struct FQuasicomboValidatedQuantumState
{
	double VacuumProbability = 1.0;
	double TauProbability = 0.0;
};

namespace QuasicomboQuantumRules
{
	// Exact one-qubit statevector evolution, matching the API's serialized PauliEvolution state.
	inline bool EvolveLocally(const TArray<FQuantumApiComplexAmplitude>& Initial, double X, double Z, double Time,
							  TArray<FQuantumApiComplexAmplitude>& Out)
	{
		if (Initial.Num() != 2)
			return false;
		const double Norm = FMath::Square(Initial[0].Real) + FMath::Square(Initial[0].Imag) + FMath::Square(Initial[1].Real) +
							FMath::Square(Initial[1].Imag);
		if (!FMath::IsFinite(Norm) || !FMath::IsNearlyEqual(Norm, 1.0, 0.001))
			return false;
		if (!FMath::IsFinite(X) || !FMath::IsFinite(Z) || !FMath::IsFinite(Time))
			return false;
		const double R = FMath::Sqrt(X * X + Z * Z);
		const double C = FMath::Cos(R * Time);
		const double S = R > 1.e-12 ? FMath::Sin(R * Time) / R : Time;
		Out.SetNum(2);
		const auto& A = Initial[0];
		const auto& B = Initial[1];
		Out[0].Real = C * A.Real + S * (Z * A.Imag + X * B.Imag);
		Out[0].Imag = C * A.Imag - S * (Z * A.Real + X * B.Real);
		Out[1].Real = C * B.Real + S * (X * A.Imag - Z * B.Imag);
		Out[1].Imag = C * B.Imag - S * (X * A.Real - Z * B.Real);
		return true;
	}

	inline bool CanAcceptBraid(int32 RequestGeneration, int32 CurrentGeneration, bool bFrozen, bool bEvolutionStarted, bool bRunActive)
	{
		return RequestGeneration == CurrentGeneration && !bFrozen && !bEvolutionStarted && bRunActive;
	}

	inline bool CanAcceptEvolution(int32 RequestGeneration, int32 CurrentGeneration, bool bFrozen, bool bRunActive)
	{
		return RequestGeneration == CurrentGeneration && !bFrozen && bRunActive;
	}

	inline bool TryValidateState(const TArray<FQuantumApiComplexAmplitude>& Amplitudes, const TArray<double>* ReportedProbabilities,
								 FQuasicomboValidatedQuantumState& Out)
	{
		if (Amplitudes.Num() != 2)
			return false;
		const double Vacuum = FMath::Square(Amplitudes[0].Real) + FMath::Square(Amplitudes[0].Imag);
		const double Tau = FMath::Square(Amplitudes[1].Real) + FMath::Square(Amplitudes[1].Imag);
		const double Sum = Vacuum + Tau;
		if (!FMath::IsFinite(Sum) || Sum < 0.999 || Sum > 1.001)
			return false;
		Out.VacuumProbability = Vacuum / Sum;
		Out.TauProbability = Tau / Sum;
		if (ReportedProbabilities)
		{
			if (ReportedProbabilities->Num() != 2)
				return false;
			const double ReportedVacuum = (*ReportedProbabilities)[0];
			const double ReportedTau = (*ReportedProbabilities)[1];
			if (!FMath::IsFinite(ReportedVacuum) || !FMath::IsFinite(ReportedTau) || ReportedVacuum < 0.0 || ReportedVacuum > 1.0 ||
				ReportedTau < 0.0 || ReportedTau > 1.0 || !FMath::IsNearlyEqual(ReportedVacuum + ReportedTau, 1.0, 0.001) ||
				!FMath::IsNearlyEqual(ReportedVacuum, Out.VacuumProbability, 0.001) ||
				!FMath::IsNearlyEqual(ReportedTau, Out.TauProbability, 0.001))
				return false;
			Out.VacuumProbability = ReportedVacuum;
			Out.TauProbability = ReportedTau;
		}
		return true;
	}
} // namespace QuasicomboQuantumRules
