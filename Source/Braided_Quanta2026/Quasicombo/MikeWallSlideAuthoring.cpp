#if WITH_EDITOR
#include "MikeBusterAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_Slot.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
template<typename T> T* AddWallNode(UEdGraph* Graph, int32 X, int32 Y)
{
    auto* Node = NewObject<T>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->NodePosX = X; Node->NodePosY = Y;
    return Node;
}

void AuthorMikeWallSlide()
{
    auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Quasicombo/Mike/ABP_Mike_SideScroller.ABP_Mike_SideScroller"));
    auto* Sequence = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Quasicombo/Mike/AN_Mike_WallSlide.AN_Mike_WallSlide"));
    if (!BP || !Sequence) { UE_LOG(LogTemp, Error, TEXT("WALL_SLIDE_AUTHOR: assets missing")); return; }
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    UAnimGraphNode_Slot* FullBody = nullptr;
    for (auto* Graph : Graphs)
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node->NodeComment == TEXT("Quasicombo Wall Slide"))
            { UE_LOG(LogTemp, Display, TEXT("WALL_SLIDE_AUTHOR: already wired")); return; }
            if (auto* Slot = Cast<UAnimGraphNode_Slot>(Node); Slot && Slot->Node.SlotName == TEXT("DefaultSlot")) FullBody = Slot;
        }
    if (!FullBody || !FullBody->FindPin(TEXT("Source")) || FullBody->FindPin(TEXT("Source"))->LinkedTo.Num() != 1)
    { UE_LOG(LogTemp, Error, TEXT("WALL_SLIDE_AUTHOR: full-body source missing")); return; }
    auto* Graph = FullBody->GetGraph();
    auto* Input = FullBody->FindPin(TEXT("Source"));
    auto* Original = Input->LinkedTo[0];
    auto* Player = AddWallNode<UAnimGraphNode_SequencePlayer>(Graph, 500, 600);
    Player->Node.SetSequence(Sequence);
    Player->Node.SetLoopAnimation(true);
    Player->AllocateDefaultPins();
    auto* Blend = AddWallNode<UAnimGraphNode_BlendListByBool>(Graph, 850, 350);
    Blend->NodeComment = TEXT("Quasicombo Wall Slide");
    Blend->AllocateDefaultPins();
    auto* State = AddWallNode<UK2Node_VariableGet>(Graph, 500, 850);
    State->VariableReference.SetExternalMember(TEXT("bWallSliding"), UMikeBusterAnimInstance::StaticClass());
    State->AllocateDefaultPins();
    Input->BreakAllPinLinks();
    const auto Link = [Graph](UEdGraphPin* From, UEdGraphPin* To)
    { return From && To && Graph->GetSchema()->TryCreateConnection(From, To); };
    if (!Link(Original, Blend->FindPin(TEXT("BlendPose_1"))) ||
        !Link(Player->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("BlendPose_0"))) ||
        !Link(State->GetValuePin(), Blend->FindPin(TEXT("bActiveValue"))) || !Link(Blend->FindPin(TEXT("Pose")), Input))
    { UE_LOG(LogTemp, Error, TEXT("WALL_SLIDE_AUTHOR: wiring failed")); return; }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error) { UE_LOG(LogTemp, Error, TEXT("WALL_SLIDE_AUTHOR: compile failed")); return; }
    BP->MarkPackageDirty();
    const bool Saved = UPackage::SavePackage(BP->GetPackage(), BP,
        *FPackageName::LongPackageNameToFilename(BP->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension()), FSavePackageArgs());
    UE_LOG(LogTemp, Display, TEXT("WALL_SLIDE_AUTHOR: saved=%d"), Saved);
}
FAutoConsoleCommand WallSlideCommand(TEXT("Quasicombo.AuthorMikeWallSlide"), TEXT("Wire the authored wall slide ahead of Mike's full-body slot."), FConsoleCommandDelegate::CreateStatic(&AuthorMikeWallSlide));
}
#endif
