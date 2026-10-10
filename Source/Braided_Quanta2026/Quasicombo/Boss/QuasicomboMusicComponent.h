#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuasicomboRouteTypes.h"
#include "QuasicomboMusicComponent.generated.h"

class USoundBase;
class UAudioComponent;
class USoundWave;

UENUM(BlueprintType)
enum class EQuasicomboMusicState : uint8 { Exploration, Combat, Evolution, QTE, Retry, Defeated, Ended };

USTRUCT()
struct FQuasicomboMusicVoice
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UAudioComponent> Audio;
	float Gain = 0.0f;
	float From = 0.0f;
	float Target = 0.0f;
	float Elapsed = 0.0f;
	float Duration = 1.0f;
	float Volume = 1.0f;
	// Audio-device sample position, with a real-time fallback before its first callback.
	double PlaybackSeconds = 0.0;
	float StartTimeSeconds = 0.0f;
	bool bHasAudioPlaybackPosition = false;
};

/** Single music owner for the level, routes, approach and encounter. Forest ambience is independent. */
UCLASS(ClassGroup=(Quasicombo), meta=(BlueprintSpawnableComponent))
class UQuasicomboMusicComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UQuasicomboMusicComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music") TSoftObjectPtr<USoundBase> DefaultMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music") TSoftObjectPtr<USoundBase> RouteAMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music") TSoftObjectPtr<USoundBase> RouteCMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music") TSoftObjectPtr<USoundBase> CombatMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music") TSoftObjectPtr<USoundBase> EvolutionMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music", meta=(ClampMin="0.0")) float CrossfadeSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music", meta=(ClampMin="0.0", ClampMax="1.0")) float EncounterVolume = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music", meta=(ClampMin="0.0", ClampMax="1.0")) float EvolutionVolumeMultiplier = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music|QTE", meta=(ClampMin="0.0", ClampMax="1.0")) float QTEVolumeMultiplier = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music|QTE", meta=(ClampMin="0.0")) float QTEDuckFadeSeconds = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Music", meta=(ClampMin="0.0", ClampMax="1.0")) float ApproachMusicGain = 1.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Music") float ApproachGain = 1.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Music") EQuasicomboMusicState State = EQuasicomboMusicState::Exploration;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Music") TObjectPtr<USoundBase> CurrentMusic;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Music") bool bEncounterActive = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Music") int32 TrackStarts = 0;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Music|Debug") float LastTrackStartSeconds = 0.0f;
	UFUNCTION(BlueprintCallable, Category="Music") void StartLevelMusic(float Volume = 1.0f);
	UFUNCTION(BlueprintCallable, Category="Music") void SetApproachProgress(float Progress);
	void SetEncounterState(EQuasicomboMusicState NewState, USoundBase* Override = nullptr);
	void StopMusic(float Seconds = 0.8f);
	void AdvanceCrossfade(float RealDeltaSeconds);
	int32 GetVoiceCount() const { return Voices.Num(); }
	float GetOutputVolume(USoundBase* Sound) const;
	UFUNCTION(BlueprintPure, Category="Music|Debug") float GetPlaybackSeconds() const;
	float GetTrackPlaybackSeconds(USoundBase* Sound) const;
	static float WrapPlaybackSeconds(double Seconds, const USoundBase* Sound);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient) TArray<FQuasicomboMusicVoice> Voices;
	float LevelVolume = 1.0f;
	bool bStarted = false;
	double LastTickSeconds = 0.0;
	float DuckGain = 1.0f;
	float DuckFrom = 1.0f;
	float DuckTarget = 1.0f;
	float DuckElapsed = 0.0f;
	EQuasicomboLane CurrentLane = EQuasicomboLane::B;
	UFUNCTION() void HandleRouteCommitted(int32 Section, EQuasicomboLane Lane);
	UFUNCTION() void HandleRunEnded(EQuasicomboRunOutcome Outcome);
	void RequestTrack(USoundBase* Sound, bool bRestart = false, bool bRetarget = false);
	void PlayVoice(FQuasicomboMusicVoice& Voice, float StartTimeSeconds);
	void HandlePlaybackPercent(const UAudioComponent* Audio, const USoundWave* Wave, float Percent);
	USoundBase* ResolveRouteTrack() const;
	void ApplyVolumes();
};
