#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraParameterStore.h"
#include "SideScrollingCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboMikePlayerTest,
	"Quasicombo.Combat.MikePlayerAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboMikePlayerTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	AStaticMeshActor* Floor = FloorMesh ? World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Locomotion floor"), Floor)) return false;
	Floor->GetStaticMeshComponent()->SetStaticMesh(FloorMesh);
	Floor->SetActorScale3D(FVector(20.0f, 3.0f, 1.0f));
	Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
		TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
	ASideScrollingCharacter* Player = PlayerClass
		? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FVector(0.0f, 0.0f, 140.0f), FRotator::ZeroRotator)
		: nullptr;
	if (!TestNotNull(TEXT("Mike player Blueprint"), Player)) return false;
	if (!Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}

	USkeletalMeshComponent* Mesh = Player->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	TestTrue(TEXT("Mike mesh is active"), Mesh && Mesh->GetSkeletalMeshAsset() &&
		Mesh->GetSkeletalMeshAsset()->GetPathName().Contains(TEXT("/Game/RadicalMike/Mesh/SKM_MegaMikeZ")));
	if (!TestNotNull(TEXT("Mike Animation Blueprint instance"), Anim)) return false;
	TestTrue(TEXT("Mike Animation Blueprint is active"),
		Anim->GetClass()->GetPathName().Contains(TEXT("/Game/Quasicombo/Mike/ABP_Mike_SideScroller")));
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	UBlendSpace* Locomotion = LoadObject<UBlendSpace>(nullptr,
		TEXT("/Game/Quasicombo/Mike/BS_Mike_Locomotion.BS_Mike_Locomotion"));
	if (!TestNotNull(TEXT("Mike locomotion blend space"), Locomotion)) return false;
#if WITH_EDITOR
	TestTrue(TEXT("Locomotion samples have interpolation triangles"), Locomotion->GetBlendSpaceData().Triangles.Num() > 0);
