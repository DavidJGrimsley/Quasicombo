#if WITH_EDITOR

#include "Animation/BlendSpace.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
void RebuildMikeBlendSpace()
{
	UBlendSpace* Blend = LoadObject<UBlendSpace>(nullptr,
		TEXT("/Game/Quasicombo/Mike/BS_Mike_Locomotion.BS_Mike_Locomotion"));
	if (!Blend)
	{
		UE_LOG(LogTemp, Error, TEXT("Mike blend space is missing"));
		return;
	}
	Blend->ResampleData();
	const int32 Triangles = Blend->GetBlendSpaceData().Triangles.Num();
	if (Triangles == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Mike blend space has no interpolation triangles"));
		return;
	}
	Blend->MarkPackageDirty();
	UPackage* Package = Blend->GetPackage();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	const bool bSaved = UPackage::SavePackage(Package, Blend, *Filename, FSavePackageArgs());
	UE_LOG(LogTemp, Display, TEXT("Mike blend space rebuilt: %d triangles, saved=%s"), Triangles, bSaved ? TEXT("true") : TEXT("false"));
}

FAutoConsoleCommand RebuildMikeBlendSpaceCommand(
	TEXT("Quasicombo.RebuildMikeBlendSpace"),
	TEXT("Recalculate and save Mike's locomotion blend space after editing its samples."),
	FConsoleCommandDelegate::CreateStatic(&RebuildMikeBlendSpace));

void RefreshMikeMontageLengths()
{
	for (const TCHAR* Name : {TEXT("Combo1"), TEXT("Combo2"), TEXT("Combo3"), TEXT("Combo4"), TEXT("Combo5"),
		TEXT("ChargeHold"), TEXT("ChargeRelease"), TEXT("Dash"), TEXT("HitFront"),
		TEXT("HitBack"), TEXT("HitLeft"), TEXT("HitRight"), TEXT("Death")})
	{
		const FString Path = FString::Printf(TEXT("/Game/Quasicombo/Mike/AM_Mike_%s.AM_Mike_%s"), Name, Name);
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Path);
		if (!Montage || !Montage->GetDataModel())
		{
			UE_LOG(LogTemp, Error, TEXT("Mike montage is missing: %s"), *Path);
			continue;
		}
		const float Length = Montage->CalculateSequenceLength();
		const FFrameNumber Frames = Montage->GetDataModel()->GetFrameRate().AsFrameTime(Length).RoundToFrame();
		Montage->GetController().SetNumberOfFrames(Frames, false);
		Montage->MarkPackageDirty();
		UPackage* Package = Montage->GetPackage();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		const bool bSaved = UPackage::SavePackage(Package, Montage, *Filename, FSavePackageArgs());
		UE_LOG(LogTemp, Display, TEXT("Mike montage %s length=%.3f saved=%s"), Name,
			Montage->GetPlayLength(), bSaved ? TEXT("true") : TEXT("false"));
	}
}

FAutoConsoleCommand RefreshMikeMontageLengthsCommand(
	TEXT("Quasicombo.RefreshMikeMontageLengths"),
	TEXT("Save Mike montage lengths after changing their animation segments."),
	FConsoleCommandDelegate::CreateStatic(&RefreshMikeMontageLengths));
}

#endif
