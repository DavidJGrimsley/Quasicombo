#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Color/ParticleModuleColorScaleOverLife.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Distributions/DistributionVectorParticleParameter.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Distributions/DistributionVectorConstant.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
const TCHAR* Paths[] = {
	TEXT("/Game/Quasicombo/Buster/VFX/P_BusterFireMuzzle.P_BusterFireMuzzle"),
	TEXT("/Game/Quasicombo/Buster/VFX/P_BusterFireTravel.P_BusterFireTravel"),
	TEXT("/Game/Quasicombo/Buster/VFX/P_BusterFireImpact.P_BusterFireImpact")
};
const TCHAR* ThunderSources[] = {
	TEXT("/Game/FXVarietyPack/Particles/P_ky_thunderBall.P_ky_thunderBall"),
	TEXT("/Game/FXVarietyPack/Particles/P_ky_ThunderBallHit.P_ky_ThunderBallHit")
};

void NeutralizeColor(FRawDistributionVector& Source, UParticleModule* Module)
{
	if (!Source.Distribution) return;
	FVector Samples[5];
	bool bAlreadyNeutral = true;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Samples[Index] = Source.GetValue(Index * 0.25f);
		bAlreadyNeutral &= FMath::IsNearlyEqual(Samples[Index].X, Samples[Index].Y, 0.01f) &&
			FMath::IsNearlyEqual(Samples[Index].Y, Samples[Index].Z, 0.01f);
	}
	if (bAlreadyNeutral) return;
	auto* Neutral = NewObject<UDistributionVectorConstantCurve>(Module, NAME_None, RF_Transactional);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float Strength = FMath::Max3(Samples[Index].X, Samples[Index].Y, Samples[Index].Z);
		Neutral->ConstantCurve.AddPoint(Index * 0.25f, FVector(FMath::Max(0.0f, Strength)));
	}
	Neutral->ConstantCurve.AutoSetTangents();
	Source.Distribution = Neutral;
	Source.Initialize();
}

void AuditBusterVFX()
{
	for (const TCHAR* Path : Paths)
	{
		UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, Path);
		if (!System) { UE_LOG(LogTemp, Error, TEXT("BUSTER_VFX missing %s"), Path); continue; }
		UE_LOG(LogTemp, Display, TEXT("BUSTER_VFX system=%s emitters=%d"), Path, System->Emitters.Num());
		for (UParticleEmitter* Emitter : System->Emitters)
		{
			if (!Emitter) continue;
			for (UParticleLODLevel* LOD : Emitter->LODLevels)
			{
				if (!LOD) continue;
				UE_LOG(LogTemp, Display, TEXT("BUSTER_VFX emitter=%s lod=%d modules=%d"), *Emitter->EmitterName.ToString(), LOD->Level, LOD->Modules.Num());
				if (LOD->RequiredModule)
					UE_LOG(LogTemp, Display, TEXT("BUSTER_VFX duration=%s loops=%d length=%.2f"), *Emitter->EmitterName.ToString(), LOD->RequiredModule->EmitterLoops, LOD->RequiredModule->EmitterDuration);
				for (UParticleModule* Module : LOD->Modules)
				{
					if (auto* Size = Cast<UParticleModuleSize>(Module))
						UE_LOG(LogTemp, Display, TEXT("BUSTER_VFX size=%s start=%s"), *Emitter->EmitterName.ToString(), *Size->StartSize.GetValue(0.0f).ToString());
					if (auto* Color = Cast<UParticleModuleColor>(Module))
						UE_LOG(LogTemp, Display, TEXT("BUSTER_VFX color=%s start=%s"), *Emitter->EmitterName.ToString(), *Color->StartColor.GetValue(0.0f).ToString());
					if (auto* Color = Cast<UParticleModuleColorOverLife>(Module))
						UE_LOG(LogTemp, Display, TEXT("BUSTER_VFX overlife=%s start=%s"), *Emitter->EmitterName.ToString(), *Color->ColorOverLife.GetValue(0.0f).ToString());
				}
			}
		}
	}
}

