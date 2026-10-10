#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "BusterCannonComponent.h"
#include "BusterProjectile.h"
#include "BusterTravelAppearance.h"
#include "QuasicomboLoadoutSubsystem.h"
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboBoss.h"
#include "QuasicomboEvolutionComponent.h"
#include "SideScrollingCharacter.h"
#include "SideScrollingCombatEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "ParticleEmitterInstances.h"
#include "ParticleHelper.h"
#include "Particles/Color/ParticleModuleColorScaleOverLife.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Distributions/DistributionVectorParticleParameter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include <limits>
#include "Engine/StaticMeshActor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "UObject/UnrealType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "MikeBusterAnimInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraParameterStore.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_ModifyBone.h"
#include "K2Node_VariableGet.h"
#endif

namespace
{
ASideScrollingCharacter* MakeBusterPlayer(UWorld* World)
{
	auto* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0,0,0), FRotator::ZeroRotator);
	Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(100,20,1));
	Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UClass* Class = LoadClass<ASideScrollingCharacter>(nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	return Class ? World->SpawnActor<ASideScrollingCharacter>(Class, FVector(0,0,145), FRotator::ZeroRotator) : nullptr;
}
void Step(FTestWorldWrapper& Fixture, float Seconds)
{
	for (int32 I=0; I<FMath::CeilToInt(Seconds/0.01f); ++I) Fixture.TickTestWorld(0.01f);
}
int32 BoltCount(UWorld* World)
{
	int32 Count=0;
	for (TActorIterator<ABusterProjectile> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) ++Count;
	return Count;
}
UParticleSystemComponent* FindActiveEffect(UWorld* World, const TCHAR* AssetName)
{
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
		if (It->GetWorld() == World && It->IsActive() && It->Template && It->Template->GetName() == AssetName)
			return *It;
	return nullptr;
}
UParticleSystemComponent* FindEffectWithColor(UWorld* World, const TCHAR* AssetName, FName Parameter, FLinearColor Color)
{
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		FLinearColor Actual;
		if (It->GetWorld() == World && It->IsActive() && It->Template &&
			It->Template->GetName() == AssetName && It->GetColorParameter(Parameter, Actual) &&
			Actual.Equals(Color, 0.01f)) return *It;
	}
	return nullptr;
}
UNiagaraComponent* FindActiveAura(UWorld* World, const TCHAR* AssetName)
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		if (It->GetWorld() == World && It->IsActive() && It->GetAsset() && It->GetAsset()->GetName() == AssetName)
			return *It;
	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterEquipAuraAssetTest, "Quasicombo.Buster.EquipAuraAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterEquipAuraAssetTest::RunTest(const FString&)
{
	const TCHAR* Names[] = {TEXT("N_BusterEquipTsunami"), TEXT("N_BusterEquipGalacticField")};
	for (const TCHAR* Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/Quasicombo/Buster/VFX/%s.%s"), Name, Name);
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Path);
		if (!TestNotNull(TEXT("Project-owned equip aura loads"), System)) continue;
		TArray<FNiagaraVariable> Parameters;
		System->GetExposedParameters().GetParameters(Parameters);
		bool bCore = false, bSpark = false;
		for (const FNiagaraVariable& Parameter : Parameters)
		{
			const FString ParamName = Parameter.GetName().ToString();
			if (!ParamName.StartsWith(TEXT("User.BusterAuraCoreMat_")) && !ParamName.StartsWith(TEXT("User.BusterAuraSparkMat_"))) continue;
			bCore |= ParamName.StartsWith(TEXT("User.BusterAuraCoreMat_"));
			bSpark |= ParamName.StartsWith(TEXT("User.BusterAuraSparkMat_"));
			const auto* Material = Cast<UMaterialInterface>(System->GetExposedParameters().GetUObject(Parameter));
			TestTrue(TEXT("Aura parameter defaults to a project-owned material"), Material &&
				Material->GetPathName().StartsWith(TEXT("/Game/Quasicombo/Buster/VFX/EquipAuraMaterials/")));
			FLinearColor Tint;
			TestTrue(TEXT("Every aura material exposes the actual final tint"), Material &&
				Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BusterAuraTint")), Tint));
		}
		TestTrue(TEXT("Aura has a light-color material"), bCore);
		TestTrue(TEXT("Aura has a body-color material"), bSpark);
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* Emitter = Handle.GetEmitterData();
			if (!Emitter) continue;
			for (const UNiagaraRendererProperties* Renderer : Emitter->GetRenderers())
			{
				if (const auto* Light = Cast<UNiagaraLightRendererProperties>(Renderer))
					TestFalse(TEXT("Aura has no imported off-palette light renderer"), Light->GetIsEnabled());
				const FNiagaraUserParameterBinding* Binding = nullptr;
				if (const auto* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer)) Binding = &Sprite->MaterialUserParamBinding;
				if (const auto* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer)) Binding = &Ribbon->MaterialUserParamBinding;
				if (Binding) TestTrue(TEXT("Aura sprite/ribbon uses the project material parameter"),
					Binding->Parameter.GetName().ToString().StartsWith(TEXT("User.BusterAura")));
			}
		}
	}
	UNiagaraSystem* Burst = LoadObject<UNiagaraSystem>(nullptr,
		TEXT("/Game/Quasicombo/Buster/VFX/N_BusterEquipTsunamiBurst.N_BusterEquipTsunamiBurst"));
	if (TestNotNull(TEXT("Project-owned Tsunami equip burst loads"), Burst))
	{
		TArray<FNiagaraVariable> Parameters;
		Burst->GetExposedParameters().GetParameters(Parameters);
		bool bTintableWater = false;
		for (const FNiagaraVariable& Parameter : Parameters)
			bTintableWater |= Parameter.GetName().ToString().StartsWith(TEXT("User.BusterAuraCoreMat_MI_TsunamiWater"));
		TestTrue(TEXT("Tsunami burst water uses Mike's light-color material binding"), bTintableWater);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterEquipAuraPreviewTest, "Quasicombo.Buster.EquipAuraPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterEquipAuraPreviewTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	UWorld* World = Fixture.GetTestWorld();
	ASideScrollingCharacter* Player = MakeBusterPlayer(World);
	if (!TestNotNull(TEXT("Mike"), Player)) return false;
	UBusterCannonComponent* Cannon = Player->GetBusterCannon();
	Cannon->Equip();
	Cannon->LiftSeconds = 0.05f;
	Cannon->OrbitSeconds = 0.15f;
	Cannon->LandSeconds = 0.05f;
	UAnimSequence* OpeningSequence = Cannon->OpeningAnimation.LoadSynchronous();
	if (!TestNotNull(TEXT("The supplied Idle Breaker animation loads"), OpeningSequence)) return false;
	UAnimSequence* Sequence = Cannon->EquipAnimation.LoadSynchronous();
	if (!TestNotNull(TEXT("The supplied BusterEquip_R animation loads"), Sequence)) return false;
	UAnimSequence* CalibrationSequence = Cannon->CalibrationAnimation.LoadSynchronous();
	if (!TestNotNull(TEXT("The supplied Calibration Stretches animation loads"), CalibrationSequence)) return false;
	TestEqual(TEXT("Opening animation skeleton matches Mike"), OpeningSequence->GetSkeleton(), Player->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton());
	TestTrue(TEXT("Equip sequence is the supplied asset"), Sequence->GetPathName().EndsWith(TEXT("/Anim_ZMIKE_BusterEquip_R.Anim_ZMIKE_BusterEquip_R")));
	TestEqual(TEXT("Equip skeleton matches Mike"), Sequence->GetSkeleton(), Player->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton());
	TestEqual(TEXT("Calibration animation skeleton matches Mike"), CalibrationSequence->GetSkeleton(), Player->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton());
	const FTransform Pickup(FRotator::ZeroRotator, Player->GetActorLocation() + FVector(150,0,30));
	const EBusterEquipAura Variants[] = {EBusterEquipAura::Tsunami, EBusterEquipAura::GalacticField, EBusterEquipAura::Both};
	const TCHAR* Names[] = {TEXT("N_BusterEquipTsunami"), TEXT("N_BusterEquipGalacticField"), TEXT("N_BusterEquipTsunami")};
	for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
	{
		const bool bBoth = Variants[VariantIndex] == EBusterEquipAura::Both;
		TestTrue(TEXT("Preview replays even with cannon equipped"), Cannon->PreviewAcquisition(Pickup, Variants[VariantIndex]));
		TestFalse(TEXT("Replay does not end the run"), World->GetSubsystem<UQuasicomboRunSubsystem>()->GetOutcome() != EQuasicomboRunOutcome::Playing);
		if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
			TestTrue(TEXT("Idle Breaker starts with the lift and orbit"), Anim->DynamicMontage_IsPlayingFrom(OpeningSequence));
		else AddError(TEXT("Mike has no animation instance during acquisition opening"));
		UNiagaraComponent* StartingAura = FindActiveAura(World, Names[VariantIndex]);
		if (!TestNotNull(TEXT("Selected aura starts with the lift"), StartingAura)) return false;
		const float ExpectedFootZ = -Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + Cannon->EquipVFXFootOffsetCm;
		const float StartingAuraZ = StartingAura->GetComponentLocation().Z;
		const float StartingAuraScaleZ = StartingAura->GetComponentScale().Z;
		const FVector StartingCannonPosition = Cannon->CannonMesh->GetComponentLocation();
		TestTrue(TEXT("Aura starts at the ground below Mike"), FMath::IsNearlyEqual(StartingAuraZ,
			Player->GetActorLocation().Z + ExpectedFootZ, 1.0f));
		Step(Fixture, 0.03f);
		TestTrue(TEXT("Cannon starts swirling during Mike's lift"),
			Cannon->CannonMesh->GetComponentLocation().Z > StartingCannonPosition.Z + 1.0f);
		TestTrue(TEXT("Aura expands upward with Mike"), StartingAura->GetComponentScale().Z > StartingAuraScaleZ);
		TestTrue(TEXT("Aura stays below Mike while rising"), StartingAura->GetComponentLocation().Z <
			Player->GetActorLocation().Z + ExpectedFootZ);
		UNiagaraComponent* Aura = FindActiveAura(World, Names[VariantIndex]);
		if (!TestNotNull(TEXT("Selected aura remains active during lift"), Aura)) return false;
		UNiagaraComponent* OtherAura = FindActiveAura(World, Names[1 - (VariantIndex % 2)]);
		if (bBoth) TestNotNull(TEXT("Both auras are active together"), FindActiveAura(World, Names[1]));
		else TestNull(TEXT("Other aura is not active"), OtherAura);
		const FLinearColor CoreColor(0.9f, 0.06f, 0.1f);
		const FLinearColor BodyColor(0.05f, 0.8f, 0.15f);
		UQuasicomboLoadoutSubsystem* State = World->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>();
		State->SetMikeLightColor(CoreColor);
		State->SetMikeBodyColor(BodyColor);
		TArray<FNiagaraVariable> Parameters;
		Aura->GetAsset()->GetExposedParameters().GetParameters(Parameters);
		bool bCheckedCore = false, bCheckedSpark = false;
		for (const FNiagaraVariable& Parameter : Parameters)
		{
			const FString Name = Parameter.GetName().ToString();
			const bool bCore = Name.StartsWith(TEXT("User.BusterAuraCoreMat_"));
			const bool bSpark = Name.StartsWith(TEXT("User.BusterAuraSparkMat_"));
			if (!bCore && !bSpark) continue;
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Aura->GetOverrideParameters().GetUObject(Parameter));
			if (!TestNotNull(TEXT("Aura uses a live material instance"), MID)) continue;
			TestTrue(TEXT("Aura material follows Mike's current color"), MID->K2_GetVectorParameterValue(TEXT("BusterAuraTint")).Equals(
				bCore ? CoreColor : State->GetMikeSparkColor(), 0.01f));
			bCheckedCore |= bCore;
			bCheckedSpark |= bSpark;
		}
		TestTrue(TEXT("Live core material checked"), bCheckedCore);
		TestTrue(TEXT("Live spark material checked"), bCheckedSpark);
		if (bBoth)
		{
			UNiagaraComponent* Galactic = FindActiveAura(World, Names[1]);
			if (TestNotNull(TEXT("Galactic material bindings are active in the combined preview"), Galactic))
			{
				TArray<FNiagaraVariable> GalacticParameters;
				Galactic->GetAsset()->GetExposedParameters().GetParameters(GalacticParameters);
				bool bGalacticCore = false, bGalacticSpark = false;
				for (const FNiagaraVariable& Parameter : GalacticParameters)
				{
					const FString Name = Parameter.GetName().ToString();
					const bool bCore = Name.StartsWith(TEXT("User.BusterAuraCoreMat_"));
					const bool bSpark = Name.StartsWith(TEXT("User.BusterAuraSparkMat_"));
					if (!bCore && !bSpark) continue;
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Galactic->GetOverrideParameters().GetUObject(Parameter));
					if (TestNotNull(TEXT("Combined Galactic material is live"), MID))
						TestTrue(TEXT("Combined Galactic aura follows Mike's colors"), MID->K2_GetVectorParameterValue(TEXT("BusterAuraTint")).Equals(
							bCore ? CoreColor : State->GetMikeSparkColor(), 0.01f));
					bGalacticCore |= bCore;
					bGalacticSpark |= bSpark;
				}
				TestTrue(TEXT("Combined Galactic core checked"), bGalacticCore);
				TestTrue(TEXT("Combined Galactic accents checked"), bGalacticSpark);
			}
		}
		Step(Fixture, 0.13f);
		TestEqual(TEXT("Replay reaches equip as both reach their apex"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::Equip);
		if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
		{
			TestTrue(TEXT("Supplied equip sequence plays on Mike"), Anim->DynamicMontage_IsPlayingFrom(Sequence));
			TestFalse(TEXT("Idle Breaker stops when the equip animation starts"), Anim->DynamicMontage_IsPlayingFrom(OpeningSequence));
		}
		else AddError(TEXT("Mike has no animation instance during equip"));
		TestNull(TEXT("Aura ends when equip animation begins"), FindActiveAura(World, Names[VariantIndex]));
		TestNull(TEXT("Other aura is not active"), FindActiveAura(World, Names[1 - (VariantIndex % 2)]));
		Step(Fixture, Sequence->GetPlayLength() * Cannon->AttachmentFraction + 0.05f);
		UNiagaraComponent* Burst = FindActiveAura(World, TEXT("N_BusterEquipTsunamiBurst"));
		if (VariantIndex != 1)
		{
			if (TestNotNull(TEXT("Tsunami burst starts when the cannon attaches"), Burst))
			{
				TestTrue(TEXT("Tsunami burst origin is at Mike's feet"), FMath::IsNearlyEqual(Burst->GetRelativeLocation().Z, ExpectedFootZ, 1.0f));
				TArray<FNiagaraVariable> BurstParameters;
				Burst->GetAsset()->GetExposedParameters().GetParameters(BurstParameters);
				for (const FNiagaraVariable& Parameter : BurstParameters)
				{
					if (!Parameter.GetName().ToString().StartsWith(TEXT("User.BusterAuraCoreMat_MI_TsunamiWater"))) continue;
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Burst->GetOverrideParameters().GetUObject(Parameter));
					if (TestNotNull(TEXT("Tsunami burst uses a live water material"), MID))
						TestTrue(TEXT("Tsunami burst water follows Mike's light color"), MID->K2_GetVectorParameterValue(TEXT("BusterAuraTint")).Equals(CoreColor, 0.01f));
				}
			}
		}
		else TestNull(TEXT("Galactic Field has no unrelated burst"), Burst);
		Step(Fixture, Sequence->GetPlayLength() + 0.25f);
		TestNull(TEXT("Aura is removed after equip"), FindActiveAura(World, Names[VariantIndex]));
		TestNull(TEXT("Other aura is removed after equip"), FindActiveAura(World, Names[1 - (VariantIndex % 2)]));
		Step(Fixture, CalibrationSequence->GetPlayLength() + 1.1f);
		TestNull(TEXT("Equip burst is cleaned up"), FindActiveAura(World, TEXT("N_BusterEquipTsunamiBurst")));
		TestTrue(TEXT("Replay restores equipped cannon"), Cannon->IsEquipped());
	}
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterAcquisitionTest, "Quasicombo.Buster.Acquisition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterAcquisitionTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	UWorld* World = Fixture.GetTestWorld();
	auto* Player = MakeBusterPlayer(World);
	if (!TestNotNull(TEXT("Mike"), Player)) return false;
	auto* Cannon = Player->GetBusterCannon();
	UAnimSequence* OpeningSequence = Cannon->OpeningAnimation.LoadSynchronous();
	if (!TestNotNull(TEXT("Idle Breaker sequence loads"), OpeningSequence)) return false;
	UAnimSequence* LandSequence = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/RadicalMike/Animations/Anim_ZMIKE_JumpEnd.Anim_ZMIKE_JumpEnd"));
	if (!TestNotNull(TEXT("JumpEnd landing sequence loads"), LandSequence)) return false;
	UAnimSequence* CalibrationSequence = Cannon->CalibrationAnimation.LoadSynchronous();
	if (!TestNotNull(TEXT("Calibration Stretches sequence loads"), CalibrationSequence)) return false;
	TestEqual(TEXT("Opening animation skeleton matches Mike"), OpeningSequence->GetSkeleton(), Player->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton());
	TestEqual(TEXT("Calibration animation skeleton matches Mike"), CalibrationSequence->GetSkeleton(), Player->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton());
	auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	TestFalse(TEXT("Fresh run is unequipped"), Cannon->IsEquipped());
	Run->HoldVictoryPresentation();
	Run->EndRun(EQuasicomboRunOutcome::Victory);
	const int32 Score = Run->GetVictoryScore();
	const FTransform Pickup(FRotator::ZeroRotator, Player->GetActorLocation() + FVector(150,0,20));
	TestTrue(TEXT("Acquisition begins"), Cannon->BeginAcquisition(Pickup));
	TestFalse(TEXT("Duplicate collection rejected"), Cannon->BeginAcquisition(Pickup));
	TestFalse(TEXT("Victory remains hidden"), Run->IsVictoryPresentationReady());
	if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
		TestTrue(TEXT("Idle Breaker starts when acquisition begins"), Anim->DynamicMontage_IsPlayingFrom(OpeningSequence));
	else AddError(TEXT("Mike has no animation instance during acquisition opening"));
	const float HP = Player->GetCurrentHP();
	Player->ApplyDamage(100, nullptr, Player->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Acquisition is invulnerable"), Player->GetCurrentHP(), HP);
	TestEqual(TEXT("Pickup makes two turns"), Cannon->OrbitTurns, 2);
	TestTrue(TEXT("Pickup lifts Mike higher"), Cannon->LiftHeight >= 165.0f);
	const float GroundZ = Player->GetActorLocation().Z;
	Step(Fixture, 0.4f);
	TestTrue(TEXT("Mike rises as the cannon starts swirling"), Player->GetActorLocation().Z > GroundZ + 1.0f);
	TestTrue(TEXT("Cannon rises during Mike's lift"), Cannon->CannonMesh->GetComponentLocation().Z > Pickup.GetLocation().Z + 1.0f);
	Step(Fixture, 0.46f);
	TestEqual(TEXT("Shared lift transitions into orbit"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::Orbit);
	const float MidLiftZ = Player->GetActorLocation().Z;
	const float MidCannonZ = Cannon->CannonMesh->GetComponentLocation().Z;
	Step(Fixture, 1.8f);
	TestEqual(TEXT("Shared ascent remains in orbit"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::Orbit);
	TestTrue(TEXT("Mike continues rising during cannon orbit"), Player->GetActorLocation().Z > MidLiftZ + 25.0f);
	TestTrue(TEXT("Cannon continues rising with Mike"), Cannon->CannonMesh->GetComponentLocation().Z > MidCannonZ + 25.0f);
	Step(Fixture, 0.95f);
	TestEqual(TEXT("Mike and cannon reach apex before equip"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::Equip);
	TestTrue(TEXT("Mike is at lifted height"), Player->GetActorLocation().Z >= GroundZ + Cannon->LiftHeight - 2.0f);
	if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
	{
		TestFalse(TEXT("Idle Breaker stops at the equip transition"), Anim->DynamicMontage_IsPlayingFrom(OpeningSequence));
		if (UAnimSequence* EquipSequence = Cannon->EquipAnimation.LoadSynchronous())
			TestTrue(TEXT("Buster equip animation starts at the apex"), Anim->DynamicMontage_IsPlayingFrom(EquipSequence));
	}
	else AddError(TEXT("Mike has no animation instance at the equip transition"));
	if (UAnimSequence* EquipSequence = Cannon->EquipAnimation.LoadSynchronous())
		Step(Fixture, EquipSequence->GetPlayLength() + 0.05f);
	else Step(Fixture, 2.0f);
	TestEqual(TEXT("Equip then descend"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::Land);
	Step(Fixture, Cannon->LandSeconds + 0.05f);
	TestEqual(TEXT("Grounded victory beat"), Cannon->AcquisitionPhase, EBusterAcquisitionPhase::VictoryWait);
	TestTrue(TEXT("Cannon attached"), Cannon->IsEquipped());
	TestTrue(TEXT("Cannon remains visible after pickup"), Cannon->CannonMesh->IsVisible());
	TestTrue(TEXT("Cannon keeps the Buster mesh"), Cannon->CannonMesh->GetStaticMesh() != nullptr);
	TestEqual(TEXT("Cannon stays attached to Mike"), Cannon->CannonMesh->GetAttachParent(), static_cast<USceneComponent*>(Player->GetMesh()));
	TestEqual(TEXT("Cannon uses the authored socket"), Cannon->CannonMesh->GetAttachSocketName(), FName(TEXT("Buster_RSocket")));
	TestTrue(TEXT("Attached cannon ignores the hidden Buster bone's scale"), Cannon->CannonMesh->IsUsingAbsoluteScale());
	TestTrue(TEXT("Attached cannon keeps visible world scale"), Cannon->CannonMesh->GetComponentScale().Equals(FVector::OneVector, 0.05f));
	TestFalse(TEXT("No early screen at landing"), Run->IsVictoryPresentationReady());
	if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
	{
		TestTrue(TEXT("Calibration starts immediately at touchdown"), Anim->DynamicMontage_IsPlayingFrom(CalibrationSequence));
		TestFalse(TEXT("Acquisition skips JumpEnd"), Anim->DynamicMontage_IsPlayingFrom(LandSequence));
	}
	else AddError(TEXT("Mike has no animation instance during Calibration Stretches"));
	Step(Fixture, CalibrationSequence->GetPlayLength() - 0.15f);
	TestFalse(TEXT("Victory stays hidden while Calibration Stretches plays"), Run->IsVictoryPresentationReady());
	Step(Fixture, 0.25f);
	TestFalse(TEXT("Victory stays hidden during the transition to idle"), Run->IsVictoryPresentationReady());
	Step(Fixture, 0.3f);
	if (UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance())
	{
		TestFalse(TEXT("Calibration has finished before the idle beat"), Anim->DynamicMontage_IsPlayingFrom(CalibrationSequence));
		TestTrue(TEXT("Full-body montage has blended back to locomotion idle"),
			FMath::IsNearlyZero(Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")), 0.01f));
	}
	else AddError(TEXT("Mike has no animation instance during the final idle beat"));
	TestFalse(TEXT("Presentation remains active for the idle beat"), Run->IsVictoryPresentationReady());
	Step(Fixture, 0.2f);
	TestTrue(TEXT("Victory screen released"), Run->IsVictoryPresentationReady());
	TestEqual(TEXT("Cinematic never reduces score"), Run->GetVictoryScore(), Score);
	TestEqual(TEXT("Movement restored"), Player->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
	Cannon->CompleteAcquisition();
	TestTrue(TEXT("Cleanup is idempotent"), Run->IsVictoryPresentationReady());
	TestFalse(TEXT("Victory cannot shoot"), Cannon->TryFire());
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterFireTest, "Quasicombo.Buster.FireAndColor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterFireTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* World = Fixture.GetTestWorld();
	auto* Player = MakeBusterPlayer(World);
	if (!TestNotNull(TEXT("Mike"), Player)) return false;
	auto* Cannon = Player->GetBusterCannon();
	TestFalse(TEXT("Cannot fire unequipped"), Cannon->TryFire());
	Step(Fixture,0.1f);
	auto* Anim = Cast<UMikeBusterAnimInstance>(Player->GetMesh()->GetAnimInstance());
	TestNotNull(TEXT("Mike uses layered Buster AnimBP"), Anim);
	if (Anim) TestTrue(TEXT("Unequipped arm stays neutral"), Anim->BusterPoseAlpha < 0.01f);
	Cannon->Equip();
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Step(Fixture,0.1f);
	TestTrue(TEXT("Replay-equipped cannon ignores socket scale"), Cannon->CannonMesh->IsUsingAbsoluteScale());
	TestTrue(TEXT("Replay-equipped cannon remains full size"), Cannon->CannonMesh->GetComponentScale().Equals(FVector::OneVector, 0.05f));
	if (Anim) TestTrue(TEXT("Equipped idle arm stays neutral"), Anim->BusterPoseAlpha < 0.01f);
	TestTrue(TEXT("First press fires"), Cannon->TryFire());
	TestTrue(TEXT("Mike's supplied FireR animation is selected"), Cannon->FireAnimation.GetAssetName() == TEXT("Anim_ZMIKE_FireR"));
	TestNull(TEXT("Impact effect waits for collision"), FindActiveEffect(World, TEXT("P_BusterThunderImpact")));
	TestFalse(TEXT("A second press cannot interrupt the first windup"), Cannon->TryFire());
	TestEqual(TEXT("Arm rises before bolt release"), BoltCount(World), 0);
	Step(Fixture,0.05f);
	if (Anim) TestTrue(TEXT("Right-arm fire layer raises the arm"), Anim->BusterPoseAlpha > 0.1f);
	TestEqual(TEXT("Shot waits through windup"), BoltCount(World), 0);
	Step(Fixture,0.12f);
	if (Anim) TestTrue(TEXT("FireR layer is fully blended before release"), Anim->BusterPoseAlpha > 0.8f);
	TestEqual(TEXT("Bolt still waits for settled aim"), BoltCount(World), 0);
	Step(Fixture,0.07f);
	TestEqual(TEXT("One bolt"), BoltCount(World), 1);
	UParticleSystemComponent* MuzzleFlash = FindActiveEffect(World, TEXT("P_BusterThunderMuzzle"));
	TestNotNull(TEXT("Thunder muzzle effect starts with the shot"), MuzzleFlash);
	const FTransform Muzzle = Cannon->GetMuzzleWorldTransform();
	const FVector ShotOrigin = Cannon->GetShotOriginWorldLocation();
	if (MuzzleFlash)
		TestTrue(TEXT("Muzzle effect sits just inside the cannon-idle shot origin"),
			FMath::IsNearlyEqual(FVector::DotProduct(ShotOrigin - MuzzleFlash->GetComponentLocation(),
				Muzzle.GetUnitAxis(EAxis::Y)), Cannon->MuzzleEffectInsetCm, 1.0f));
	TestTrue(TEXT("Recoil stays within four centimeters"), Cannon->CannonMesh->GetRelativeLocation().Size() <= Cannon->CannonRecoilCm + 0.1f);
	for (TActorIterator<ABusterProjectile> It(World); It; ++It)
	{
		TestTrue(TEXT("Bolt travels in Mike's combat lane"), FMath::IsNearlyEqual(It->GetActorLocation().Y, Player->GetActorLocation().Y));
		if (auto* Particles = It->FindComponentByClass<UParticleSystemComponent>())
		{
			TestTrue(TEXT("Thunder travel effect is present"), Particles->Template && Particles->Template->GetPathName().Contains(TEXT("P_BusterThunderTravel")));
			TestTrue(TEXT("Visible bolt remains at cannon-idle shot height"), FMath::IsNearlyEqual(Particles->GetComponentLocation().Z, ShotOrigin.Z, 2.0f));
			TestTrue(TEXT("Visible bolt remains at cannon-idle shot depth"), FMath::IsNearlyEqual(Particles->GetComponentLocation().Y, ShotOrigin.Y, 2.0f));
			const float ExpectedScale = Cannon->TravelDiameterCm / Cannon->ThunderVisualDiameterCm;
			TestTrue(TEXT("Travel has its independent diameter"), FMath::IsNearlyEqual(Particles->GetComponentScale().GetAbsMax(), ExpectedScale, 0.02f));
			TestTrue(TEXT("Thunder matches original Blueprint component rotation"), Particles->GetRelativeRotation().Equals(FRotator(90, 0, 0), 0.01f));
		}
		else AddError(TEXT("Projectile has no Thunder particle component"));
		TestNull(TEXT("Thunder Ball is not covered by a solid sphere"), It->FindComponentByClass<UStaticMeshComponent>());
	}
	TestTrue(TEXT("Early follow-up press is buffered once"), Cannon->TryFire());
	TestFalse(TEXT("Only one follow-up can be buffered"), Cannon->TryFire());
	Step(Fixture,0.08f);
	TestEqual(TEXT("Buffered shot waits for the 0.16-second interval"), BoltCount(World), 1);
	if (Anim) TestTrue(TEXT("Procedural arm lift is disabled in favor of FireR"), Anim->BusterArmRecoilOffset.IsNearlyZero());
	Step(Fixture,0.30f);
	TestEqual(TEXT("Buffered press releases exactly one second bolt"), BoltCount(World), 2);
	Step(Fixture,0.3f);
	TestEqual(TEXT("Holding the button does not repeat fire"), BoltCount(World), 2);
	TestTrue(TEXT("Arm holds raised pose briefly after firing"), Cannon->IsHoldingFirePose());
	if (Anim) TestTrue(TEXT("Arm never drops between release and hold"), Anim->BusterPoseAlpha > 0.8f);
	TestTrue(TEXT("Ready press starts another authored firing motion"), Cannon->TryFire());
	TestEqual(TEXT("Follow-up waits for the FireR muzzle pose"), BoltCount(World), 2);
	Step(Fixture,0.24f);
	TestEqual(TEXT("Follow-up releases a third bolt"), BoltCount(World), 3);
	Step(Fixture,0.30f);
	TestTrue(TEXT("Follow-up shot also enters short idle hold"), Cannon->IsHoldingFirePose());
	Player->GetCharacterMovement()->Velocity.X = 100.0f;
	Step(Fixture,0.02f);
	TestFalse(TEXT("Moving cancels held pose"), Cannon->IsHoldingFirePose());
	Player->GetCharacterMovement()->Velocity.X = 0;
	Step(Fixture,0.7f);
	if (Anim) TestTrue(TEXT("Arm lowers after hold"), Anim->BusterPoseAlpha < 0.1f);
	auto* State = World->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>();
	TestNotNull(TEXT("Session loadout"), State);
	const FLinearColor Color(0.9f,0.1f,0.3f);
	Cannon->SetMikeLightColor(Color);
	FLinearColor MaterialTint;
	UMaterialInterface* MikeBodyMaterial = Player->GetMesh()->GetMaterial(0);
	TestTrue(TEXT("Mike body emissive uses direct tint"), MikeBodyMaterial &&
		MikeBodyMaterial->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BusterLightColor")), MaterialTint) && MaterialTint.Equals(Color, 0.01f));
	UMaterialInterface* MikeLensMaterial = Player->GetMesh()->GetMaterial(1);
	TestTrue(TEXT("Mike lens emissive follows light color"), MikeLensMaterial &&
		MikeLensMaterial->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColorPicker Emiss")), MaterialTint) && MaterialTint.Equals(Color, 0.01f));
	UMaterialInterface* CannonMaterial = Cannon->CannonMesh->GetMaterial(0);
	TestTrue(TEXT("Cannon emissive follows direct tint"), CannonMaterial &&
		CannonMaterial->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BusterLightColor")), MaterialTint) && MaterialTint.Equals(Color, 0.01f));
	State->SetMikeBodyColor(FLinearColor(0.08f, 0.8f, 0.2f));
	const FLinearColor Spark = State->GetMikeSparkColor();
	for (TActorIterator<ABusterProjectile> It(World); It; ++It)
	{
		FVector Actual, ActualSpark;
		auto* Particles = It->FindComponentByClass<UParticleSystemComponent>();
		TestTrue(TEXT("Bolt has floating-point core parameter"), Particles && Particles->GetVectorParameter(TEXT("BusterCoreColor"), Actual));
		TestTrue(TEXT("In-flight bolt changes compensated core color"), Actual.Equals(BusterTravel::ResolveTint(Color, true), 0.01f));
		TestTrue(TEXT("Bolt has floating-point spark parameter"), Particles && Particles->GetVectorParameter(TEXT("BusterSparkColor"), ActualSpark));
		TestTrue(TEXT("In-flight bolt changes compensated spark color"), ActualSpark.Equals(BusterTravel::ResolveTint(Spark, false), 0.01f));
	}
	TestTrue(TEXT("New press fires again"), Cannon->TryFire());
	Step(Fixture, 0.24f);
	TestNotNull(TEXT("New muzzle core uses Mike's lights"), FindEffectWithColor(World, TEXT("P_BusterThunderMuzzle"), TEXT("BusterCoreColor"), Color));
	TestNotNull(TEXT("New muzzle sparks use Mike's body"), FindEffectWithColor(World, TEXT("P_BusterThunderMuzzle"), TEXT("BusterSparkColor"), Spark));
	Step(Fixture, 0.3f);
	Player->SetActorLocation(FVector(0,0,600));
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Player->GetCharacterMovement()->Velocity = FVector(240,0,-80);
	const float Gravity = Player->GetCharacterMovement()->GravityScale;
	TestTrue(TEXT("Airborne shot accepted"), Cannon->TryFire());
	TestEqual(TEXT("Horizontal velocity preserved"), Player->GetCharacterMovement()->Velocity.X, 240.0);
	TestTrue(TEXT("Air shot keeps half gravity"), FMath::IsNearlyEqual(Player->GetCharacterMovement()->GravityScale, Gravity * 0.5f));
	TestEqual(TEXT("Air shot retains half falling speed"), Player->GetCharacterMovement()->Velocity.Z, -40.0);
	Cannon->CancelFire();
	TestEqual(TEXT("Gravity restored on interruption"), Player->GetCharacterMovement()->GravityScale, Gravity);
	TestEqual(TEXT("Vertical velocity restored"), Player->GetCharacterMovement()->Velocity.Z, -80.0);
	Fixture.ForwardErrorMessages(this);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterArmRecoilGraphTest, "Quasicombo.Buster.ArmRecoilGraph", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterArmRecoilGraphTest::RunTest(const FString&)
{
	auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Quasicombo/Mike/ABP_Mike_SideScroller.ABP_Mike_SideScroller"));
	if (!TestNotNull(TEXT("Mike animation Blueprint"), BP)) return false;
	TArray<UEdGraph*> Graphs;
	BP->GetAllGraphs(Graphs);
	bool bWired = false;
	for (UEdGraph* Graph : Graphs)
		for (UEdGraphNode* Node : Graph->Nodes)
			if (const auto* Bone = Cast<UAnimGraphNode_ModifyBone>(Node))
				if (Bone->Node.BoneToModify.BoneName == TEXT("clavicle_r") &&
					Bone->Node.TranslationMode == BMM_Additive && Bone->Node.TranslationSpace == BCS_ComponentSpace)
				{
					const UEdGraphPin* Translation = Bone->FindPin(TEXT("Translation"));
					bWired = Translation && Translation->LinkedTo.Num() == 1 &&
						Cast<UK2Node_VariableGet>(Translation->LinkedTo[0]->GetOwningNode()) &&
						Cast<UK2Node_VariableGet>(Translation->LinkedTo[0]->GetOwningNode())->VariableReference.GetMemberName() == TEXT("BusterArmRecoilOffset");
				}
	TestTrue(TEXT("Right clavicle translates from live two-centimeter recoil offset"), bWired);
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterVFXAssetTest, "Quasicombo.Buster.ThunderAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterVFXAssetTest::RunTest(const FString&)
{
	const TCHAR* Names[] = {TEXT("P_BusterThunderMuzzle"), TEXT("P_BusterThunderTravel"), TEXT("P_BusterThunderImpact")};
	for (const TCHAR* Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/Quasicombo/Buster/VFX/%s.%s"), Name, Name);
		UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, *Path);
		if (!TestNotNull(*Path, System)) continue;
		TestTrue(TEXT("Thunder derivative has emitters"), System->Emitters.Num() > 0);
		bool bSawCore = false, bSawSpark = false;
		for (UParticleEmitter* Emitter : System->Emitters)
			if (Emitter)
				for (UParticleLODLevel* LOD : Emitter->LODLevels)
					if (LOD)
					{
						bool bTinted = false;
						UMaterialInterface* Material = LOD->RequiredModule ? LOD->RequiredModule->Material.Get() : nullptr;
						if (TestNotNull(TEXT("Thunder emitter has a material"), Material))
						{
							TestTrue(TEXT("Thunder uses a project-owned material"), Material->GetPathName().StartsWith(TEXT("/Game/Quasicombo/Buster/VFX/ThunderMaterials/")));
							const bool bTravelCore = FCString::Strcmp(Name, TEXT("P_BusterThunderTravel")) == 0 && Emitter->EmitterName == TEXT("core");
							TestEqual(TEXT("Travel core composites; electricity and approved explosions remain additive"), Material->GetBlendMode(), bTravelCore ? BLEND_AlphaComposite : BLEND_Additive);
						}
						const FString EmitterName = Emitter->EmitterName.ToString();
						const bool bCore = EmitterName == TEXT("core") || EmitterName == TEXT("burst") ||
							EmitterName == TEXT("dust") || EmitterName == TEXT("light");
						const FName Expected = bCore ? TEXT("BusterCoreColor") : TEXT("BusterSparkColor");
						for (UParticleModule* Module : LOD->Modules)
							if (const auto* Scale = Cast<UParticleModuleColorScaleOverLife>(Module))
								if (const auto* Param = Cast<UDistributionVectorParticleParameter>(Scale->ColorScaleOverLife.Distribution))
									bTinted |= Param->ParameterName == Expected;
						bSawCore |= bCore;
						bSawSpark |= !bCore;
						TestTrue(*FString::Printf(TEXT("%s emitter %s LOD%d reads %s"), Name, *Emitter->EmitterName.ToString(), LOD->Level, *Expected.ToString()), bTinted);
					}
		TestTrue(TEXT("Thunder effect includes a core"), bSawCore);
		TestTrue(TEXT("Thunder effect includes sparks"), bSawSpark);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterThunderParticleColorTest, "Quasicombo.Buster.ThunderParticleColors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterThunderParticleColorTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	const FLinearColor Light(1.0f, 0.82f, 0.05f), Spark(1.0f, 0.08f, 0.5f);
	const TCHAR* Names[] = {TEXT("P_BusterThunderTravel"), TEXT("P_BusterThunderMuzzle"), TEXT("P_BusterThunderImpact")};
	for (const TCHAR* Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/Quasicombo/Buster/VFX/%s.%s"), Name, Name);
		UParticleSystem* System = LoadObject<UParticleSystem>(nullptr, *Path);
		if (!TestNotNull(*Path, System)) continue;
		UParticleSystemComponent* Effect = UGameplayStatics::SpawnEmitterAtLocation(Fixture.GetTestWorld(), System,
			FTransform::Identity, false, EPSCPoolMethod::None, false);
		if (!TestNotNull(TEXT("Thunder particle test component"), Effect)) continue;
		const bool bTravel = FCString::Strcmp(Name, TEXT("P_BusterThunderTravel")) == 0;
		if (bTravel)
		{
			Effect->SetVectorParameter(TEXT("BusterCoreColor"), BusterTravel::ResolveTint(Light, true));
			Effect->SetVectorParameter(TEXT("BusterSparkColor"), BusterTravel::ResolveTint(Spark, false));
		}
		else
		{
			Effect->SetColorParameter(TEXT("BusterCoreColor"), Light);
			Effect->SetColorParameter(TEXT("BusterSparkColor"), Spark);
		}
		Effect->Activate(true);
		Step(Fixture, 0.05f);
		bool bCore = false, bSpark = false;
		for (FParticleEmitterInstance* Instance : Effect->EmitterInstances)
		{
			if (!Instance || !Instance->SpriteTemplate) continue;
			const FString EmitterName = Instance->SpriteTemplate->EmitterName.ToString();
			if (EmitterName == TEXT("light")) continue;
			const bool bCoreEmitter = EmitterName == TEXT("core") || EmitterName == TEXT("burst") || EmitterName == TEXT("dust");
			const FVector Expected = FVector(bCoreEmitter ? Light : Spark);
			for (int32 Index = 0; Index < Instance->ActiveParticles; ++Index)
			{
				const FBaseParticle* Particle = Instance->GetParticle(Index);
				if (!Particle) continue;
				const FVector RGB(Particle->Color.R, Particle->Color.G, Particle->Color.B);
				const double Strength = FMath::Max3(RGB.X, RGB.Y, RGB.Z);
				if (Strength <= 0.01f) continue;
				TestTrue(TEXT("Particle RGB is finite"), FMath::IsFinite(RGB.X) && FMath::IsFinite(RGB.Y) && FMath::IsFinite(RGB.Z));
				if (bTravel) TestTrue(TEXT("Travel particle retains authored HDR above one"), Strength > 1.0);
				TestTrue(*FString::Printf(TEXT("%s %s emitted particles use %s"), Name, *EmitterName,
					bCoreEmitter ? TEXT("yellow lights") : TEXT("pink body color")), (RGB / Strength).Equals(Expected, 0.03f));
				bCore |= bCoreEmitter;
				bSpark |= !bCoreEmitter;
				break;
			}
		}
		TestTrue(*FString::Printf(TEXT("%s emits light-colored core particle data (visual acceptance remains manual)"), Name), bCore);
		TestTrue(*FString::Printf(TEXT("%s emits body-colored electricity particle data"), Name), bSpark);
		Effect->DestroyComponent();
	}
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterTravelTintTest, "Quasicombo.Buster.TravelTintLuminance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterTravelTintTest::RunTest(const FString&)
{
	const FLinearColor Colors[] = {
		FLinearColor(1, .82f, .05f), FLinearColor(1, .08f, .5f),
		FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue,
		FLinearColor(.000001f, .00000008f, .0000005f), FLinearColor(0, 0, .00000001f),
		FLinearColor::Black, FLinearColor(.3f, .3f, .3f), FLinearColor::White,
		FLinearColor(std::numeric_limits<float>::quiet_NaN(), 0, 1),
		FLinearColor(1, std::numeric_limits<float>::infinity(), 0)
	};
	for (bool bCore : {false, true})
		for (const FLinearColor& Color : Colors)
		{
			const FVector Actual = BusterTravel::ResolveTint(Color, bCore);
			TestFalse(TEXT("Compensated tint is finite"), Actual.ContainsNaN());
			TestTrue(TEXT("Tint matches original emitter hue luminance without an extra HDR multiplier"),
				FMath::IsNearlyEqual(BusterTravel::Luminance(Actual), BusterTravel::Luminance(BusterTravel::ReferenceHue(bCore)), 0.00001));
			const double RGBPeak = FVector(Color).GetMax();
			const bool bFallback = !FMath::IsFinite(Color.R) || !FMath::IsFinite(Color.G) || !FMath::IsFinite(Color.B) || RGBPeak <= 0;
			const FVector ExpectedHue = bFallback ? FVector::OneVector : FVector(Color) / RGBPeak;
			TestTrue(TEXT("Normalization preserves chosen hue, including dark/neutral colors"), (Actual / Actual.GetMax()).Equals(ExpectedHue, 0.00001));
			TestTrue(TEXT("Intensity is an independent linear control"), BusterTravel::ResolveTint(Color, bCore, 2.f).Equals(Actual * 2, 0.00001));
		}
	TestTrue(TEXT("Blue spark compensation is HDR above one"), BusterTravel::ResolveTint(FLinearColor::Blue, false).Z > 1);
	TestTrue(TEXT("Very dark pink brightens without changing hue"),
		BusterTravel::ResolveTint(Colors[5], false).Equals(BusterTravel::ResolveTint(Colors[1], false), 0.00001));
	const FVector Core = BusterTravel::ResolveTint(Colors[1], true), Sparks = BusterTravel::ResolveTint(Colors[1], false);
	TestTrue(TEXT("Matching core and spark hues remain allowed"), (Core / Core.GetMax()).Equals(Sparks / Sparks.GetMax(), 0.00001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterTravelSourceTest, "Quasicombo.Buster.TravelSourcePreserved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterTravelSourceTest::RunTest(const FString&)
{
	auto* Source = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FXVarietyPack/Particles/P_ky_thunderBall.P_ky_thunderBall"));
	auto* Copy = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/Quasicombo/Buster/VFX/P_BusterThunderTravel.P_BusterThunderTravel"));
	if (!TestNotNull(TEXT("Original Thunder travel"), Source) || !TestNotNull(TEXT("Rebuilt travel"), Copy)) return false;
	TestEqual(TEXT("All source emitters retained"), Copy->Emitters.Num(), Source->Emitters.Num());
	for (int32 E = 0; E < FMath::Min(Source->Emitters.Num(), Copy->Emitters.Num()); ++E)
	{
		auto* S = Source->Emitters[E].Get();
		auto* C = Copy->Emitters[E].Get();
		if (!S || !C) { AddError(TEXT("Missing travel emitter")); continue; }
		TestEqual(TEXT("Emitter identity/order retained"), C->EmitterName, S->EmitterName);
		TestEqual(TEXT("LOD count retained"), C->LODLevels.Num(), S->LODLevels.Num());
		for (int32 L = 0; L < FMath::Min(S->LODLevels.Num(), C->LODLevels.Num()); ++L)
		{
			auto* SL = S->LODLevels[L].Get();
			auto* CL = C->LODLevels[L].Get();
			if (!SL || !CL || !SL->RequiredModule || !CL->RequiredModule) { AddError(TEXT("Missing travel LOD")); continue; }
			TestEqual(TEXT("Source loop count retained"), CL->RequiredModule->EmitterLoops, SL->RequiredModule->EmitterLoops);
			TestEqual(TEXT("Source duration retained"), CL->RequiredModule->EmitterDuration, SL->RequiredModule->EmitterDuration);
			TestEqual(TEXT("Source particle coordinate space retained"), bool(CL->RequiredModule->bUseLocalSpace), bool(SL->RequiredModule->bUseLocalSpace));
			TestEqual(TEXT("Only one extra tint module per LOD"), CL->Modules.Num(), SL->Modules.Num() + 1);
			for (int32 I = 0; I < FMath::Min(SL->Modules.Num(), CL->Modules.Num()); ++I)
			{
				auto* SM = SL->Modules[I].Get();
				auto* CM = CL->Modules[I].Get();
				if (!SM || !CM) { TestEqual(TEXT("Null module retained"), SM == nullptr, CM == nullptr); continue; }
				TestEqual(TEXT("Original module classes/order retained"), CM->GetClass(), SM->GetClass());
				for (int32 T = 0; T <= 16; ++T)
				{
					const float Time = T / 16.f;
					FRandomStream SR(119 + T), CR(119 + T);
					if (auto* Life = Cast<UParticleModuleLifetime>(SM))
						TestEqual(TEXT("Lifetime distribution retained"), CastChecked<UParticleModuleLifetime>(CM)->Lifetime.GetValue(Time, nullptr, &CR), Life->Lifetime.GetValue(Time, nullptr, &SR));
					if (auto* Size = Cast<UParticleModuleSize>(SM))
						TestTrue(TEXT("Size distribution retained"), CastChecked<UParticleModuleSize>(CM)->StartSize.GetValue(Time, nullptr, 0, &CR).Equals(Size->StartSize.GetValue(Time, nullptr, 0, &SR), .0001));
					if (auto* Color = Cast<UParticleModuleColorOverLife>(SM))
					{
						auto* CC = CastChecked<UParticleModuleColorOverLife>(CM);
						TestTrue(TEXT("Authored color brightness envelope retained"), CC->ColorOverLife.GetValue(Time).Equals(FVector(Color->ColorOverLife.GetValue(Time).GetMax()), .0001));
						TestEqual(TEXT("Authored alpha envelope retained"), CC->AlphaOverLife.GetValue(Time), Color->AlphaOverLife.GetValue(Time));
					}
					if (auto* Scale = Cast<UParticleModuleColorScaleOverLife>(SM))
					{
						const FVector Actual = CastChecked<UParticleModuleColorScaleOverLife>(CM)->ColorScaleOverLife.GetValue(Time);
						const FVector Expected(Scale->ColorScaleOverLife.GetValue(Time).GetMax());
						TestTrue(*FString::Printf(TEXT("%s LOD%d time%.3f HDR scale retained (copy %s / source %s)"), *S->EmitterName.ToString(), L, Time, *Actual.ToString(), *Expected.ToString()), Actual.Equals(Expected, .0001));
						TestEqual(TEXT("Authored alpha scale retained"), CastChecked<UParticleModuleColorScaleOverLife>(CM)->AlphaScaleOverLife.GetValue(Time), Scale->AlphaScaleOverLife.GetValue(Time));
					}
				}
			}
			if (SL->SpawnModule && CL->SpawnModule)
			{
				FRandomStream SR(77), CR(77);
				TestEqual(TEXT("Source spawn rate retained"), CL->SpawnModule->Rate.GetValue(0, nullptr, &CR), SL->SpawnModule->Rate.GetValue(0, nullptr, &SR));
				TestEqual(TEXT("Spawn bursts retained"), CL->SpawnModule->BurstList.Num(), SL->SpawnModule->BurstList.Num());
			}
			UMaterialInterface* Mat = CL->RequiredModule->Material;
			UMaterialInterface* SourceMat = SL->RequiredModule->Material;
			if (Mat && SourceMat)
			{
				TArray<FMaterialParameterInfo> Infos;
				TArray<FGuid> Guids;
				SourceMat->GetAllTextureParameterInfo(Infos, Guids);
				for (const FMaterialParameterInfo& Info : Infos)
				{
					UTexture* SourceTexture = nullptr;
					UTexture* CopyTexture = nullptr;
					TestTrue(TEXT("Source texture binding retained"), SourceMat->GetTextureParameterValue(Info, SourceTexture) && Mat->GetTextureParameterValue(Info, CopyTexture) && SourceTexture == CopyTexture);
				}
				Infos.Reset(); Guids.Reset();
				SourceMat->GetAllScalarParameterInfo(Infos, Guids);
				for (const FMaterialParameterInfo& Info : Infos)
				{
					float SourceValue = 0, CopyValue = 0;
					TestTrue(TEXT("Source noise/mask scalar retained"), SourceMat->GetScalarParameterValue(Info, SourceValue) && Mat->GetScalarParameterValue(Info, CopyValue) && SourceValue == CopyValue);
				}
			}
			TestTrue(TEXT("Travel source noise/texture parameters retained"), Mat && Mat->GetName() == (S->EmitterName == TEXT("core") ? TEXT("MI_BusterThunderTravelCore") : TEXT("MI_BusterThunderTravelSparks")));
			float Gain = 0;
			TestTrue(TEXT("Travel material gain restores full authored emission"), Mat && Mat->GetScalarParameterValue(FMaterialParameterInfo(TEXT("TravelEmissiveGain")), Gain) && Gain == 1.f);
#if WITH_EDITOR
			bool bPremultiply = false;
			FGuid Guid;
			TestTrue(TEXT("Travel instance selects correct premultiplication"), Mat && Mat->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("TravelPremultiply")), bPremultiply, Guid) && bPremultiply == (S->EmitterName == TEXT("core")));
#endif
		}
	}
#if WITH_EDITOR
	auto* Master = LoadObject<UMaterial>(nullptr, TEXT("/Game/Quasicombo/Buster/VFX/ThunderMaterials/M_BusterThunderTravel.M_BusterThunderTravel"));
	if (TestNotNull(TEXT("Dedicated travel master"), Master))
	{
		bool bPremultipliesBoundedOpacity = false;
		for (UMaterialExpression* Expression : Master->GetExpressionCollection().Expressions)
			if (auto* Switch = Cast<UMaterialExpressionStaticSwitchParameter>(Expression); Switch && Switch->ParameterName == TEXT("TravelPremultiply"))
				if (auto* Multiply = Cast<UMaterialExpressionMultiply>(Switch->A.Expression))
					if (auto* Clamp = Cast<UMaterialExpressionSaturate>(Multiply->B.Expression))
						bPremultipliesBoundedOpacity = Clamp->Input.Expression != nullptr && Switch->B.Expression == Multiply->A.Expression;
		TestTrue(TEXT("Core emission multiplies final masked and clamped opacity; sparks bypass premultiplication"), bPremultipliesBoundedOpacity);
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterTravelComparisonTest, "Quasicombo.Buster.TravelComparison", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterTravelComparisonTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* World = Fixture.GetTestWorld();
	auto* Player = MakeBusterPlayer(World);
	if (!TestNotNull(TEXT("Mike"), Player)) return false;
	Step(Fixture, .1f);
	auto* Cannon = Player->GetBusterCannon();
	Cannon->Equip();
	TestFalse(TEXT("Default travel is custom"), Cannon->UsesOriginalTravel());
	TestEqual(TEXT("Default travel diameter is 114 cm"), Cannon->TravelDiameterCm, 114.f);
	TestFalse(TEXT("Unsupported comparison diameter is rejected"), Cannon->PreviewTravelSize(0.f));
	ABusterProjectile* Bolts[3] = {};
	const bool Originals[] = {false, true, false};
	const float Sizes[] = {85.f, 57.f, 85.f};
	for (int32 I = 0; I < 3; ++I)
	{
		TestTrue(TEXT("Preview variant switches"), Cannon->PreviewTravel(Originals[I]));
		TestTrue(TEXT("Preview diameter switches"), Cannon->PreviewTravelSize(Sizes[I]));
		TestTrue(TEXT("Same fire controller accepts comparison shot"), Cannon->TryFire());
		Step(Fixture, .24f);
		for (TActorIterator<ABusterProjectile> It(World); It; ++It)
			if (*It != Bolts[0] && *It != Bolts[1]) Bolts[I] = *It;
		if (!TestNotNull(TEXT("New comparison bolt"), Bolts[I])) continue;
		auto* Visual = Bolts[I]->FindComponentByClass<UParticleSystemComponent>();
		if (!TestNotNull(TEXT("Comparison particles"), Visual)) continue;
		TestEqual(TEXT("Selected travel template"), Visual->Template->GetName(), FString(Originals[I] ? TEXT("P_ky_thunderBall") : TEXT("P_BusterThunderTravel")));
		TestTrue(TEXT("Selected diameter applied"), FMath::IsNearlyEqual(Visual->GetComponentScale().GetMax(), Sizes[I] / Cannon->ThunderVisualDiameterCm, .0001));
		TestEqual(TEXT("Visual size leaves collision unchanged"), Bolts[I]->CollisionRadius, 8.f);
		const FVector Before = Bolts[I]->GetActorLocation();
		Bolts[I]->Tick(.01f);
		TestTrue(TEXT("Both templates use identical normal travel speed"), FMath::IsNearlyEqual(FVector::Dist(Before, Bolts[I]->GetActorLocation()), Cannon->ProjectileSpeed * .01f, .001f));
		auto* Muzzle = FindActiveEffect(World, TEXT("P_BusterThunderMuzzle"));
		const float ExplosionScale = 37.642f * Cannon->CannonMesh->GetComponentScale().GetAbsMax() * Cannon->BusterProjectileDiameterMultiplier / Cannon->ThunderVisualDiameterCm;
		TestTrue(TEXT("Travel enlargement leaves muzzle scale unchanged"), Muzzle && FMath::IsNearlyEqual(Muzzle->GetComponentScale().GetMax(), ExplosionScale * Cannon->MuzzleEffectScaleMultiplier, .0001));
		FHitResult Hit;
		Hit.ImpactPoint = FVector(0, 300, 200);
		auto* Probe = World->SpawnActor<ABusterProjectile>(Hit.ImpactPoint, FRotator::ZeroRotator);
		Probe->Initialize(Player, FVector::ForwardVector, 2, Cannon->ProjectileSpeed, 2, FVector::ZeroVector,
			Sizes[I] / Cannon->ThunderVisualDiameterCm, ExplosionScale * Cannon->ImpactEffectScaleMultiplier, Originals[I]);
		Probe->ResolveImpact(Hit);
		auto* Impact = FindActiveEffect(World, TEXT("P_BusterThunderImpact"));
		TestTrue(TEXT("Travel enlargement leaves impact scale unchanged"), Impact && FMath::IsNearlyEqual(Impact->GetComponentScale().GetMax(), ExplosionScale * Cannon->ImpactEffectScaleMultiplier, .0001));
		Cannon->CancelFire();
		Step(Fixture, .18f);
	}
	auto* State = World->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>();
	State->SetMikeLightColor(FLinearColor::Blue);
	State->SetMikeBodyColor(FLinearColor::Blue);
	for (int32 I = 0; I < 3; ++I)
		if (Bolts[I])
		{
			auto* Visual = Bolts[I]->FindComponentByClass<UParticleSystemComponent>();
			TestTrue(TEXT("Existing shot keeps its own diameter after switching"), FMath::IsNearlyEqual(Visual->GetComponentScale().GetMax(), Sizes[I] / Cannon->ThunderVisualDiameterCm, .0001));
			if (Originals[I]) TestEqual(TEXT("Original travel ignores customization; retains source colors/materials"), Visual->InstanceParameters.Num(), 0);
			else
			{
				FVector Tint;
				TestTrue(TEXT("HDR vector survives transport and updates during flight"), Visual->GetVectorParameter(TEXT("BusterSparkColor"), Tint) && Tint.Z > 1 && Tint.Equals(BusterTravel::ResolveTint(FLinearColor::Blue, false), .0001));
			}
		}
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterInputTest, "Quasicombo.Buster.InputBindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterInputTest::RunTest(const FString&)
{
	auto* Context = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Input/IMC_SideScroller.IMC_SideScroller"));
	auto* Interact = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Input/Actions/IA_Interact.IA_Interact"));
	if (!TestNotNull(TEXT("Side-scroller mapping"), Context) || !TestNotNull(TEXT("Interact action"), Interact)) return false;
	bool bY = false, bE = false, bX = false, bF = false;
	for (const auto& Mapping : Context->GetMappings()) if (Mapping.Action == Interact)
	{
		bY |= Mapping.Key == EKeys::Gamepad_FaceButton_Top;
		bE |= Mapping.Key == EKeys::E;
		bX |= Mapping.Key == EKeys::Gamepad_FaceButton_Left;
		bF |= Mapping.Key == EKeys::F;
	}
	TestTrue(TEXT("Interact moved to gamepad Y"), bY);
	TestTrue(TEXT("Interact moved to keyboard E"), bE);
	TestFalse(TEXT("X reserved for Buster fire"), bX);
	TestFalse(TEXT("F reserved for Buster fire"), bF);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterHudTest, "Quasicombo.Buster.ProjectileUpdatesHUD", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterHudTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	UWorld* World = Fixture.GetTestWorld();
	auto* Player = MakeBusterPlayer(World);
	if (!TestNotNull(TEXT("Mike"), Player)) return false;
	UClass* ControllerClass = LoadClass<APlayerController>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingPlayerController.BP_SideScrollingPlayerController_C"));
	auto* PC = ControllerClass ? World->SpawnActor<APlayerController>(ControllerClass) : nullptr;
	if (!TestNotNull(TEXT("HUD owning controller"), PC)) return false;
	PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	PC->SetAsLocalPlayerController();
	PC->Possess(Player);
	UClass* HudClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/UBP_GameHUD.UBP_GameHUD_C"));
	if (!TestNotNull(TEXT("Game HUD class"), HudClass)) return false;
	UUserWidget* HUD = CreateWidget<UUserWidget>(PC, HudClass);
	if (!TestNotNull(TEXT("Game HUD instance"), HUD)) return false;
	TSharedRef<SWidget> RootWidget = HUD->TakeWidget(); // Keep Slate alive so Construct's combo binding remains active.
	const FObjectProperty* CountProperty = FindFProperty<FObjectProperty>(HudClass, TEXT("ComboCount"));
	if (!TestNotNull(TEXT("HUD combo text widget"), CountProperty)) return false;
	auto* CountText = Cast<UTextBlock>(CountProperty->GetObjectPropertyValue_InContainer(HUD));
	if (!TestNotNull(TEXT("HUD combo text"), CountText)) return false;
	const FString BeforeText = CountText->GetText().ToString();
	auto* Enemy = World->SpawnActor<ASideScrollingCombatEnemy>(FVector(400,0,145), FRotator::ZeroRotator);
	auto* Bolt = World->SpawnActor<ABusterProjectile>(FVector(300,0,145), FRotator::ZeroRotator);
	Bolt->Initialize(Player, FVector::ForwardVector, 2, 2500, 2);
	Bolt->ResolveImpact(FHitResult(Enemy, Enemy->GetCapsuleComponent(), Enemy->GetActorLocation(), -FVector::ForwardVector));
	TestEqual(TEXT("Accepted projectile increments Mike's combo"), Player->GetHitComboCount(), 1);
	Step(Fixture, 0.02f);
	const FString AfterText = CountText->GetText().ToString();
	TestTrue(TEXT("Accepted projectile refreshes the UMG combo"), AfterText.Contains(TEXT("1")) && AfterText != BeforeText);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterDamageTest, "Quasicombo.Buster.SingleHitAndBossEvolution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterDamageTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* World = Fixture.GetTestWorld();
	auto* Player = MakeBusterPlayer(World);
	if (!Player) return false;
	auto* Enemy = World->SpawnActor<ASideScrollingCombatEnemy>(FVector(400,0,145), FRotator::ZeroRotator);
	Enemy->SetActorTickEnabled(false);
	auto* Bolt = World->SpawnActor<ABusterProjectile>(FVector(300,0,145), FRotator::ZeroRotator);
	Bolt->Initialize(Player, FVector::ForwardVector, 2, 2500, 2);
	FHitResult Hit(Enemy, Enemy->GetCapsuleComponent(), Enemy->GetActorLocation(), -FVector::ForwardVector);
	Bolt->ResolveImpact(Hit);
	TestNotNull(TEXT("Thunder impact effect starts on collision"), FindActiveEffect(World, TEXT("P_BusterThunderImpact")));
	TestEqual(TEXT("One shot consumes one strike"), Enemy->GetAcceptedStrikes(), 1);
	TestEqual(TEXT("One impact awards one combo hit"), Player->GetHitComboCount(), 1);
	Bolt->ResolveImpact(Hit);
	TestEqual(TEXT("Duplicate impact ignored"), Enemy->GetAcceptedStrikes(), 1);
	const FLinearColor NextColor(0.05f, 0.9f, 1.0f);
	Player->GetBusterCannon()->SetMikeLightColor(NextColor);
	auto* Second = World->SpawnActor<ABusterProjectile>();
	Second->Initialize(Player, FVector::ForwardVector, 2, 2500, 2);
	Second->ResolveImpact(Hit);
	TestNotNull(TEXT("Impact effect uses current Mike light color"), FindEffectWithColor(World, TEXT("P_BusterThunderImpact"), TEXT("BusterCoreColor"), NextColor));
	TestEqual(TEXT("Second shot consumes one more strike"), Enemy->GetAcceptedStrikes(), 2);
	TestFalse(TEXT("Two shots do not defeat a three-hit enemy"), Enemy->IsCombatDefeated());
	auto *Boss = World->SpawnActor<AQuasicomboBoss>(FVector(1800, 0, 145), FRotator::ZeroRotator);
	Boss->Quantum->bUseLiveQuantumApi = false;
	Boss->Evolution->DurationScale = 0.001f;
	Boss->EvolutionMinimumSeconds = 0;
	Boss->EvolutionTimeoutSeconds = 0;
	Boss->FirstArmorThreshold = 2.0;
	Boss->SecondArmorThreshold = 3.0;
	Boss->StartEncounter(Player);
	for (int32 I=0; I<Boss->BaseHealthHits - Boss->EvolutionTriggerHealth; ++I)
	{
		const int32 Before = Boss->GetAcceptedStrikes();
		auto* Shot = World->SpawnActor<ABusterProjectile>();
		Shot->Initialize(Player, FVector::ForwardVector, 2,2500,2);
		Boss->ApplyDamage(2, Shot, Boss->GetActorLocation(), FVector::ZeroVector);
		TestEqual(TEXT("Boss accepts one strike per projectile"), Boss->GetAcceptedStrikes(), Before + 1);
		Shot->Destroy();
	}
	TestEqual(TEXT("One HP starts evolution"), Boss->GetBossPhase(), EQuasicomboBossPhase::Evolution);
	Step(Fixture,0.15f);
	TestEqual(TEXT("Boss resumes combat after evolution"), Boss->GetBossPhase(), EQuasicomboBossPhase::Combat);
	TestEqual(TEXT("Evolution refills boss health before more projectiles"),Boss->GetBaseHealthFraction(),1.0f);
	for (int32 I = 0; I < Boss->BaseHealthHits; ++I)
	{
		auto* FinalShot = World->SpawnActor<ABusterProjectile>();
		FinalShot->Initialize(Player, FVector::ForwardVector, 2,2500,2);
		Boss->ApplyDamage(2, FinalShot, Boss->GetActorLocation(), FVector::ZeroVector);
		FinalShot->Destroy();
	}
	TestEqual(TEXT("Depleting the full healed phase starts QTE without armor"), Boss->GetBossPhase(), EQuasicomboBossPhase::QTE);
	TestFalse(TEXT("Cannot fire during QTE"), Player->GetBusterCannon()->TryFire());
	Fixture.ForwardErrorMessages(this);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterSweepTest, "Quasicombo.Buster.SweptCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterSweepTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* World = Fixture.GetTestWorld();
	auto* Player = MakeBusterPlayer(World);
	if (!Player) return false;
	UClass* EnemyClass = LoadClass<ASideScrollingCombatEnemy>(nullptr,
		TEXT("/Game/Quasicombo/Enemies/BP_QC_Enemy.BP_QC_Enemy_C"));
	if (!TestNotNull(TEXT("Level enemy class"), EnemyClass)) return false;
	auto* Enemy = World->SpawnActor<ASideScrollingCombatEnemy>(EnemyClass, FVector(400,0,145), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Level enemy"), Enemy)) return false;
	Enemy->SetActorTickEnabled(false);
	Enemy->GetCharacterMovement()->DisableMovement();
	Enemy->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	auto* Bolt = World->SpawnActor<ABusterProjectile>(FVector(100,0,145), FRotator::ZeroRotator);
	Bolt->Initialize(Player, FVector::ForwardVector,2,2500,2,FVector::ZeroVector,5.0f,5.0f,true);
	Step(Fixture,0.2f);
	TestEqual(TEXT("Original travel uses the same swept hit and damage controller"), Enemy->GetAcceptedStrikes(), 1);
	TestEqual(TEXT("Shot causes the punch hit reaction"), Enemy->CombatState, ESideCombatState::HitReaction);
	auto* Wall = World->SpawnActor<AStaticMeshActor>(FVector(200,0,145), FRotator::ZeroRotator);
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Wall->SetActorScale3D(FVector(0.1f,3,3));
	Wall->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	auto* Blocked = World->SpawnActor<ABusterProjectile>(FVector(100,0,145), FRotator::ZeroRotator);
	Blocked->Initialize(Player,FVector::ForwardVector,2,2500,2,FVector::ZeroVector,0.01f,0.01f);
	Step(Fixture,0.2f);
	TestEqual(TEXT("Thin wall blocks fast shot"), Enemy->GetAcceptedStrikes(), 1);
	TestEqual(TEXT("Impacted bolts are destroyed"), BoltCount(World), 0);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterFreshSessionTest, "Quasicombo.Buster.FreshSessionLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterFreshSessionTest::RunTest(const FString&)
{
	// A new fixture creates a new game instance, as stopping/restarting PIE does.
	for (int32 Session = 0; Session < 2; ++Session)
	{
		FTestWorldWrapper Fixture;
		if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
		UWorld* World = Fixture.GetTestWorld();
		auto* State = World->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>();
		TestFalse(TEXT("Fresh session has no replay reward"), State->HasReplayCannon());
		// The wrapper does not create PIE worlds; exercise PIE pawn startup in its game world.
		World->WorldType = EWorldType::PIE;
		auto* Player = MakeBusterPlayer(World);
		World->WorldType = EWorldType::Game;
		if (!TestNotNull(TEXT("Fresh PIE Mike"), Player)) return false;
		auto* Cannon = Player->GetBusterCannon();
		TestFalse(TEXT("Fresh PIE pawn starts without the cannon"), Cannon->IsEquipped());
		TestFalse(TEXT("Fresh PIE cannon mesh is hidden"), Cannon->CannonMesh->IsVisible());
		TestFalse(TEXT("Fresh PIE pawn cannot fire the cannon"), Cannon->TryFire());
		if (Session == 0)
		{
			TestTrue(TEXT("Editor acquisition preview starts"), Cannon->PreviewAcquisition(
				FTransform(FRotator::ZeroRotator, Player->GetActorLocation() + FVector(150, 0, 30)), EBusterEquipAura::Tsunami));
			Cannon->CompleteAcquisition();
			TestFalse(TEXT("Preview does not grant a session replay reward"), State->HasReplayCannon());
			Player->Destroy();
			World->WorldType = EWorldType::PIE;
			Player = MakeBusterPlayer(World);
			World->WorldType = EWorldType::Game;
			if (!TestNotNull(TEXT("Mike after preview run"), Player)) return false;
			TestFalse(TEXT("Next pawn does not inherit the preview cannon"), Player->GetBusterCannon()->IsEquipped());
		}
		Fixture.ForwardErrorMessages(this);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBusterReplayTest, "Quasicombo.Buster.ReplayLoadout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBusterReplayTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* World = Fixture.GetTestWorld();
	auto* State = World->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>();
	auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	TestFalse(TEXT("New session has no cannon"), State->HasReplayCannon());
	TestFalse(TEXT("Replay cannot bypass victory"), State->ReplayWithBusterCannon(World));
	Run->HoldVictoryPresentation();
	Run->EndRun(EQuasicomboRunOutcome::Victory);
	TestFalse(TEXT("Cannot replay during cinematic"), State->ReplayWithBusterCannon(World));
	Run->ReleaseVictoryPresentation();
	TestTrue(TEXT("Replay requests map reload"), State->ReplayWithBusterCannon(World));
	TestFalse(TEXT("Duplicate travel is rejected"), State->ReplayWithBusterCannon(World));
	TestTrue(TEXT("Session carries cannon"), State->HasReplayCannon());
	const auto* Context = GEngine->GetWorldContextFromWorld(World);
	TestTrue(TEXT("Travel targets Lvl_Main"), Context && Context->TravelURL.Contains(TEXT("Lvl_Main")));
	auto* NewPlayer = MakeBusterPlayer(World);
	if (!TestNotNull(TEXT("Respawned Mike"), NewPlayer)) return false;
	TestTrue(TEXT("New pawn inherits replay cannon"), NewPlayer->GetBusterCannon()->IsEquipped());
	NewPlayer->Destroy();
	NewPlayer = MakeBusterPlayer(World);
	TestTrue(TEXT("Death replacement keeps session cannon"), NewPlayer && NewPlayer->GetBusterCannon()->IsEquipped());
	Fixture.ForwardErrorMessages(this);
	return true;
}
#endif
