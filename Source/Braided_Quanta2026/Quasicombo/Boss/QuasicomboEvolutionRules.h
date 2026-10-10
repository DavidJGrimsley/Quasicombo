#pragma once
#include "CoreMinimal.h"

namespace QuasicomboEvolutionRules
{
inline constexpr int32 EyeFlashCount = 15;
inline constexpr float EyeFlashSeconds = 0.06f;
inline float ChargeCurve(float T)
{
	// Four doublings: a gentle start builds into a rapid final surge.
	return (FMath::Pow(2.0f, 4.0f * FMath::Clamp(T, 0.0f, 1.0f)) - 1.0f) / 15.0f;
}
inline float FaceArrivalSeconds(float Duration)
{
	return FMath::Max(0.01f, Duration / 12.0f);
}
struct FEyeCharge
{
	float Alpha = 0.0f;
	bool bNewEyes = false;
	bool bPermanent = false;
};
inline FEyeCharge EyeCharge(float Age, float Duration)
{
	const float Arrival = FaceArrivalSeconds(Duration);
	const float Span = FMath::Max(0.01f, Duration - Arrival);
	FEyeCharge Result;
	Result.Alpha = ChargeCurve((Age - Arrival) / Span);
	if (Age < Arrival) return Result;
	// Keep new-color flickers short. Only the old-color gaps decay exponentially.
	// Fourteen gaps range from about 306ms to 38ms at the default three seconds.
	const float GapRatio = FMath::Pow(0.125f, 1.0f / (EyeFlashCount - 2));
	const float GapBudget = FMath::Max(0.0f, Span - EyeFlashCount * EyeFlashSeconds);
	const float FirstGap = GapBudget * (1.0f - GapRatio) /
		(1.0f - FMath::Pow(GapRatio, EyeFlashCount - 1));
	float Cursor = Arrival;
	for (int32 Flash = 0; Flash < EyeFlashCount; ++Flash)
	{
		Cursor += EyeFlashSeconds;
		if (Age < Cursor || Flash == EyeFlashCount - 1)
		{
			Result.bNewEyes = true;
			Result.bPermanent = Flash == EyeFlashCount - 1;
			return Result;
		}
		Cursor += FirstGap * FMath::Pow(GapRatio, Flash);
		if (Age < Cursor) return Result;
	}
	return Result;
}
} // namespace QuasicomboEvolutionRules