void AuditThunderVFX()
{
	for (const TCHAR* Path : ThunderSources)
	{
		UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, Path);
		if (!System) { UE_LOG(LogTemp, Error, TEXT("THUNDER_VFX missing %s"), Path); continue; }
		UE_LOG(LogTemp, Display, TEXT("THUNDER_VFX system=%s emitters=%d"), Path, System->Emitters.Num());
		for (UParticleEmitter* Emitter : System->Emitters)
		{
			if (!Emitter) continue;
			for (UParticleLODLevel* LOD : Emitter->LODLevels)
			{
				if (!LOD || !LOD->RequiredModule) continue;
				UE_LOG(LogTemp, Display, TEXT("THUNDER_VFX emitter=%s lod=%d material=%s duration=%.3f loops=%d"),
					*Emitter->EmitterName.ToString(), LOD->Level,
					LOD->RequiredModule->Material ? *LOD->RequiredModule->Material->GetPathName() : TEXT("none"),
					LOD->RequiredModule->EmitterDuration, LOD->RequiredModule->EmitterLoops);
				for (UParticleModule* Module : LOD->Modules)
				{
				if (auto* Size = Cast<UParticleModuleSize>(Module))
					UE_LOG(LogTemp, Display, TEXT("THUNDER_VFX size=%s %s"), *Emitter->EmitterName.ToString(), *Size->StartSize.GetValue(0).ToString());
				if (auto* Color = Cast<UParticleModuleColorOverLife>(Module))
					UE_LOG(LogTemp, Display, TEXT("THUNDER_VFX color=%s %s"), *Emitter->EmitterName.ToString(), *Color->ColorOverLife.GetValue(0).ToString());
				if (auto* Scale = Cast<UParticleModuleColorScaleOverLife>(Module))
					UE_LOG(LogTemp, Display, TEXT("THUNDER_VFX scale=%s %s"), *Emitter->EmitterName.ToString(), *Scale->ColorScaleOverLife.GetValue(0).ToString());
				}
			}
		}
	}
}

void AuthorBusterVFX()
{
	for (const TCHAR* Path : Paths)
	{
		UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, Path);
		if (!System) { UE_LOG(LogTemp, Error, TEXT("BUSTER_VFX missing %s"), Path); continue; }
		int32 Added = 0;
		const bool bTransient = FCString::Strstr(Path, TEXT("Muzzle")) || FCString::Strstr(Path, TEXT("Impact"));
		for (UParticleEmitter* Emitter : System->Emitters)
		{
			if (!Emitter) continue;
			for (UParticleLODLevel* LOD : Emitter->LODLevels)
			{
				if (!LOD) continue;
				if (bTransient && LOD->RequiredModule)
				{
					LOD->RequiredModule->EmitterLoops = 1;
					LOD->RequiredModule->EmitterDuration = FCString::Strstr(Path, TEXT("Muzzle")) ? 0.14f : 0.25f;
				}
				bool bHasOverLife = false;
				bool bAlreadyTinted = false;
				for (UParticleModule* Module : LOD->Modules)
				{
					bHasOverLife |= Module && Module->IsA<UParticleModuleColorOverLife>();
					if (const auto* Existing = Cast<UParticleModuleColorScaleOverLife>(Module))
						if (const auto* Param = Cast<UDistributionVectorParticleParameter>(Existing->ColorScaleOverLife.Distribution))
							bAlreadyTinted |= Param->ParameterName == TEXT("BusterColor");
				}
				if (bAlreadyTinted) continue;
				for (UParticleModule* Module : LOD->Modules)
				{
					if (auto* Color = Cast<UParticleModuleColor>(Module)) NeutralizeColor(Color->StartColor, Color);
					if (auto* Color = Cast<UParticleModuleColorOverLife>(Module)) NeutralizeColor(Color->ColorOverLife, Color);
					if (auto* Scale = Cast<UParticleModuleColorScaleOverLife>(Module)) NeutralizeColor(Scale->ColorScaleOverLife, Scale);
				}
				auto* Tint = NewObject<UParticleModuleColorScaleOverLife>(System, NAME_None, RF_Transactional);
			auto* Parameter = NewObject<UDistributionVectorParticleParameter>(Tint, NAME_None, RF_Transactional);
				Parameter->ParameterName = TEXT("BusterColor");
				Parameter->Constant = FVector::OneVector;
				for (int32 Channel = 0; Channel < 3; ++Channel) Parameter->ParamModes[Channel] = DPM_Direct;
				Tint->ColorScaleOverLife.Distribution = Parameter;
				Tint->bUpdateModule = bHasOverLife;
				LOD->Modules.Add(Tint);
				LOD->UpdateModuleLists();
				++Added;
			}
		}
		if (Added > 0 || bTransient)
		{
			System->BuildEmitters();
			System->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(System->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
			const bool bSaved = UPackage::SavePackage(System->GetPackage(), System, *Filename, FSavePackageArgs());
			UE_LOG(LogTemp, Display, TEXT("BUSTER_VFX authored=%s modules=%d saved=%d"), Path, Added, bSaved);
		}
	}
}

