#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Color/ParticleModuleColorScaleOverLife.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Distributions/DistributionVectorParticleParameter.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
void AuthorQTEEffects()
{
    auto* System=LoadObject<UParticleSystem>(nullptr,TEXT("/Game/Quasicombo/QTE/VFX/P_QTE_Lightning.P_QTE_Lightning"));
    if(!System) return;
    for(UParticleEmitter* Emitter:System->Emitters) if(Emitter) for(UParticleLODLevel* LOD:Emitter->LODLevels) if(LOD)
    {
        bool bTinted=false;
        for(UParticleModule* Module:LOD->Modules)
        {
            auto Neutralize=[Module](FRawDistributionVector& Color)
            {
                if(!Color.Distribution) return;
                auto* Curve=NewObject<UDistributionVectorConstantCurve>(Module);
                for(int32 I=0;I<=4;++I)
                {
                    const FVector V=Color.GetValue(I*.25f);
                    Curve->ConstantCurve.AddPoint(I*.25f,FVector(FMath::Max3(V.X,V.Y,V.Z)));
                }
                Color.Distribution=Curve; Color.Initialize();
            };
            if(auto* Color=Cast<UParticleModuleColor>(Module)) Neutralize(Color->StartColor);
            if(auto* Color=Cast<UParticleModuleColorOverLife>(Module)) Neutralize(Color->ColorOverLife);
            if(auto* Scale=Cast<UParticleModuleColorScaleOverLife>(Module))
            {
                if(auto* Parameter=Cast<UDistributionVectorParticleParameter>(Scale->ColorScaleOverLife.Distribution))
                    bTinted|=Parameter->ParameterName==TEXT("BusterSparkColor");
                else Neutralize(Scale->ColorScaleOverLife);
            }
        }
        if(!bTinted)
        {
            auto* Tint=NewObject<UParticleModuleColorScaleOverLife>(System);
            auto* Parameter=NewObject<UDistributionVectorParticleParameter>(Tint);
            Parameter->ParameterName=TEXT("BusterSparkColor"); Parameter->Constant=FVector::OneVector;
            for(int32 I=0;I<3;++I) Parameter->ParamModes[I]=DPM_Direct;
            Tint->ColorScaleOverLife.Distribution=Parameter; Tint->ColorScaleOverLife.Initialize();
            LOD->Modules.Add(Tint); LOD->UpdateModuleLists();
        }
    }
    System->MarkPackageDirty();
    UPackage::SavePackage(System->GetPackage(),System,*FPackageName::LongPackageNameToFilename(
        System->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),FSavePackageArgs());
}
FAutoConsoleCommand Command(TEXT("Quasicombo.AuthorQTEEffects"),TEXT("Tint only the dedicated QTE lightning copy."),FConsoleCommandDelegate::CreateStatic(&AuthorQTEEffects));
}
#endif
