#include "QuasicomboMusicComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "QuasicomboLoadoutSubsystem.h"

namespace
{
float MusicDuration(const USoundBase* Sound)
{
	// GetDuration() returns an indefinite sentinel for looping SoundWaves, rather than the file length.
	const auto* Wave = Cast<USoundWave>(Sound);
	const float Duration = Wave ? Wave->Duration : Sound ? Sound->GetDuration() : 0.0f;
	return FMath::IsFinite(Duration) && Duration > 0.0f && Duration < INDEFINITELY_LOOPING_DURATION ? Duration : 0.0f;
}
}

UQuasicomboMusicComponent::UQuasicomboMusicComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	DefaultMusic = FSoftObjectPath(TEXT("/Game/Audio/Quantum_Ambience.Quantum_Ambience"));
	RouteAMusic = FSoftObjectPath(TEXT("/Game/Audio/Music/SW_RouteA.SW_RouteA"));
	RouteCMusic = FSoftObjectPath(TEXT("/Game/Audio/Music/SW_RouteC.SW_RouteC"));
	CombatMusic = FSoftObjectPath(TEXT("/Game/Audio/Music/SW_Boss.SW_Boss"));
	EvolutionMusic = FSoftObjectPath(TEXT("/Game/Audio/Music/SW_Evolution.SW_Evolution"));
}

void UQuasicomboMusicComponent::BeginPlay()
{
	Super::BeginPlay();
	LastTickSeconds = FPlatformTime::Seconds();
	if (auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		Run->OnRouteCommitted.AddUniqueDynamic(this, &UQuasicomboMusicComponent::HandleRouteCommitted);
		Run->OnRunEnded.AddUniqueDynamic(this, &UQuasicomboMusicComponent::HandleRunEnded);
		const auto Routes = Run->GetRouteHistory();
		if (!Routes.IsEmpty()) CurrentLane = Routes.Last();
	}
}

USoundBase* UQuasicomboMusicComponent::ResolveRouteTrack() const
{
	USoundBase* Track = CurrentLane == EQuasicomboLane::A ? RouteAMusic.LoadSynchronous() :
		CurrentLane == EQuasicomboLane::C ? RouteCMusic.LoadSynchronous() : DefaultMusic.LoadSynchronous();
	return Track ? Track : DefaultMusic.LoadSynchronous();
}

void UQuasicomboMusicComponent::StartLevelMusic(float Volume)
{
	LevelVolume = FMath::IsFinite(Volume) ? FMath::Clamp(Volume, 0.0f, 1.0f) : 1.0f;
	bStarted = true;
	if (!bEncounterActive && State == EQuasicomboMusicState::Exploration) RequestTrack(ResolveRouteTrack(), false, true);
}

void UQuasicomboMusicComponent::HandleRouteCommitted(int32, EQuasicomboLane Lane)
{
	if (bEncounterActive || State != EQuasicomboMusicState::Exploration) return;
	CurrentLane = Lane;
	if (bStarted) RequestTrack(ResolveRouteTrack());
}

void UQuasicomboMusicComponent::SetApproachProgress(float Progress)
{
	if (bEncounterActive || State != EQuasicomboMusicState::Exploration || !FMath::IsFinite(Progress)) return;
	ApproachGain = FMath::Clamp(ApproachMusicGain, 0.0f, 1.0f) * (1.0f - FMath::Clamp(Progress, 0.0f, 1.0f));
	ApplyVolumes();
}

void UQuasicomboMusicComponent::SetEncounterState(EQuasicomboMusicState NewState, USoundBase* Override)
{
	if (State == EQuasicomboMusicState::Defeated || State == EQuasicomboMusicState::Ended) return;
	const bool bNewEncounter = !bEncounterActive;
	if (bNewEncounter)
	{
		// Bake the stair attenuation into outgoing voices before releasing the global gain.
		// Otherwise combat would briefly bring the silenced route track back up.
		for (auto& Voice : Voices) Voice.Gain *= ApproachGain;
		ApproachGain = 1.0f;
		bEncounterActive = true;
	}
	const bool bEnteringEvolution = NewState == EQuasicomboMusicState::Evolution && State != NewState;
	const float NewDuckTarget = NewState == EQuasicomboMusicState::QTE ? FMath::Clamp(QTEVolumeMultiplier, 0.0f, 1.0f) : 1.0f;
	if (!FMath::IsNearlyEqual(NewDuckTarget, DuckTarget))
	{
		DuckFrom = DuckGain;
		DuckTarget = NewDuckTarget;
		DuckElapsed = 0.0f;
	}
	State = NewState;
	bStarted = true;
	USoundBase* Desired = NewState == EQuasicomboMusicState::Evolution ? EvolutionMusic.LoadSynchronous() : Override;
	if (!Desired) Desired = CombatMusic.LoadSynchronous();
	RequestTrack(Desired, bEnteringEvolution, bNewEncounter);
	// Ducking is independent of track switches; entering QTE can keep the same song.
	AdvanceCrossfade(0.0f);
}