void NeutralizeThunderColor(FRawDistributionVector& Source, UParticleModule* Module, float Ceiling)
{
	if (!Source.Distribution) return;
	auto* Neutral = NewObject<UDistributionVectorConstantCurve>(Module, NAME_None, RF_Transactional);
	for (int32 Index = 0; Index <= 16; ++Index)
	{
		const FVector Sample = Source.GetValue(Index / 16.0f);
		const float Strength = FMath::Clamp(FMath::Max3(Sample.X, Sample.Y, Sample.Z), 0.0f, Ceiling);
		Neutral->ConstantCurve.AddPoint(Index / 16.0f, FVector(Strength));
	}
	Neutral->ConstantCurve.AutoSetTangents();
	Source.Distribution = Neutral;
	Source.Initialize();
}

bool NeutralizeTravelColor(FRawDistributionVector& Source)
{
	// Extract authored strength from the brightest channel at every original key
	// and tangent. Mike supplies hue; retain keys and interpolation without resampling.
	// Access the serialized Table through reflection; its C++ member is protected.
	const FStructProperty* TableProperty = FindFProperty<FStructProperty>(FRawDistributionVector::StaticStruct(), TEXT("Table"));
	if (!TableProperty)
	{
		UE_LOG(LogTemp, Error, TEXT("THUNDER_TRAVEL distribution Table property missing"));
		return false;
	}
	FDistributionLookupTable* Table = TableProperty->ContainerPtrToValuePtr<FDistributionLookupTable>(&Source);
	FDistributionLookupTable BakedSource = *Table;
	if (!BakedSource.IsEmpty() && (BakedSource.EntryStride != 3 || BakedSource.SubEntryStride != 0))
	{
		UE_LOG(LogTemp, Error, TEXT("THUNDER_TRAVEL unexpected color lookup layout; refusing approximate rebuild"));
		return false;
	}
	if (auto* Curve = Cast<UDistributionVectorConstantCurve>(Source.Distribution))
	{
		bool bNeutral = true;
		for (const auto& Point : Curve->ConstantCurve.Points)
			bNeutral &= Point.OutVal.Equals(FVector(Point.OutVal.X), UE_DOUBLE_SMALL_NUMBER) &&
				Point.ArriveTangent.Equals(FVector(Point.ArriveTangent.X), UE_DOUBLE_SMALL_NUMBER) &&
				Point.LeaveTangent.Equals(FVector(Point.LeaveTangent.X), UE_DOUBLE_SMALL_NUMBER);
		// Keep already-gray HDR curves and their original baked tables byte-for-byte.
		if (bNeutral) return true;
		for (auto& Point : Curve->ConstantCurve.Points)
		{
			const int32 Channel = Point.OutVal.X >= Point.OutVal.Y && Point.OutVal.X >= Point.OutVal.Z ? 0 :
				(Point.OutVal.Y >= Point.OutVal.Z ? 1 : 2);
			Point.OutVal = FVector(Point.OutVal[Channel]);
			Point.ArriveTangent = FVector(Point.ArriveTangent[Channel]);
			Point.LeaveTangent = FVector(Point.LeaveTangent[Channel]);
		}
	}
	else if (auto* Constant = Cast<UDistributionVectorConstant>(Source.Distribution))
	{
		if (Constant->Constant.Equals(FVector(Constant->Constant.X), UE_DOUBLE_SMALL_NUMBER)) return true;
		Constant->Constant = FVector(Constant->Constant.GetMax());
	}
	else if (Source.Distribution)
	{
		UE_LOG(LogTemp, Error, TEXT("THUNDER_TRAVEL unexpected color distribution %s; refusing approximate rebuild"), *Source.Distribution->GetClass()->GetName());
		return false;
	}
	if (Source.Distribution) Source.Distribution->bIsDirty = true;
	Source.Initialize();
	// UE versions can optimize the same curve into different lookup tables.
	// Preserve the source sample times and strengths instead of re-baking them.
	if (!BakedSource.IsEmpty())
	{
		for (int32 Index = 0; Index + 2 < BakedSource.Values.Num(); Index += 3)
		{
			const float Strength = FMath::Max3(BakedSource.Values[Index], BakedSource.Values[Index + 1], BakedSource.Values[Index + 2]);
			BakedSource.Values[Index] = BakedSource.Values[Index + 1] = BakedSource.Values[Index + 2] = Strength;
		}
		*Table = MoveTemp(BakedSource);
	}
	return true;
}

