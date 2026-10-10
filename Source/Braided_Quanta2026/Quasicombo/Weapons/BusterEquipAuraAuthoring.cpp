#if WITH_EDITOR
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/SavePackage.h"

namespace
{
const TCHAR* AuraFolder = TEXT("/Game/Quasicombo/Buster/VFX");
const TCHAR* MaterialFolder = TEXT("/Game/Quasicombo/Buster/VFX/EquipAuraMaterials");

UObject* EnsureAuraCopy(UObject* Source, const FString& Name, const TCHAR* Folder)
{
	if (!Source) return nullptr;
	const FString AssetPath = FString::Printf(TEXT("%s/%s.%s"), Folder, *Name, *Name);
	if (UObject* Existing = LoadObject<UObject>(nullptr, *AssetPath)) return Existing;
	IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	return Tools.DuplicateAsset(Name, Folder, Source);
}

bool SaveAuraAsset(UObject* Asset)
{
	if (!Asset) return false;
	Asset->MarkPackageDirty();
	return UPackage::SavePackage(Asset->GetPackage(), Asset,
		*FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension()), FSavePackageArgs());
}

void AuthorAura(const TCHAR* SourcePath, const TCHAR* DestinationName, const TCHAR* MaterialPrefix)
{
	UNiagaraSystem* Source = LoadObject<UNiagaraSystem>(nullptr, SourcePath);
	UNiagaraSystem* System = Cast<UNiagaraSystem>(EnsureAuraCopy(Source, DestinationName, AuraFolder));
	if (!System) { UE_LOG(LogTemp, Error, TEXT("BUSTER_AURA_AUTHOR: failed to duplicate %s"), SourcePath); return; }
	int32 BoundRenderers = 0;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* Emitter = Handle.GetEmitterData();
		if (!Emitter) continue;
		for (UNiagaraRendererProperties* Renderer : Emitter->GetRenderers())
		{
			if (auto* Light = Cast<UNiagaraLightRendererProperties>(Renderer))
			{
				// Imported light renderers still use their baked particle hues. The
				// project-owned colored material renderers provide the aura light.
				Light->Modify();
				Light->SetIsEnabled(false);
				continue;
			}
			UMaterialInterface* Original = nullptr;
			FNiagaraUserParameterBinding* Binding = nullptr;
			if (auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Original = Sprite->Material;
				Binding = &Sprite->MaterialUserParamBinding;
			}
			else if (auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
			{
				Original = Ribbon->Material;
				Binding = &Ribbon->MaterialUserParamBinding;
			}
			if (!Original || !Binding)
			{
				UE_LOG(LogTemp, Warning, TEXT("BUSTER_AURA_AUTHOR: unhandled renderer %s / %s"), *Handle.GetName().ToString(), *GetNameSafe(Renderer));
				continue;
			}
			if (Binding->Parameter.GetName().ToString().StartsWith(TEXT("User.BusterAura")))
			{
				++BoundRenderers;
				continue;
			}
			const FString OriginalName = Original->GetName();
			const FString CopyName = FString::Printf(TEXT("MI_Buster%s_%s"), MaterialPrefix, *OriginalName);
			UMaterialInterface* Copy = Cast<UMaterialInterface>(EnsureAuraCopy(Original, CopyName, MaterialFolder));
			if (!Copy) { UE_LOG(LogTemp, Error, TEXT("BUSTER_AURA_AUTHOR: missing copy of %s"), *Original->GetPathName()); continue; }
			const FString ParentName = Original->GetBaseMaterial() ? Original->GetBaseMaterial()->GetName() : FString();
			const TCHAR* TintParentName = ParentName == TEXT("M_Lightning") ? TEXT("M_BusterAuraTintableLightning")
				: ParentName == TEXT("M_MasterSpriteMat") ? TEXT("M_BusterAuraTintableParticles")
				: ParentName == TEXT("M_Stars") ? TEXT("M_BusterAuraTintableStars")
				: ParentName == TEXT("M_Water") ? TEXT("M_BusterAuraTintableWater")
				: ParentName == TEXT("M_SmokeSubUV") ? TEXT("M_BusterAuraTintableSmoke") : nullptr;
			if (auto* Instance = Cast<UMaterialInstanceConstant>(Copy); TintParentName && Instance)
			{
				const FString TintPath = FString::Printf(TEXT("%s/%s.%s"), MaterialFolder, TintParentName, TintParentName);
				if (UMaterialInterface* TintParent = LoadObject<UMaterialInterface>(nullptr, *TintPath)) Instance->SetParentEditorOnly(TintParent);
				else UE_LOG(LogTemp, Error, TEXT("BUSTER_AURA_AUTHOR: tint parent missing %s"), *TintPath);
			}
			else UE_LOG(LogTemp, Error, TEXT("BUSTER_AURA_AUTHOR: unsupported material for tint: %s"), *Original->GetPathName());
			if (!SaveAuraAsset(Copy)) UE_LOG(LogTemp, Error, TEXT("BUSTER_AURA_AUTHOR: failed to save %s"), *Copy->GetPathName());
			const bool bCore = OriginalName.Contains(TEXT("Main")) || OriginalName.Contains(TEXT("Lightning")) || OriginalName.Contains(TEXT("Water"));
			const FString ParameterName = FString::Printf(TEXT("User.BusterAura%sMat_%s"), bCore ? TEXT("Core") : TEXT("Spark"), *OriginalName);
			const FNiagaraVariable MaterialVariable(FNiagaraTypeDefinition::GetUMaterialDef(), FName(*ParameterName));
			System->GetExposedParameters().AddParameter(MaterialVariable);
			System->GetExposedParameters().SetUObject(Copy, MaterialVariable);
			Renderer->Modify();
			Binding->Parameter = MaterialVariable;
			if (auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer)) Sprite->Material = Copy;
			if (auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer)) Ribbon->Material = Copy;
			++BoundRenderers;
			UE_LOG(LogTemp, Display, TEXT("BUSTER_AURA_AUTHOR: %s / %s -> %s (%s)"),
				*Handle.GetName().ToString(), *OriginalName, *ParameterName, *Copy->GetPathName());
		}
	}
	if (BoundRenderers == 0) { UE_LOG(LogTemp, Error, TEXT("BUSTER_AURA_AUTHOR: no material renderers bound in %s"), *System->GetPathName()); return; }
	System->RequestCompile(true);
	System->WaitForCompilationComplete();
	UE_LOG(LogTemp, Display, TEXT("BUSTER_AURA_AUTHOR: saved=%d renderers=%d system=%s"), SaveAuraAsset(System), BoundRenderers, *System->GetPathName());
}

void AuthorEquipAuras()
{
	AuthorAura(TEXT("/Game/MegaMagicVFXBundle/VFX/MagicAuraVFX/VFX/Tsunami/Systems/N_Tsunami.N_Tsunami"),
		TEXT("N_BusterEquipTsunami"), TEXT("Tsunami"));
	AuthorAura(TEXT("/Game/MegaMagicVFXBundle/VFX/MagicAuraVFX/VFX/GalacticField/Systems/N_GalacticField.N_GalacticField"),
		TEXT("N_BusterEquipGalacticField"), TEXT("Galactic"));
	AuthorAura(TEXT("/Game/MegaMagicVFXBundle/VFX/MagicalExplosionsVFX/VFX/Tsunami/Systems/N_Tsunami.N_Tsunami"),
		TEXT("N_BusterEquipTsunamiBurst"), TEXT("TsunamiBurst"));
}

FAutoConsoleCommand AuthorEquipAurasCommand(TEXT("Quasicombo.AuthorBusterEquipAuras"),
	TEXT("Duplicate Tsunami/Galactic Field auras and Tsunami burst with project-owned tintable materials for Buster equip."),
	FConsoleCommandDelegate::CreateStatic(&AuthorEquipAuras));
}
#endif