void UQuasicomboMusicComponent::RequestTrack(USoundBase* Sound, bool bRestart, bool bRetarget)
{
	const bool bSameSound = CurrentMusic == Sound;
	if (!Sound || (!bRestart && !bRetarget && bSameSound)) return;
	const bool bRouteSwitch = !bEncounterActive && State == EQuasicomboMusicState::Exploration && CurrentMusic && !bSameSound;
	// Route variants pick up at the outgoing song's playhead. Encounter/cinematic starts keep their own semantics.
	const float StartTime = bRouteSwitch ? WrapPlaybackSeconds(GetPlaybackSeconds(), Sound) : 0.0f;
	// Stair silence must not stop the source or reset its playback position when walking back down.
	Sound->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	// Match the existing piano behavior and loop optional SoundWave overrides too.
	if (auto* Wave = Cast<USoundWave>(Sound)) Wave->bLooping = true;
	int32 Incoming = INDEX_NONE;
	for (int32 I = 0; I < Voices.Num(); ++I)
	{
		if (Voices[I].Audio && Voices[I].Audio->Sound == Sound) Incoming = I;
	}
	if (Incoming == INDEX_NONE)
	{
		FQuasicomboMusicVoice Voice;
		Voice.Audio = NewObject<UAudioComponent>(GetOwner());
		Voice.Audio->bAutoActivate = false;
		Voice.Audio->bAutoDestroy = false;
		Voice.Audio->bAllowSpatialization = false;
		Voice.Audio->bIsUISound = false;
		Voice.Audio->SetSound(Sound);
		Voice.Audio->OnAudioPlaybackPercentNative.AddUObject(this, &UQuasicomboMusicComponent::HandlePlaybackPercent);
		Voice.Audio->SetPitchMultiplier(1.0f);
		Voice.Audio->SetVolumeMultiplier(0.0f);
		Voice.Audio->RegisterComponent();
		Incoming = Voices.Add(Voice);
	}
	for (int32 I = 0; I < Voices.Num(); ++I)
	{
		auto& Voice = Voices[I];
		Voice.From = Voice.Gain;
		Voice.Target = I == Incoming ? 1.0f : 0.0f;
		Voice.Elapsed = 0.0f;
		Voice.Duration = FMath::Max(0.0f, CrossfadeSeconds);
	}
	auto& Voice = Voices[Incoming];
	Voice.Volume = bEncounterActive ? EncounterVolume : LevelVolume;
	if (State == EQuasicomboMusicState::Evolution) Voice.Volume *= EvolutionVolumeMultiplier;
	const float PositionDifference = FMath::Abs(GetTrackPlaybackSeconds(Sound) - StartTime);
	const float Duration = MusicDuration(Sound);
	const bool bAligned = PositionDifference < 0.1f || (Duration > 0.0f && FMath::Abs(Duration - PositionDifference) < 0.1f);
	if (bRestart || (!bSameSound && !Voice.Audio->IsPlaying()) || (bRouteSwitch && !bAligned))
	{
		PlayVoice(Voice, StartTime);
	}
	LastTrackStartSeconds = Voice.StartTimeSeconds;
	CurrentMusic = Sound;
	AdvanceCrossfade(0.0f);
}

float UQuasicomboMusicComponent::WrapPlaybackSeconds(double Seconds, const USoundBase* Sound)
{
	if (!FMath::IsFinite(Seconds) || Seconds < 0.0) return 0.0f;
	const float Duration = MusicDuration(Sound);
	return Duration > 0.0f && FMath::IsFinite(Duration) ? float(FMath::Fmod(Seconds, double(Duration))) : float(Seconds);
}

void UQuasicomboMusicComponent::PlayVoice(FQuasicomboMusicVoice& Voice, float StartTimeSeconds)
{
	Voice.Audio->Stop();
	Voice.StartTimeSeconds = WrapPlaybackSeconds(StartTimeSeconds, Voice.Audio->Sound);
	Voice.PlaybackSeconds = Voice.StartTimeSeconds;
	Voice.bHasAudioPlaybackPosition = false;
	LastTrackStartSeconds = Voice.StartTimeSeconds;
	Voice.Audio->Play(Voice.StartTimeSeconds);
	++TrackStarts;
}