#endif
	TArray<UNiagaraComponent*> JumpTrails;
	Player->GetComponents<UNiagaraComponent>(JumpTrails);
	TestEqual(TEXT("Mike has two foot trails"), JumpTrails.Num(), 2);
	for (UNiagaraComponent* Trail : JumpTrails)
	{
		TestTrue(TEXT("Foot trail uses the platforming Niagara system"), Trail->GetAsset() &&
			Trail->GetAsset()->GetPathName().Contains(TEXT("NS_Jump_Trail")));
		TArray<FNiagaraVariable> NiagaraParameters;
		if (Trail->GetAsset()) Trail->GetAsset()->GetExposedParameters().GetParameters(NiagaraParameters);
		TestTrue(TEXT("Foot trail exposes a LinearColor"), NiagaraParameters.ContainsByPredicate([](const FNiagaraVariable& Parameter)
		{
			return Parameter.GetName() == TEXT("User.Color") && Parameter.GetType().GetName() == TEXT("LinearColor");
		}));
	}
	const int32 LocomotionMachineIndex = Anim->GetStateMachineIndex(TEXT("Locomotion"));
	FQuat FirstThigh = FQuat::Identity;
	FQuat LaterThigh = FQuat::Identity;
	for (int32 Step = 0; Step < 20; ++Step)
	{
		Player->DoMove(1.0f);
		Fixture.TickTestWorld(0.05f);
		if (Step == 0) FirstThigh = Mesh->GetBoneQuaternion(TEXT("thigh_l"));
		if (Step == 9) LaterThigh = Mesh->GetBoneQuaternion(TEXT("thigh_l"));
	}
	TestEqual(TEXT("Mike enters walk/run state"), Anim->GetCurrentStateName(LocomotionMachineIndex), FName(TEXT("Walk / Run")));
	TestFalse(TEXT("Mike's leg animates while walking"), FirstThigh.Equals(LaterThigh, 0.01f));

	TArray<UAnimMontage*> ComboMontages;
	for (int32 Stage = 1; Stage <= 5; ++Stage)
	{
		const FString Path = FString::Printf(TEXT("/Game/Quasicombo/Mike/AM_Mike_Combo%d.AM_Mike_Combo%d"), Stage, Stage);
		UAnimMontage* ComboMontage = LoadObject<UAnimMontage>(nullptr, *Path);
		if (!TestNotNull(*FString::Printf(TEXT("Combo montage %d"), Stage), ComboMontage)) return false;
		ComboMontages.Add(ComboMontage);
	}
	UAnimMontage* Hold = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Quasicombo/Mike/AM_Mike_ChargeHold.AM_Mike_ChargeHold"));
	UAnimMontage* Release = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Quasicombo/Mike/AM_Mike_ChargeRelease.AM_Mike_ChargeRelease"));
	UAnimMontage* Dash = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Quasicombo/Mike/AM_Mike_Dash.AM_Mike_Dash"));
	UAnimMontage* Death = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Quasicombo/Mike/AM_Mike_Death.AM_Mike_Death"));
	if (!TestNotNull(TEXT("Hold montage"), Hold) ||
		!TestNotNull(TEXT("Release montage"), Release) || !TestNotNull(TEXT("Dash montage"), Dash) ||
		!TestNotNull(TEXT("Death montage"), Death)) return false;
	const TCHAR* ExpectedMoves[] = { TEXT("PunchR"), TEXT("UppercutL"), TEXT("PunchR"), TEXT("PunchL"), TEXT("UppercutR") };
	for (int32 Stage = 0; Stage < ComboMontages.Num(); ++Stage)
	{
		TestTrue(*FString::Printf(TEXT("Combo %d uses the requested move"), Stage + 1),
			ComboMontages[Stage]->GetFirstAnimReference() &&
			ComboMontages[Stage]->GetFirstAnimReference()->GetName().Contains(ExpectedMoves[Stage]));
	}
	TestTrue(TEXT("Charged release is a distinct claw clip"),
		Release->GetFirstAnimReference() && Release->GetFirstAnimReference()->GetName().Contains(TEXT("ChargedClaw")));

	Player->DoComboAttackStart();
	TestTrue(TEXT("Combo plays Mike's first montage"), Anim->Montage_IsPlaying(ComboMontages[0]));
	for (int32 Press = 0; Press < 4; ++Press) Player->DoComboAttackStart();
	TArray<UAnimMontage*> SeenMontages;
	SeenMontages.Add(ComboMontages[0]);
	for (int32 Step = 0; Step < 55; ++Step)
	{
		Fixture.TickTestWorld(0.05f);
		for (int32 Stage = 1; Stage < ComboMontages.Num(); ++Stage)
		{
			if (Anim->Montage_IsPlaying(ComboMontages[Stage]) && !SeenMontages.Contains(ComboMontages[Stage]))
			{
				SeenMontages.Add(ComboMontages[Stage]);
				const float ExpectedRate = Stage == 1 || Stage == 4 ? 2.0f : 2.4f;
				TestTrue(*FString::Printf(TEXT("Combo %d play rate"), Stage + 1),
					FMath::IsNearlyEqual(Anim->Montage_GetPlayRate(ComboMontages[Stage]), ExpectedRate, 0.01f));
			}
		}
	}
	TestEqual(TEXT("Five rapid taps play all five stages"), SeenMontages.Num(), 5);
	for (int32 Stage = 0; Stage < SeenMontages.Num(); ++Stage)
	{
		TestEqual(*FString::Printf(TEXT("Combo stage %d order"), Stage + 1), SeenMontages[Stage], ComboMontages[Stage]);
	}
	Player->DoComboAttackStart();
	TestTrue(TEXT("A new combo restarts at right punch"), Anim->Montage_IsPlaying(ComboMontages[0]));
	Player->PrepareForFinisher();
	Player->DoChargedAttackStart();
	TestTrue(TEXT("Charged hold plays Mike's montage"), Anim->Montage_IsPlaying(Hold));
	Player->DoChargedAttackEnd();
	TestTrue(TEXT("Charged release plays Mike's montage"), Anim->Montage_IsPlaying(Release));
	// Clear the action state before testing the independent dash timer.
	Player->PrepareForFinisher();

	Player->DoDash();
	TestTrue(TEXT("Dash starts gameplay movement"), Player->IsDashing());
	TestTrue(TEXT("Dash plays Mike's montage"), Anim->Montage_IsPlaying(Dash));
	for (int32 Step = 0; Step < 12; ++Step) Fixture.TickTestWorld(0.05f);
	TestTrue(TEXT("Dash still spans the old gap-jump duration"), Player->IsDashing());
	for (int32 Step = 0; Step < 10; ++Step) Fixture.TickTestWorld(0.05f);
	TestFalse(TEXT("Dash ends on gameplay timer"), Player->IsDashing());

	Player->TakeDamage(Player->GetCurrentHP(), FDamageEvent(), nullptr, nullptr);
	TestTrue(TEXT("Death plays Mike's montage first"), Anim->Montage_IsPlaying(Death));
	TestFalse(TEXT("Death does not ragdoll immediately"), Mesh->IsSimulatingPhysics());
	for (int32 Step = 0; Step < 19; ++Step) Fixture.TickTestWorld(0.05f);
	TestTrue(TEXT("Death enters ragdoll within one second"), Mesh->IsSimulatingPhysics());

	Fixture.ForwardErrorMessages(this);
	return !Fixture.HasFailed();
}

#endif
