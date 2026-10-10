#pragma once
#include "CoreMinimal.h"

namespace BusterTravel
{
inline double Luminance(const FVector& Color)
{
	return 0.2126 * Color.X + 0.7152 * Color.Y + 0.0722 * Color.Z;
}

/** Source hue only: the original Cascade curves already supply HDR brightness. */
inline FVector ReferenceHue(bool bCore)
{
	return bCore ? FVector(9.89 / 15.0, 0, 1) : FVector(22.74 / 25.0, 1, 6.302 / 25.0);
}

inline FVector ResolveTint(FLinearColor Color, bool bCore, float Intensity = 1.0f)
{
	FVector Hue = FVector::OneVector;
	if (FMath::IsFinite(Color.R) && FMath::IsFinite(Color.G) && FMath::IsFinite(Color.B))
	{
		const FVector Positive(FMath::Max(0.f, Color.R), FMath::Max(0.f, Color.G), FMath::Max(0.f, Color.B));
		const double Peak = Positive.GetMax();
		if (Peak > 0) Hue = Positive / Peak;
	}
	const double Gain = FMath::IsFinite(Intensity) ? FMath::Max(0.f, Intensity) : 1.f;
	return Hue * (Luminance(ReferenceHue(bCore)) / Luminance(Hue)) * Gain;
}
}
