#pragma once

#include "CoreMinimal.h"

namespace QuasicomboBossRules
{
	inline int32 ArmorBars(double Shift, double FirstThreshold = 0.05, double SecondThreshold = 0.15)
	{
		if (!FMath::IsFinite(Shift)) return 0;
		Shift = FMath::Abs(Shift);
		return Shift < FirstThreshold ? 0 : (Shift < SecondThreshold ? 1 : 2);
	}
	inline int32 AggressionTier(double Tau)
	{
		return Tau < 1.0 / 3.0 ? 0 : (Tau < 2.0 / 3.0 ? 1 : 2);
	}
	inline int32 QTERounds(double Tau)
	{
		const double Uncertainty = 1.0 - FMath::Abs(2.0 * FMath::Clamp(Tau, 0.0, 1.0) - 1.0);
		return 1 + AggressionTier(Uncertainty);
	}
}
