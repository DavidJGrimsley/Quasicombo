#if WITH_DEV_AUTOMATION_TESTS
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboLoadoutSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "QuasicomboMusicComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWave.h"
#include "SideScrollingCharacter.h"
#include "QuasicomboQTEDirector.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboFrontendRunTest, "Quasicombo.Frontend.RunStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboFrontendRunTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld())
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
    Run->HoldForFrontend();
    UClass* PlayerClass = LoadClass<ASideScrollingCharacter>(nullptr,
        TEXT("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter_C"));
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = PlayerClass ? World->SpawnActor<ASideScrollingCharacter>(PlayerClass, FTransform::Identity, Spawn) : nullptr;
    auto* InactiveQTE = World->SpawnActor<AQuasicomboQTEDirector>();
    if (!TestNotNull(TEXT("Playable character Blueprint"), Player) || !TestNotNull(TEXT("QTE director"), InactiveQTE)) return false;
    Run->SetQTEDirector(InactiveQTE);
    Player->DoJumpStart();
    Player->DoDash();
    TestFalse(TEXT("An inactive QTE director cannot admit dash input during the frontend"), Player->IsDashing());
    for (int32 Step = 0; Step < 250; ++Step) World->Tick(LEVELTICK_All, 0.1f);
    TestEqual(TEXT("Reading and menu time are excluded"), Run->GetElapsedSeconds(), 0.0f);
    TestTrue(TEXT("Combat is locked before Start"), Run->IsBossCombatLocked());
    TestFalse(TEXT("Routes cannot commit before Start"), Run->TryCommitRoute(1, EQuasicomboLane::B));
    Run->RecordCombo(9);
    Run->RecordDamageTaken(50.0f);
    Run->EndRun(EQuasicomboRunOutcome::Defeat);
    TestEqual(TEXT("No menu combo"), Run->GetHighestCombo(), 0);
    TestEqual(TEXT("No menu damage"), Run->GetDamageTaken(), 0.0f);
    TestEqual(TEXT("A menu cannot end the run"), Run->GetOutcome(), EQuasicomboRunOutcome::Playing);
    TestTrue(TEXT("Start accepted once"), Run->StartRun());
    for (int32 Step = 0; Step < 20; ++Step) World->Tick(LEVELTICK_All, 0.1f);
    TestFalse(TEXT("Repeated Start is ignored"), Run->StartRun());
    TestTrue(FString::Printf(TEXT("Run clock starts at gameplay (elapsed %.6f)"), Run->GetElapsedSeconds()),
        FMath::IsNearlyEqual(Run->GetElapsedSeconds(), 2.0f, 0.001f));
    TestTrue(TEXT("Routes work after Start"), Run->TryCommitRoute(1, EQuasicomboLane::B));
    Run->HoldForFrontend();
    TestTrue(TEXT("An active route cannot be reset by frontend"), Run->HasRunStarted());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboFrontendAudioTest, "Quasicombo.Frontend.AudioPreferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboFrontendAudioTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) return false;
    auto* Loadout = Fixture.GetTestWorld()->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>();
    TestFalse(TEXT("New launch has not started"), Loadout->HasStartedSession());
    Loadout->SetUserAudioVolumes(-0.5f, 3.0f);
    TestEqual(TEXT("Music clamps to silence"), Loadout->GetUserMusicVolume(), 0.0f);
    TestEqual(TEXT("Ambience clamps to full mix"), Loadout->GetUserAmbienceVolume(), 1.0f);
    Loadout->MarkSessionStarted();
    TestTrue(TEXT("Retries remember session start"), Loadout->HasStartedSession());
    TestFalse(TEXT("Starting a session does not grant cannon"), Loadout->HasReplayCannon());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboFrontendMusicGainTest, "Quasicombo.Frontend.MusicMultiplier",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboFrontendMusicGainTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    auto* Loadout = World->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>();
    Loadout->SetUserAudioVolumes(0.35f, 0.6f);
    auto* Owner = World->SpawnActor<AActor>();
    auto* Music = NewObject<UQuasicomboMusicComponent>(Owner);
    Owner->AddInstanceComponent(Music); Music->RegisterComponent();
    auto* Track = NewObject<USoundWave>(Owner); Track->Duration = 90.0f;
    Music->DefaultMusic = Track; Music->CombatMusic = Track;
    Music->StartLevelMusic(0.8f); Music->AdvanceCrossfade(1.0f);
    TestTrue(TEXT("User music multiplies the level mix"), FMath::IsNearlyEqual(Music->GetOutputVolume(Track), 0.28f, 0.0001f));
    Music->SetApproachProgress(0.5f);
    TestTrue(TEXT("Approach attenuation composes with user music"), FMath::IsNearlyEqual(Music->GetOutputVolume(Track), 0.14f, 0.0001f));
    Music->SetEncounterState(EQuasicomboMusicState::Combat); Music->AdvanceCrossfade(1.0f);
    TestTrue(TEXT("Boss music retains its base mix"), FMath::IsNearlyEqual(Music->GetOutputVolume(Track), 0.245f, 0.0001f));
    Music->SetEncounterState(EQuasicomboMusicState::QTE); Music->AdvanceCrossfade(0.25f);
    TestTrue(TEXT("QTE ducking composes with user music"), FMath::IsNearlyEqual(Music->GetOutputVolume(Track), 0.098f, 0.0001f));
    const int32 Starts = Music->TrackStarts;
    Loadout->SetUserAudioVolumes(0.0f, 0.6f); Music->AdvanceCrossfade(0.0f);
    TestEqual(TEXT("User music can silence the mix"), Music->GetOutputVolume(Track), 0.0f);
    Loadout->SetUserAudioVolumes(0.8f, 0.6f); Music->AdvanceCrossfade(0.0f);
    TestTrue(TEXT("Restored user gain preserves QTE ducking"), FMath::IsNearlyEqual(Music->GetOutputVolume(Track), 0.224f, 0.0001f));
    TestEqual(TEXT("Volume changes do not restart playback"), Music->TrackStarts, Starts);
    Fixture.ForwardErrorMessages(this);
    return true;
}
#endif
