#include "QuasicomboQTEPresentation.h"

FQuasicomboQTESample FQuasicomboQTESegment::Evaluate(float Fraction) const
{
    if(Samples.IsEmpty()) return {};
    const float T=FMath::Clamp(Fraction,0.f,1.f);
    for(int32 I=1;I<Samples.Num();++I)
    {
        if(T>Samples[I].Fraction) continue;
        const auto& A=Samples[I-1]; const auto& B=Samples[I];
        const float U=FMath::Clamp((T-A.Fraction)/FMath::Max(KINDA_SMALL_NUMBER,B.Fraction-A.Fraction),0.f,1.f);
        FQuasicomboQTESample R;
        R.Fraction=T; R.MikeOffset=FMath::Lerp(A.MikeOffset,B.MikeOffset,U);
        R.LizardOffset=FMath::Lerp(A.LizardOffset,B.LizardOffset,U);
        R.LizardYaw=A.LizardYaw+FMath::FindDeltaAngleDegrees(A.LizardYaw,B.LizardYaw)*U;
        R.CameraOffset=FMath::Lerp(A.CameraOffset,B.CameraOffset,U);
        R.FocusOffset=FMath::Lerp(A.FocusOffset,B.FocusOffset,U);
        R.TargetBlend=FMath::Lerp(A.TargetBlend,B.TargetBlend,U); R.FOV=FMath::Lerp(A.FOV,B.FOV,U);
        return R;
    }
    return Samples.Last();
}

#if !WITH_EDITOR
void UQuasicomboQTEPresentation::PublishChoreography() {}
UAnimSequence* UQuasicomboQTEPresentation::CreateStagedClip(UAnimSequence*) { return nullptr; }
#endif