void UQuasicomboMusicComponent::HandlePlaybackPercent(const UAudioComponent* Audio, const USoundWave* Wave, float Percent)
{
	if (!Wave || !FMath::IsFinite(Percent) || Percent < 0.0f || Wave->Duration <= 0.0f) return;
	for (auto& Voice : Voices)
	{
		if (Voice.Audio != Audio || Voice.Audio->Sound != Wave) continue;
		const double Seconds = double(Percent) * Wave->Duration;
		// The mixer can send zero while a nonzero seek is still initializing.
		if (!Voice.bHasAudioPlaybackPosition && Seconds + 0.1 < Voice.StartTimeSeconds) return;
		Voice.PlaybackSeconds = Seconds;
		Voice.bHasAudioPlaybackPosition = true;
		return;
	}
}

float UQuasicomboMusicComponent::GetTrackPlaybackSeconds(USoundBase* Sound) const
{
	for (const auto& Voice : Voices)
		if (Voice.Audio && Voice.Audio->Sound == Sound) return WrapPlaybackSeconds(Voice.PlaybackSeconds, Sound);
	return 0.0f;
}

float UQuasicomboMusicComponent::GetPlaybackSeconds() const
{
	return GetTrackPlaybackSeconds(CurrentMusic);
}

void UQuasicomboMusicComponent::ApplyVolumes()
{
	const auto* Loadout = UQuasicomboLoadoutSubsystem::GetLoadout(this);
	const float UserGain = Loadout ? Loadout->GetUserMusicVolume() : 1.0f;
	for (auto& Voice : Voices)
		if (Voice.Audio) Voice.Audio->SetVolumeMultiplier(Voice.Volume * Voice.Gain * ApproachGain * DuckGain * UserGain);
}

void UQuasicomboMusicComponent::AdvanceCrossfade(float RealDeltaSeconds)
{
	DuckElapsed += FMath::Max(0.0f, RealDeltaSeconds);
	const float DuckAlpha = QTEDuckFadeSeconds <= 0.0f ? 1.0f : FMath::Clamp(DuckElapsed / QTEDuckFadeSeconds, 0.0f, 1.0f);
	DuckGain = FMath::Lerp(DuckFrom, DuckTarget, DuckAlpha);
	for (int32 I = Voices.Num() - 1; I >= 0; --I)
	{
		auto& Voice = Voices[I];
		Voice.Elapsed += FMath::Max(0.0f, RealDeltaSeconds);
		const float Alpha = Voice.Duration <= 0.0f ? 1.0f : FMath::Clamp(Voice.Elapsed / Voice.Duration, 0.0f, 1.0f);
		Voice.Gain = FMath::Lerp(Voice.From, Voice.Target, Alpha);
		if (Alpha >= 1.0f && Voice.Target == 0.0f)
		{
			Voice.Audio->Stop();
			Voice.Audio->DestroyComponent();
			Voices.RemoveAt(I);
		}
	}
	ApplyVolumes();
}

float UQuasicomboMusicComponent::GetOutputVolume(USoundBase* Sound) const
{
	for (const auto& Voice : Voices)
		if (Voice.Audio && Voice.Audio->Sound == Sound) return Voice.Audio->VolumeMultiplier;
	return 0.0f;
}

void UQuasicomboMusicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const double Now = FPlatformTime::Seconds();
	const float RealDelta = LastTickSeconds > 0.0 ? float(Now - LastTickSeconds) : 0.0f;
	LastTickSeconds = Now;
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	if (!bPaused)
	{
		for (auto& Voice : Voices)
			if (!Voice.bHasAudioPlaybackPosition) Voice.PlaybackSeconds += RealDelta;
		AdvanceCrossfade(RealDelta);
	}
	for (auto& Voice : Voices)
		if (Voice.Target > 0.0f && !Voice.Audio->IsPlaying() && !bPaused)
			PlayVoice(Voice, WrapPlaybackSeconds(Voice.PlaybackSeconds, Voice.Audio->Sound));
}

void UQuasicomboMusicComponent::StopMusic(float Seconds)
{
	if (State == EQuasicomboMusicState::Defeated || State == EQuasicomboMusicState::Ended) return;
	State = EQuasicomboMusicState::Defeated;
	// Fade from the current QTE volume without lifting the music during victory.
	DuckFrom = DuckTarget = DuckGain;
	CurrentMusic = nullptr;
	for (auto& Voice : Voices)
	{
		Voice.From = Voice.Gain;
		Voice.Target = 0.0f;
		Voice.Elapsed = 0.0f;
		Voice.Duration = FMath::Max(0.0f, Seconds);
	}
	AdvanceCrossfade(0.0f);
}

void UQuasicomboMusicComponent::HandleRunEnded(EQuasicomboRunOutcome Outcome)
{
	StopMusic(0.8f);
}

void UQuasicomboMusicComponent::EndPlay(EEndPlayReason::Type Reason)
{
	for (auto& Voice : Voices) if (Voice.Audio) Voice.Audio->Stop();
	Voices.Empty();
	State = EQuasicomboMusicState::Ended;
	Super::EndPlay(Reason);
}
