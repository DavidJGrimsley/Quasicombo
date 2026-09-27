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
	inline bool CanAcceptBraid(int32 RequestGeneration, int32 CurrentGeneration, bool bFrozen,
		bool bEvolutionStarted, bool bRunActive)
	{
		return RequestGeneration == CurrentGeneration && !bFrozen && !bEvolutionStarted && bRunActive;
	}

	inline bool CanAcceptEvolution(int32 RequestGeneration, int32 CurrentGeneration, bool bFrozen,
		bool bRunActive)
	{
		return RequestGeneration == CurrentGeneration && !bFrozen && bRunActive;
	}

	inline bool TryValidateState(const TArray<FQuantumApiComplexAmplitude>& Amplitudes,
		const TArray<double>* ReportedProbabilities, FQuasicomboValidatedQuantumState& Out)
	{
		if (Amplitudes.Num() != 2) return false;
		const double Vacuum = FMath::Square(Amplitudes[0].Real) + FMath::Square(Amplitudes[0].Imag);
		const double Tau = FMath::Square(Amplitudes[1].Real) + FMath::Square(Amplitudes[1].Imag);
		const double Sum = Vacuum + Tau;
		if (!FMath::IsFinite(Sum) || Sum < 0.999 || Sum > 1.001) return false;
		Out.VacuumProbability = Vacuum / Sum;
		Out.TauProbability = Tau / Sum;
		if (ReportedProbabilities)
		{
			if (ReportedProbabilities->Num() != 2) return false;
			const double ReportedVacuum = (*ReportedProbabilities)[0];
			const double ReportedTau = (*ReportedProbabilities)[1];
			if (!FMath::IsFinite(ReportedVacuum) || !FMath::IsFinite(ReportedTau) ||
				ReportedVacuum < 0.0 || ReportedVacuum > 1.0 || ReportedTau < 0.0 || ReportedTau > 1.0 ||
				!FMath::IsNearlyEqual(ReportedVacuum + ReportedTau, 1.0, 0.001) ||
				!FMath::IsNearlyEqual(ReportedVacuum, Out.VacuumProbability, 0.001) ||
				!FMath::IsNearlyEqual(ReportedTau, Out.TauProbability, 0.001)) return false;
			Out.VacuumProbability = ReportedVacuum;
			Out.TauProbability = ReportedTau;
		}
		return true;
	}
}
