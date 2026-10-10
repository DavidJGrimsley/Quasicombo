#if WITH_EDITOR
#include "MikeBusterAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

namespace
{
template <typename T> T* AddNode(UEdGraph* Graph, int32 X, int32 Y)
{
	T* Node = NewObject<T>(Graph);
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->NodePosX = X; Node->NodePosY = Y;
	return Node;
}
bool SaveBusterAsset(UObject* Asset)
{
	Asset->MarkPackageDirty();
	return UPackage::SavePackage(Asset->GetPackage(), Asset,
		*FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension()), FSavePackageArgs());
}
bool ConnectPose(UEdGraph* Graph, UEdGraphPin* Output, UEdGraphPin* Input)
{
	return Output && Input && Graph->GetSchema()->TryCreateConnection(Output, Input);
}
UAnimGraphNode_UseCachedPose* AddLocomotionUse(UEdGraph* Graph, UAnimGraphNode_SaveCachedPose* Save, int32 X, int32 Y)
{
	auto* Use = AddNode<UAnimGraphNode_UseCachedPose>(Graph, X, Y);
	Use->SaveCachedPoseNode = Save;
	Use->Node.CachePoseName = Save->Node.CachePoseName;
	Use->AllocateDefaultPins();
	return Use;
}
bool WireLocomotionIntoBuster(UEdGraph* Graph, UAnimGraphNode_LayeredBoneBlend* Blend, UAnimGraphNode_Slot* ArmSlot)
{
	auto* BaseInput = Blend->FindPin(TEXT("BasePose"));
	auto* ArmInput = ArmSlot->FindPin(TEXT("Source"));
	if (!BaseInput || !ArmInput) return false;
	if (ArmInput->LinkedTo.Num() == 1 &&
		Cast<UAnimGraphNode_UseCachedPose>(ArmInput->LinkedTo[0]->GetOwningNode())) return true;
	if (BaseInput->LinkedTo.Num() != 1) return false;
	UEdGraphPin* Locomotion = BaseInput->LinkedTo[0];
	BaseInput->BreakAllPinLinks();
	ArmInput->BreakAllPinLinks();
	auto* Save = AddNode<UAnimGraphNode_SaveCachedPose>(Graph, -1000, 250);
	Save->CacheName = TEXT("MikeLocomotionForBuster");
	Save->Node.CachePoseName = FName(*Save->CacheName);
	Save->AllocateDefaultPins();
	auto* BaseUse = AddLocomotionUse(Graph, Save, -550, 200);
	auto* ArmUse = AddLocomotionUse(Graph, Save, -550, 550);
	return ConnectPose(Graph, Locomotion, Save->FindPin(TEXT("Pose"))) &&
		ConnectPose(Graph, BaseUse->FindPin(TEXT("Pose")), BaseInput) &&
		ConnectPose(Graph, ArmUse->FindPin(TEXT("Pose")), ArmInput);
}