void AuthorThunderCopy(const TCHAR* SourcePath, const TCHAR* Name, float Duration, float Lifetime, bool bMuzzle, bool bTravel = false)
{
	UParticleSystem* Source = LoadObject<UParticleSystem>(nullptr, SourcePath);
	if (!Source) { UE_LOG(LogTemp, Error, TEXT("THUNDER_VFX source missing %s"), SourcePath); return; }
	const FString PackageName = FString::Printf(TEXT("/Game/Quasicombo/Buster/VFX/%s"), Name);
	const FString ObjectPath = PackageName + TEXT(".") + Name;
	UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, *ObjectPath);
	if (!System)
	{
		UPackage* Package = CreatePackage(*PackageName);
		System = DuplicateObject<UParticleSystem>(Source, Package, Name);
		if (!System) { UE_LOG(LogTemp, Error, TEXT("THUNDER_VFX duplicate failed %s"), Name); return; }
		System->SetFlags(RF_Public | RF_Standalone);
	}
	// Cascade LODs and modules can belong to the system rather than their emitter.
	// Duplicate the entire graph so shared modules remain shared only within this copy.
	UParticleSystem* Fresh = DuplicateObject<UParticleSystem>(Source, GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), Source->GetClass(), TEXT("BusterThunderSource")));
	if (!Fresh) { UE_LOG(LogTemp, Error, TEXT("THUNDER_VFX source graph duplicate failed %s"), Name); return; }
	TArray<UObject*> Children;
	GetObjectsWithOuter(Fresh, Children, EGetObjectsFlags::None);
	for (UObject* Child : Children)
		Child->Rename(*MakeUniqueObjectName(System, Child->GetClass(), Child->GetFName()).ToString(), System,
			REN_DontCreateRedirectors | REN_NonTransactional);
	System->Emitters = Fresh->Emitters;
	if (bMuzzle)
	{
		System->Emitters.RemoveAll([](const UParticleEmitter* Emitter)
		{
			return !Emitter || (Emitter->EmitterName != TEXT("sparkCore") &&
				Emitter->EmitterName != TEXT("spark") && Emitter->EmitterName != TEXT("burst") &&
				Emitter->EmitterName != TEXT("sparkDust") && Emitter->EmitterName != TEXT("dust"));
		});
	}
	int32 Parameterized = 0;
	TSet<UParticleModuleRequired*> UpdatedRequired;
	TSet<UParticleModule*> UpdatedModules;
	for (UParticleEmitter* Emitter : System->Emitters)
	{
		if (!Emitter) continue;
		const FString EmitterName = Emitter->EmitterName.ToString();
		const bool bCore = EmitterName == TEXT("core") || EmitterName == TEXT("burst") ||
			EmitterName == TEXT("dust") || EmitterName == TEXT("light");
		const FName ParameterName = bCore ? TEXT("BusterCoreColor") : TEXT("BusterSparkColor");
		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (!LOD) continue;
			if (LOD->RequiredModule && LOD->RequiredModule->Material && !UpdatedRequired.Contains(LOD->RequiredModule))
			{
				const FString MaterialName = bTravel
					? (bCore ? TEXT("MI_BusterThunderTravelCore") : TEXT("MI_BusterThunderTravelSparks"))
					: TEXT("M_BusterThunder_") + LOD->RequiredModule->Material->GetName();
				const FString MaterialPath = FString::Printf(TEXT("/Game/Quasicombo/Buster/VFX/ThunderMaterials/%s.%s"), *MaterialName, *MaterialName);
				UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
				if (!Material) { UE_LOG(LogTemp, Error, TEXT("THUNDER_VFX tintable material missing %s"), *MaterialPath); return; }
				LOD->RequiredModule->Material = Material;
				UpdatedRequired.Add(LOD->RequiredModule);
			}
			if (Duration > 0 && LOD->RequiredModule)
			{
				LOD->RequiredModule->EmitterLoops = 1;
				LOD->RequiredModule->EmitterDuration = Duration;
				LOD->RequiredModule->bEmitterDurationUseRange = false;
			}
			bool bAlreadyTinted = false;
			for (UParticleModule* Module : LOD->Modules)
				if (auto* Tint = Cast<UParticleModuleColorScaleOverLife>(Module))
					if (auto* Parameter = Cast<UDistributionVectorParticleParameter>(Tint->ColorScaleOverLife.Distribution))
						if (Parameter->ParameterName == TEXT("BusterCoreColor") || Parameter->ParameterName == TEXT("BusterSparkColor"))
						{
							Parameter->ParameterName = ParameterName;
							bAlreadyTinted = true;
						}
			if (bAlreadyTinted) continue;
			for (UParticleModule* Module : LOD->Modules)
			{
				if (!Module || UpdatedModules.Contains(Module)) continue;
				UpdatedModules.Add(Module);
				auto Neutralize = [bTravel, Module](FRawDistributionVector& Distribution)
				{
					if (bTravel) return NeutralizeTravelColor(Distribution);
					NeutralizeThunderColor(Distribution, Module, 255.f);
					return true;
				};
				if (auto* Color = Cast<UParticleModuleColor>(Module); Color && !Neutralize(Color->StartColor)) return;
				if (auto* Color = Cast<UParticleModuleColorOverLife>(Module); Color && !Neutralize(Color->ColorOverLife)) return;
				if (auto* Scale = Cast<UParticleModuleColorScaleOverLife>(Module); Scale && !Neutralize(Scale->ColorScaleOverLife)) return;
				if (Lifetime > 0)
					if (auto* Life = Cast<UParticleModuleLifetime>(Module))
					{
						auto* ShortLife = NewObject<UDistributionFloatConstant>(Life, NAME_None, RF_Transactional);
						ShortLife->Constant = Lifetime;
						Life->Lifetime.Distribution = ShortLife;
						Life->Lifetime.Initialize();
					}
			}
			auto* Tint = NewObject<UParticleModuleColorScaleOverLife>(System, NAME_None, RF_Transactional);
			auto* Parameter = NewObject<UDistributionVectorParticleParameter>(Tint, NAME_None, RF_Transactional);
			Parameter->ParameterName = ParameterName;
			Parameter->Constant = FVector::OneVector;
			for (int32 Channel = 0; Channel < 3; ++Channel) Parameter->ParamModes[Channel] = DPM_Direct;
			Tint->ColorScaleOverLife.Distribution = Parameter;
			Tint->bUpdateModule = true;
			LOD->Modules.Add(Tint);
			LOD->UpdateModuleLists();
			++Parameterized;
		}
	}
	System->BuildEmitters();
	System->MarkPackageDirty();
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	const bool bSaved = UPackage::SavePackage(System->GetPackage(), System, *Filename, FSavePackageArgs());
	UE_LOG(LogTemp, Display, TEXT("THUNDER_VFX authored=%s emitters=%d modules=%d saved=%d"), Name, System->Emitters.Num(), Parameterized, bSaved);
}

