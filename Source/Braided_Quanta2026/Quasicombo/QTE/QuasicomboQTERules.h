#pragma once
#include "CoreMinimal.h"
namespace QuasicomboQTERules
{
inline int32 FinalPromptCount(double Tau, bool bCannon)
{
    const int32 Maximum = bCannon ? 6 : 5;
    const double P = FMath::IsFinite(Tau) ? FMath::Clamp(Tau, 0.0, 1.0) : 0.0;
    return FMath::Clamp(2 + FMath::FloorToInt((Maximum - 1) * P), 2, Maximum);
}
inline int32 RoundPromptCount(int32 FinalCount, int32 Round, int32 Rounds)
{
    return FMath::Clamp(FinalCount - FMath::Max(0, Rounds - Round), 2, 6);
}
}
