#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "QuasicomboLoadoutSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMikeLightColorChanged, FLinearColor, Color);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMikeBodyPaletteChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQuasicomboAudioSettingsChanged);

UENUM(BlueprintType)
enum class EMikeBodyRegion : uint8 { ColorR, ColorG, ColorB };

/** Session-only loadout. World/run state deliberately remains in the world subsystem. */
UCLASS()
class UQuasicomboLoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Quasicombo|Session", meta=(WorldContext="WorldContextObject"))
	static UQuasicomboLoadoutSubsystem* GetLoadout(UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category="Quasicombo|Session") bool HasStartedSession() const { return bSessionStarted; }
	void MarkSessionStarted() { bSessionStarted = true; }
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Audio") void SetUserAudioVolumes(float Music, float Ambience);
	UFUNCTION(BlueprintPure, Category="Quasicombo|Audio") float GetUserMusicVolume() const { return UserMusicVolume; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Audio") float GetUserAmbienceVolume() const { return UserAmbienceVolume; }
	UPROPERTY(BlueprintAssignable, Category="Quasicombo|Audio") FQuasicomboAudioSettingsChanged OnAudioSettingsChanged;
	UFUNCTION(BlueprintPure, Category="Buster Cannon") bool HasReplayCannon() const { return bReplayCannon; }
	UFUNCTION(BlueprintCallable, Category="Buster Cannon", meta=(WorldContext="WorldContextObject")) bool ReplayWithBusterCannon(UObject* WorldContextObject);
	UFUNCTION(BlueprintCallable, Category="Mike|Appearance") void SetMikeLightColor(FLinearColor Color);
	UFUNCTION(BlueprintPure, Category="Mike|Appearance") FLinearColor GetMikeLightColor() const { return LightColor; }
	UPROPERTY(BlueprintAssignable, Category="Mike|Appearance") FMikeLightColorChanged OnMikeLightColorChanged;
	/** Change all three painted body masks together, keeping their different values. */
	UFUNCTION(BlueprintCallable, Category="Mike|Appearance") void SetMikeBodyColor(FLinearColor Color);
	/** Fine control for the three mask colors exposed by Radical Mike's materials. */
	UFUNCTION(BlueprintCallable, Category="Mike|Appearance") void SetMikeBodyRegionColor(EMikeBodyRegion Region, FLinearColor Color);
	UFUNCTION(BlueprintPure, Category="Mike|Appearance") FLinearColor GetMikeBodyRegionColor(EMikeBodyRegion Region) const;
	UFUNCTION(BlueprintPure, Category="Mike|Appearance") bool HasCustomMikeBodyPalette() const { return bCustomBodyPalette; }
	/** Returns a visibility-adjusted Color G tint for Buster electrical sparks. */
	UFUNCTION(BlueprintPure, Category="Mike|Appearance") FLinearColor GetMikeSparkColor() const;
	UPROPERTY(BlueprintAssignable, Category="Mike|Appearance") FMikeBodyPaletteChanged OnMikeBodyPaletteChanged;
	void NotifyPlayerReady() { bTravelRequested = false; }
private:
	bool bSessionStarted = false;
	float UserMusicVolume = 1.0f;
	float UserAmbienceVolume = 1.0f;
	bool bReplayCannon = false;
	bool bTravelRequested = false;
	// Temporary high-contrast palette for checking that Mike and Buster VFX share colors.
	FLinearColor LightColor = FLinearColor(1.0f, 0.82f, 0.05f);
	FLinearColor BodyRegions[3] = {
		FLinearColor(0.55f, 0.044f, 0.275f),
		FLinearColor(1.0f, 0.08f, 0.5f),
		FLinearColor(1.0f, 0.128f, 0.8f)
	};
	bool bCustomBodyPalette = true;
};
