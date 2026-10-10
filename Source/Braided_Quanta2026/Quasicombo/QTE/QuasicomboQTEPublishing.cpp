#if WITH_EDITOR
#include "QuasicomboQTEPresentation.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimSequence.h"

namespace
{
void PublishQTE()
{
    if(auto* Python=IPythonScriptPlugin::Get())
    {
        const FString Script=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Project/BakeQTECinematics.py"));
        if(!Python->ExecPythonCommand(*FString::Printf(TEXT("\"%s\""),*Script)))
            UE_LOG(LogTemp,Error,TEXT("QTE publish failed; see the Python output log. Gameplay data was not published."));
    }
    else UE_LOG(LogTemp,Error,TEXT("QTE publishing requires the project's editor Python plugin."));
}
FAutoConsoleCommand PublishCommand(TEXT("Quasicombo.PublishQTEChoreography"),TEXT("Publish Sequencer QTE sources, paths, markers and cameras."),FConsoleCommandDelegate::CreateStatic(&PublishQTE));
}
void UQuasicomboQTEPresentation::PublishChoreography() { PublishQTE(); }
UAnimSequence* UQuasicomboQTEPresentation::CreateStagedClip(UAnimSequence* Template)
{
    if(!Template) return nullptr;
    auto* Clip=DuplicateObject<UAnimSequence>(Template,GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UAnimSequence::StaticClass(),TEXT("QTEPublish")));
    Clip->ClearFlags(RF_Public|RF_Standalone); Clip->SetFlags(RF_Transient);
    return Clip;
}

// Registration is installed by the game module after the editor's menus are ready.
void RegisterQuasicomboQTEMenu()
{
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]
    {
        auto* Menu=UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
        auto& Section=Menu->FindOrAddSection(TEXT("Quasicombo"),NSLOCTEXT("QTE","Menu","Quasicombo"));
        Section.AddSubMenu(TEXT("QuasicomboTools"),NSLOCTEXT("QTE","Tools","Quasicombo"),
            NSLOCTEXT("QTE","ToolsHelp","Quasicombo authoring tools"),FNewToolMenuDelegate::CreateLambda([](UToolMenu* Submenu)
            {
                Submenu->AddSection(TEXT("Choreography")).AddMenuEntry(TEXT("PublishQTEChoreography"),NSLOCTEXT("QTE","Publish","Publish QTE Choreography"),
                    NSLOCTEXT("QTE","PublishHelp","Bake the retained Sequencer sources and publish paths, cameras and timing markers."),
                    FSlateIcon(),FUIAction(FExecuteAction::CreateStatic(&PublishQTE)));
            }));
    }));
}
#endif