void AuthorBusterThunderTravel()
{
	AuthorThunderCopy(ThunderSources[0], TEXT("P_BusterThunderTravel"), 0.f, 0.f, false, true);
}

void AuthorBusterThunderVFX()
{
	AuthorBusterThunderTravel();
	AuthorThunderCopy(ThunderSources[1], TEXT("P_BusterThunderImpact"), 0.25f, 0.45f, false);
	AuthorThunderCopy(ThunderSources[1], TEXT("P_BusterThunderMuzzle"), 0.10f, 0.18f, true);
}

FAutoConsoleCommand AuditCommand(TEXT("Quasicombo.AuditBusterVFX"), TEXT("Log Fire Ball emitter sizes and colors."), FConsoleCommandDelegate::CreateStatic(&AuditBusterVFX));
FAutoConsoleCommand AuditThunderCommand(TEXT("Quasicombo.AuditThunderVFX"), TEXT("Log Thunder Ball emitter sizes and colors."), FConsoleCommandDelegate::CreateStatic(&AuditThunderVFX));
FAutoConsoleCommand AuthorCommand(TEXT("Quasicombo.AuthorBusterVFX"), TEXT("Parameterize project-owned Fire Ball effects with BusterColor."), FConsoleCommandDelegate::CreateStatic(&AuthorBusterVFX));
FAutoConsoleCommand AuthorThunderCommand(TEXT("Quasicombo.AuthorBusterThunderVFX"), TEXT("Create recolorable project-owned Thunder Ball travel, impact and muzzle effects."), FConsoleCommandDelegate::CreateStatic(&AuthorBusterThunderVFX));
FAutoConsoleCommand AuthorTravelCommand(TEXT("Quasicombo.AuthorBusterThunderTravel"), TEXT("Rebuild only Thunder travel from its source; requires travel-only materials. Does not save muzzle or impact."), FConsoleCommandDelegate::CreateStatic(&AuthorBusterThunderTravel));
}
#endif