bool WireArmRecoil(UEdGraph* Graph, UAnimGraphNode_Slot* FullBody)
{
	if (!Graph || !FullBody) return false;
	for (UEdGraphNode* GraphNode : Graph->Nodes)
		if (const auto* Bone = Cast<UAnimGraphNode_ModifyBone>(GraphNode))
			if (Bone->Node.BoneToModify.BoneName == TEXT("clavicle_r")) return true;
	auto* FullInput = FullBody->FindPin(TEXT("Source"));
	if (!FullInput || FullInput->LinkedTo.Num() != 1) return false;
	UEdGraphPin* LayerOutput = FullInput->LinkedTo[0];
	auto* ToComponent = AddNode<UAnimGraphNode_LocalToComponentSpace>(Graph, 50, 300);
	ToComponent->AllocateDefaultPins();
	auto* Bone = AddNode<UAnimGraphNode_ModifyBone>(Graph, 280, 300);
	Bone->Node.BoneToModify.BoneName = TEXT("clavicle_r");
	Bone->Node.TranslationMode = BMM_Additive;
	Bone->Node.TranslationSpace = BCS_ComponentSpace;
	Bone->AllocateDefaultPins();
	auto* ToLocal = AddNode<UAnimGraphNode_ComponentToLocalSpace>(Graph, 540, 300);
	ToLocal->AllocateDefaultPins();
	auto* Offset = AddNode<UK2Node_VariableGet>(Graph, 100, 650);
	Offset->VariableReference.SetExternalMember(TEXT("BusterArmRecoilOffset"), UMikeBusterAnimInstance::StaticClass());
	Offset->AllocateDefaultPins();
	FullInput->BreakAllPinLinks();
	return ConnectPose(Graph, LayerOutput, ToComponent->FindPin(TEXT("LocalPose"))) &&
		ConnectPose(Graph, ToComponent->FindPin(TEXT("ComponentPose")), Bone->FindPin(TEXT("ComponentPose"))) &&
		ConnectPose(Graph, Offset->GetValuePin(), Bone->FindPin(TEXT("Translation"))) &&
		ConnectPose(Graph, Bone->FindPin(TEXT("Pose")), ToLocal->FindPin(TEXT("ComponentPose"))) &&
		ConnectPose(Graph, ToLocal->FindPin(TEXT("Pose")), FullInput);
}
void AuthorBusterLayer()
{
	auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Quasicombo/Mike/ABP_Mike_SideScroller.ABP_Mike_SideScroller"));
	if (!BP) { UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: missing Mike AnimBP")); return; }
	TArray<UEdGraph*> Graphs;
	BP->GetAllGraphs(Graphs);
	UAnimGraphNode_Slot* FullBody = nullptr;
    UAnimGraphNode_Slot* ExistingArm = nullptr;
	for (auto* Graph : Graphs)
		for (UEdGraphNode* Node : Graph->Nodes)
			if (auto* Slot = Cast<UAnimGraphNode_Slot>(Node))
			{
				if (Slot->Node.SlotName == TEXT("BusterArm")) ExistingArm = Slot;
				if (Slot->Node.SlotName == TEXT("DefaultSlot")) FullBody = Slot;
			}
    if (ExistingArm)
    {
        auto* Graph = ExistingArm->GetGraph();
        UAnimGraphNode_LayeredBoneBlend* Blend = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* Candidate = Cast<UAnimGraphNode_LayeredBoneBlend>(Node)) Blend = Candidate;
        if (!Blend) { UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: layer missing")); return; }
		if (!WireLocomotionIntoBuster(Graph, Blend, ExistingArm))
		{ UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: cannot route locomotion through arm slot")); return; }
		if (!WireArmRecoil(Graph, FullBody))
		{ UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: cannot add arm recoil")); return; }
        auto* WeightPin = Blend->FindPin(TEXT("BlendWeights_0"));
		if (WeightPin)
		{
			auto* Weight = WeightPin->LinkedTo.Num() == 1
				? Cast<UK2Node_VariableGet>(WeightPin->LinkedTo[0]->GetOwningNode()) : nullptr;
			if (!Weight || Weight->VariableReference.GetMemberName() != TEXT("BusterPoseAlpha"))
			{
				WeightPin->BreakAllPinLinks();
				Weight = AddNode<UK2Node_VariableGet>(Graph, -500, 800);
				Weight->VariableReference.SetExternalMember(TEXT("BusterPoseAlpha"), UMikeBusterAnimInstance::StaticClass());
				Weight->AllocateDefaultPins();
				if (!Graph->GetSchema()->TryCreateConnection(Weight->GetValuePin(), WeightPin))
				{ UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: cannot connect weight")); return; }
			}
		}
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error) { UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: repair compilation failed")); return; }
        UE_LOG(LogTemp, Display, TEXT("BUSTER_AUTHOR: repaired saved=%d"), SaveBusterAsset(BP));
        return;
    }
	if (!FullBody || !FullBody->FindPin(TEXT("Source")) || FullBody->FindPin(TEXT("Source"))->LinkedTo.Num() != 1)
	{ UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: expected one full-body source")); return; }
	BP->ParentClass = UMikeBusterAnimInstance::StaticClass();
	BP->TargetSkeleton->SetSlotGroupName(TEXT("BusterArm"), TEXT("BusterGroup"));
	SaveBusterAsset(BP->TargetSkeleton);
	UEdGraph* Graph = FullBody->GetGraph();
	UEdGraphPin* BasePose = FullBody->FindPin(TEXT("Source"))->LinkedTo[0];
	auto* Slot = AddNode<UAnimGraphNode_Slot>(Graph, -500, 500);
	Slot->Node.SlotName = TEXT("BusterArm");
	Slot->AllocateDefaultPins();
	auto* Blend = AddNode<UAnimGraphNode_LayeredBoneBlend>(Graph, -200, 300);
	if (Blend->Node.BlendPoses.IsEmpty()) Blend->Node.AddPose();
	FBranchFilter Filter;
	Filter.BoneName = TEXT("clavicle_r");
	Filter.BlendDepth = 0;
	Blend->Node.LayerSetup[0].BranchFilters = { Filter };
	Blend->Node.bMeshSpaceRotationBlend = true;
	Blend->AllocateDefaultPins();
	auto* Weight = AddNode<UK2Node_VariableGet>(Graph, -500, 800);
	Weight->VariableReference.SetExternalMember(TEXT("BusterPoseAlpha"), UMikeBusterAnimInstance::StaticClass());
	Weight->AllocateDefaultPins();
	const auto Link = [Graph](UEdGraphPin* A, UEdGraphPin* B)
	{
		if (!A || !B || !Graph->GetSchema()->TryCreateConnection(A, B))
			UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: failed graph connection"));
	};
	FullBody->FindPin(TEXT("Source"))->BreakAllPinLinks();
	Link(BasePose, Blend->FindPin(TEXT("BasePose")));
	if (!WireLocomotionIntoBuster(Graph, Blend, Slot))
	{ UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: cannot cache locomotion pose")); return; }
	Link(Slot->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("BlendPoses_0")));
	Link(Weight->GetValuePin(), Blend->FindPin(TEXT("BlendWeights_0")));
	Link(Blend->FindPin(TEXT("Pose")), FullBody->FindPin(TEXT("Source")));
	if (!WireArmRecoil(Graph, FullBody))
	{ UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: cannot add arm recoil")); return; }
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) { UE_LOG(LogTemp, Error, TEXT("BUSTER_AUTHOR: compilation failed")); return; }
	UE_LOG(LogTemp, Display, TEXT("BUSTER_AUTHOR: saved=%d"), SaveBusterAsset(BP));
}
FAutoConsoleCommand Command(TEXT("Quasicombo.AuthorBusterLayer"), TEXT("Add Mike's Buster arm layer once."), FConsoleCommandDelegate::CreateStatic(&AuthorBusterLayer));
}
#endif
