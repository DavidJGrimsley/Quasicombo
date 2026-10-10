#if WITH_DEV_AUTOMATION_TESTS
#include "QuasicomboBossApproachVolume.h"
#include "QuasicomboBoss.h"
#include "QuasicomboEncounterLightingComponent.h"
#include "QuasicomboMusicComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "QuantumBossComponent.h"
#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Sound/SoundWave.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApproachProjectionTest, "Quasicombo.Approach.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApproachProjectionTest::RunTest(const FString&)
{
	float Progress = -1;
	const FVector Start(0, 100, 0), End(1000, 100, 500);
	for (const float Alpha : {-0.5f, 0.0f, 0.5f, 1.0f, 1.5f})
	{
		const FVector Position = FMath::Lerp(Start, End, Alpha) + FVector(0, 15000, 0);
		TestTrue(TEXT("Valid diagonal"), AQuasicomboBossApproachVolume::ProjectProgress(Position, Start, End, Progress));
		TestTrue(TEXT("Projection uses X/Z and ignores depth"), FMath::IsNearlyEqual(Progress, Alpha));
	}
	const FTransform Rotated(FRotator(32, 0, 0), FVector(900, 1000, 200), FVector(2, 1, 3));
	const FVector A = Rotated.TransformPosition(Start), B = Rotated.TransformPosition(End);
	TestTrue(TEXT("Rotated and scaled endpoints"), AQuasicomboBossApproachVolume::ProjectProgress((A+B)*0.5, A, B, Progress));
	TestTrue(TEXT("Rotated midpoint"), FMath::IsNearlyEqual(Progress, 0.5f));
	TestFalse(TEXT("Coincident endpoints are rejected"), AQuasicomboBossApproachVolume::ProjectProgress(Start, Start, Start, Progress));
	TestFalse(TEXT("Depth-only endpoints are rejected"), AQuasicomboBossApproachVolume::ProjectProgress(Start, Start, Start+FVector(0,100,0), Progress));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApproachOwnershipTest, "Quasicombo.Approach.LightingOwnershipAndExits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApproachOwnershipTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	UWorld* World = Fixture.GetTestWorld();
	auto* Sun = World->SpawnActor<ADirectionalLight>()->GetLightComponent();
	Sun->SetMobility(EComponentMobility::Movable); Sun->SetIntensity(20);
	auto* Boss = World->SpawnActor<AQuasicomboBoss>();
	Boss->Quantum->bUseLiveQuantumApi = false;
	auto* Volume = World->SpawnActor<AQuasicomboBossApproachVolume>();
	Volume->Boss = Boss; Volume->StartPoint = FVector(0,0,0); Volume->EndPoint = FVector(1000,0,500);
	Volume->ApplyPlayerLocation(FVector(500,2000,250));
	TestEqual(TEXT("Approach midpoint is twelve lux"), Sun->Intensity, 12.0f);
	Volume->ApplyPlayerLocation(FVector(250,0,125));
	TestEqual(TEXT("Backtracking restores brightness"), Sun->Intensity, 16.0f);
	Volume->FinishOverlap(FVector(300,10000,150));
	TestEqual(TEXT("Side exit holds last intensity"), Sun->Intensity, 16.0f);
	Volume->FinishOverlap(FVector(-100,0,-50));
	TestEqual(TEXT("Bottom exit restores twenty"), Sun->Intensity, 20.0f);
	Volume->FinishOverlap(FVector(1100,0,550));
	TestEqual(TEXT("Arena exit holds four"), Sun->Intensity, 4.0f);
	Boss->EncounterLighting->CombatFadeSeconds = Boss->EncounterLighting->EvolutionFadeSeconds = 0;
	Boss->EncounterLighting->BeginCombat();
	TestFalse(TEXT("Combat rejects approach writes"), Volume->ApplyPlayerLocation(FVector::ZeroVector));
	TestEqual(TEXT("No jump at handover"), Sun->Intensity, 4.0f);
	Boss->EncounterLighting->BeginEvolution();
	Volume->FinishOverlap(FVector::ZeroVector);
	TestEqual(TEXT("Overlap exit cannot brighten evolution"), Sun->Intensity, 0.2f);
	Boss->EncounterLighting->EndEvolution();
	TestEqual(TEXT("Completed cinematic returns sunlight to one"), Sun->Intensity, 1.0f);
	Boss->EncounterLighting->BeginCombat();
	TestEqual(TEXT("Retry retains post-evolution light"), Sun->Intensity, 1.0f);
	Boss->EncounterLighting->Restore();
	TestEqual(TEXT("Restore uses pre-approach baseline"), Sun->Intensity, 20.0f);
	Volume->StartPoint = Volume->EndPoint;
	AddExpectedError(TEXT("invalid or coincident approach endpoints"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Bad endpoints skip effects"), Volume->ApplyPlayerLocation(FVector::ZeroVector));
	TestEqual(TEXT("Bad endpoints do not change sunlight"), Sun->Intensity, 20.0f);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApproachInitialOverlapTest, "Quasicombo.Approach.InitialOverlapAndPlayerFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FApproachInitialOverlapTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* World = Fixture.GetTestWorld();
	auto* Sun = World->SpawnActor<ADirectionalLight>()->GetLightComponent();
	Sun->SetMobility(EComponentMobility::Movable); Sun->SetIntensity(20);
	auto* Boss = World->SpawnActor<AQuasicomboBoss>(FVector(20000,0,0), FRotator::ZeroRotator); Boss->Quantum->bUseLiveQuantumApi = false;
	auto* VolumeClass = LoadClass<AQuasicomboBossApproachVolume>(nullptr, TEXT("/Game/Quasicombo/Boss/BP_BossApproachVolume.BP_BossApproachVolume_C"));
	if (!TestNotNull(TEXT("Reusable approach Blueprint is available"), VolumeClass)) return false;
	auto* Volume = World->SpawnActor<AQuasicomboBossApproachVolume>(VolumeClass);
	TestEqual(TEXT("Fresh Blueprint instance finds the nearby boss"), Volume->Boss.Get(), Boss);
	TestEqual(TEXT("Fresh Blueprint instance inherits four lux"), Volume->EndIntensity, 4.0f);
	Volume->StartPoint = FVector(-1000,0,-500); Volume->EndPoint = FVector(1000,0,500);
	auto* Player = World->SpawnActor<ACharacter>();
	auto* Controller = World->SpawnActor<APlayerController>(); Controller->Possess(Player);
	Player->GetRootComponent()->UpdateComponentToWorld(); Volume->Trigger->UpdateOverlaps();
	Fixture.TickTestWorld(0.01f);
	TestTrue(TEXT("Player spawned inside is tracked"), Volume->bPlayerInside);
	TestTrue(TEXT("Initial overlap evaluates midpoint"), FMath::IsNearlyEqual(Sun->Intensity, 12.0f, 0.1f));
	Player->SetActorLocation(FVector(-3000,0,-1500)); Fixture.TickTestWorld(0.01f);
	auto* Enemy = World->SpawnActor<ACharacter>(); Volume->Trigger->UpdateOverlaps(); Fixture.TickTestWorld(0.01f);
	TestFalse(TEXT("Other pawns do not control approach"), Volume->bPlayerInside);
	TestEqual(TEXT("Enemy leaves restored light alone"), Sun->Intensity, 20.0f);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMusicTransitionsTest, "Quasicombo.Music.RouteApproachAndEncounter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMusicTransitionsTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* Owner = Fixture.GetTestWorld()->SpawnActor<AActor>();
	auto* Music = NewObject<UQuasicomboMusicComponent>(Owner); Owner->AddInstanceComponent(Music); Music->RegisterComponent();
	auto* Base = NewObject<USoundWave>(Owner); auto* A = NewObject<USoundWave>(Owner);
	auto* C = NewObject<USoundWave>(Owner); auto* Boss = NewObject<USoundWave>(Owner); auto* Evolution = NewObject<USoundWave>(Owner);
	Music->DefaultMusic = Base; Music->RouteAMusic = A; Music->RouteCMusic = C;
	Music->CombatMusic = Boss; Music->EvolutionMusic = Evolution;
	Music->StartLevelMusic(); Music->AdvanceCrossfade(1);
	TestEqual(TEXT("Default piano"), Music->CurrentMusic.Get(), static_cast<USoundBase*>(Base));
	auto* Run = Fixture.GetTestWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	Run->OnRouteCommitted.Broadcast(1, EQuasicomboLane::A);
	Music->AdvanceCrossfade(0.5f);
	TestTrue(TEXT("Halfway crossfade keeps both tracks"), Music->GetOutputVolume(A) > 0 && Music->GetOutputVolume(Base) > 0);
	Run->OnRouteCommitted.Broadcast(2, EQuasicomboLane::C);
	TestTrue(TEXT("Rapid changes retain fading voices"), Music->GetVoiceCount() >= 3);
	Music->AdvanceCrossfade(1);
	TestEqual(TEXT("Old voices stop after the fade"), Music->GetVoiceCount(), 1);
	Music->SetApproachProgress(0.5f); TestEqual(TEXT("Halfway approach music"), Music->GetOutputVolume(C), 0.5f);
	Music->ApproachMusicGain = 0.6f; Music->SetApproachProgress(0.5f);
	TestEqual(TEXT("Editable approach gain scales the route track"), Music->GetOutputVolume(C), 0.3f);
	Music->ApproachMusicGain = 1.0f;
	Music->SetApproachProgress(0); TestEqual(TEXT("Backtracking restores music"), Music->GetOutputVolume(C), 1.0f);
	Music->SetApproachProgress(1); TestEqual(TEXT("Top of stairs is silent"), Music->GetOutputVolume(C), 0.0f);
	Music->SetEncounterState(EQuasicomboMusicState::Combat);
	TestEqual(TEXT("Combat cannot unmute outgoing route music"), Music->GetOutputVolume(C), 0.0f);
	Music->AdvanceCrossfade(1);
	TestEqual(TEXT("Combat uses boss track"), Music->CurrentMusic.Get(), static_cast<USoundBase*>(Boss));
	const int32 Starts = Music->TrackStarts;
	Music->SetEncounterState(EQuasicomboMusicState::Combat);
	TestEqual(TEXT("Repeated state does not restart track"), Music->TrackStarts, Starts);
	Music->SetEncounterState(EQuasicomboMusicState::Evolution, C); Music->AdvanceCrossfade(1);
	TestEqual(TEXT("Cinematic track wins over quantum override"), Music->CurrentMusic.Get(), static_cast<USoundBase*>(Evolution));
	TestEqual(TEXT("Transformation music uses half encounter volume"), Music->GetOutputVolume(Evolution), 0.35f);
	Music->SetEncounterState(EQuasicomboMusicState::Evolution, A);
	TestEqual(TEXT("Quantum update cannot restart evolution track"), Music->TrackStarts, Starts + 1);
	Music->SetEncounterState(EQuasicomboMusicState::Combat); Music->AdvanceCrossfade(1);
	TestEqual(TEXT("Transformation completion returns boss track"), Music->CurrentMusic.Get(), static_cast<USoundBase*>(Boss));
	TestEqual(TEXT("Boss music returns to full encounter volume"), Music->GetOutputVolume(Boss), 0.7f);
	Music->SetEncounterState(EQuasicomboMusicState::QTE); Music->SetEncounterState(EQuasicomboMusicState::Retry);
	Run->OnRouteCommitted.Broadcast(3,EQuasicomboLane::B); Music->SetApproachProgress(1);
	TestEqual(TEXT("Routes and approach cannot overwrite encounter"), Music->CurrentMusic.Get(), static_cast<USoundBase*>(Boss));
	TestEqual(TEXT("Encounter ignores approach gain"), Music->ApproachGain, 1.0f);
	Music->StopMusic(0.8f); Music->AdvanceCrossfade(0.4f);
	TestTrue(TEXT("Defeat fades rather than stops abruptly"), Music->GetOutputVolume(Boss) > 0 && Music->GetOutputVolume(Boss) < 0.7f);
	Music->AdvanceCrossfade(0.4f); Music->SetEncounterState(EQuasicomboMusicState::Combat);
	TestEqual(TEXT("Victory stays silent"), Music->GetVoiceCount(), 0);
	// An optional boss override may be the same asset as the current route track.
	auto* SharedTrackMusic = NewObject<UQuasicomboMusicComponent>(Owner);
	Owner->AddInstanceComponent(SharedTrackMusic); SharedTrackMusic->RegisterComponent();
	SharedTrackMusic->DefaultMusic = SharedTrackMusic->CombatMusic = Base;
	SharedTrackMusic->StartLevelMusic(0.8f); SharedTrackMusic->AdvanceCrossfade(1);
	SharedTrackMusic->SetApproachProgress(1);
	const int32 SharedStarts = SharedTrackMusic->TrackStarts;
	SharedTrackMusic->SetEncounterState(EQuasicomboMusicState::Combat);
	TestEqual(TEXT("Shared override remains silent at handover"), SharedTrackMusic->GetOutputVolume(Base), 0.0f);
	SharedTrackMusic->AdvanceCrossfade(1);
	TestEqual(TEXT("Shared override fades up to encounter volume"), SharedTrackMusic->GetOutputVolume(Base), 0.7f);
	TestEqual(TEXT("Shared override does not restart playback"), SharedTrackMusic->TrackStarts, SharedStarts);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMusicPlaybackContinuityTest, "Quasicombo.Music.RoutePlaybackContinuity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMusicPlaybackContinuityTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* World = Fixture.GetTestWorld();
	auto* Owner = World->SpawnActor<AActor>();
	auto* Music = NewObject<UQuasicomboMusicComponent>(Owner);
	Owner->AddInstanceComponent(Music); Music->RegisterComponent();
	auto* Base = NewObject<USoundWave>(Owner); Base->Duration = 120.0f;
	auto* A = NewObject<USoundWave>(Owner); A->Duration = 60.0f;
	auto* C = NewObject<USoundWave>(Owner); C->Duration = 80.0f;
	auto* Boss = NewObject<USoundWave>(Owner); Boss->Duration = 90.0f;
	auto* Evolution = NewObject<USoundWave>(Owner); Evolution->Duration = 90.0f;
	Music->DefaultMusic = Base; Music->RouteAMusic = A; Music->RouteCMusic = C;
	Music->CombatMusic = Boss; Music->EvolutionMusic = Evolution;
	auto AudioFor = [Owner](USoundBase* Sound) -> UAudioComponent* {
		TArray<UAudioComponent*> Components; Owner->GetComponents(Components);
		for (auto* Audio : Components) if (Audio->Sound == Sound) return Audio;
		return nullptr;
	};
	auto ReportPosition = [&AudioFor](USoundWave* Wave, float Seconds) {
		if (auto* Audio = AudioFor(Wave)) Audio->OnAudioPlaybackPercentNative.Broadcast(Audio, Wave, Seconds / Wave->Duration);
	};
	auto* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	Music->StartLevelMusic(); Music->AdvanceCrossfade(1.0f);
	ReportPosition(Base, 42.0f);
	Run->OnRouteCommitted.Broadcast(1, EQuasicomboLane::A);
	TestEqual(TEXT("B to A seeks to outgoing timestamp"), Music->LastTrackStartSeconds, 42.0f);
	TestEqual(TEXT("Incoming playhead is immediately visible"), Music->GetPlaybackSeconds(), 42.0f);
	ReportPosition(A, 0.0f);
	TestEqual(TEXT("Pre-seek zero callback cannot reset the timestamp"), Music->GetPlaybackSeconds(), 42.0f);
	ReportPosition(A, 42.5f);
	Music->AdvanceCrossfade(0.5f);
	Run->OnRouteCommitted.Broadcast(2, EQuasicomboLane::C);
	TestEqual(TEXT("Rapid A to C switch preserves timestamp"), Music->LastTrackStartSeconds, 42.5f);
	ReportPosition(C, 43.0f); Music->AdvanceCrossfade(1.0f);
	Run->OnRouteCommitted.Broadcast(3, EQuasicomboLane::B);
	TestEqual(TEXT("Returning to a destroyed route voice seeks rather than starts over"), Music->LastTrackStartSeconds, 43.0f);
	const int32 Starts = Music->TrackStarts;
	Run->OnRouteCommitted.Broadcast(4, EQuasicomboLane::B);
	TestEqual(TEXT("Same route never restarts"), Music->TrackStarts, Starts);
	ReportPosition(Base, 113.5f);
	Run->OnRouteCommitted.Broadcast(5, EQuasicomboLane::A);
	TestEqual(TEXT("Shorter incoming file wraps outgoing timestamp"), Music->LastTrackStartSeconds, 53.5f);
	ReportPosition(A, 61.25f);
	TestTrue(TEXT("Audio callback wraps natural loops"), FMath::IsNearlyEqual(Music->GetPlaybackSeconds(), 1.25f, 0.001f));
	Run->OnRouteCommitted.Broadcast(6, EQuasicomboLane::C);
	TestTrue(TEXT("Switch after loop uses current song position"), FMath::IsNearlyEqual(Music->LastTrackStartSeconds, 1.25f, 0.001f));
	TestEqual(TEXT("Silent route retains its playback voice"), C->VirtualizationMode, EVirtualizationMode::PlayWhenSilent);
	Music->SetApproachProgress(1.0f); ReportPosition(C, 3.5f);
	Music->SetApproachProgress(0.0f);
	TestTrue(TEXT("Stair silence and backtracking preserve playback"), FMath::IsNearlyEqual(Music->GetPlaybackSeconds(), 3.5f));
	Music->SetEncounterState(EQuasicomboMusicState::Combat);
	TestEqual(TEXT("Boss music still starts from the beginning"), Music->LastTrackStartSeconds, 0.0f);
	ReportPosition(Boss, 25.0f);
	Music->SetEncounterState(EQuasicomboMusicState::Evolution);
	TestEqual(TEXT("Transformation music intentionally starts from the beginning"), Music->LastTrackStartSeconds, 0.0f);
	TestEqual(TEXT("Invalid positions fall back safely"), UQuasicomboMusicComponent::WrapPlaybackSeconds(-1.0, A), 0.0f);
	Fixture.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMusicQTEDuckingTest, "Quasicombo.Music.QTEDuckingPreservesHitsAndPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMusicQTEDuckingTest::RunTest(const FString&)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
	auto* Owner = Fixture.GetTestWorld()->SpawnActor<AActor>();
	auto* Music = NewObject<UQuasicomboMusicComponent>(Owner);
	Owner->AddInstanceComponent(Music); Music->RegisterComponent();
	auto* BossTrack = NewObject<USoundWave>(Owner); BossTrack->Duration = 90.0f;
	Music->CombatMusic = BossTrack;
	auto* Punch = NewObject<UAudioComponent>(Owner);
	Owner->AddInstanceComponent(Punch); Punch->RegisterComponent(); Punch->SetVolumeMultiplier(0.85f);
	Music->SetEncounterState(EQuasicomboMusicState::Combat); Music->AdvanceCrossfade(1.0f);
	const int32 Starts = Music->TrackStarts;
	TArray<UAudioComponent*> Audio; Owner->GetComponents(Audio);
	for (auto* Component : Audio) if (Component->Sound == BossTrack)
		Component->OnAudioPlaybackPercentNative.Broadcast(Component, BossTrack, 12.0f / BossTrack->Duration);
	Music->SetEncounterState(EQuasicomboMusicState::QTE);
	TestEqual(TEXT("QTE starts a smooth fade from combat volume"), Music->GetOutputVolume(BossTrack), 0.7f);
	Music->AdvanceCrossfade(0.125f);
	Music->SetEncounterState(EQuasicomboMusicState::QTE);
	Music->AdvanceCrossfade(0.125f);
	TestTrue(TEXT("QTE reduces boss music to forty percent"), FMath::IsNearlyEqual(Music->GetOutputVolume(BossTrack), 0.28f, 0.0001f));
	TestEqual(TEXT("Repeated QTE updates do not restart duck fade or song"), Music->TrackStarts, Starts);
	TestTrue(TEXT("Music keeps its playhead"), FMath::IsNearlyEqual(Music->GetPlaybackSeconds(), 12.0f, 0.001f));
	TestEqual(TEXT("Punch effects keep their own volume"), Punch->VolumeMultiplier, 0.85f);
	Music->SetEncounterState(EQuasicomboMusicState::Combat); Music->AdvanceCrossfade(0.25f);
	TestEqual(TEXT("Nonfinal counter returns normal boss music"), Music->GetOutputVolume(BossTrack), 0.7f);
	Music->SetEncounterState(EQuasicomboMusicState::QTE); Music->AdvanceCrossfade(0.25f);
	Music->SetEncounterState(EQuasicomboMusicState::Retry); Music->AdvanceCrossfade(0.25f);
	TestEqual(TEXT("Interruption restores music"), Music->GetOutputVolume(BossTrack), 0.7f);
	Music->SetEncounterState(EQuasicomboMusicState::QTE); Music->AdvanceCrossfade(0.25f);
	Music->StopMusic(0.8f); Music->AdvanceCrossfade(0.4f);
	TestTrue(TEXT("Victory fades from quiet QTE volume"), FMath::IsNearlyEqual(Music->GetOutputVolume(BossTrack), 0.14f, 0.0001f));
	Music->AdvanceCrossfade(0.4f);
	TestEqual(TEXT("Victory ends the music"), Music->GetVoiceCount(), 0);
	TestEqual(TEXT("Victory leaves punch effects independent"), Punch->VolumeMultiplier, 0.85f);
	Fixture.ForwardErrorMessages(this);
	return true;
}
#endif
