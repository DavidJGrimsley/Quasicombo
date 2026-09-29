// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SideScrollingGameMode.generated.h"

class USideScrollingUI;
class UAudioComponent;

/**
 *  Simple Side Scrolling Game Mode
 *  Spawns and manages the game UI
 *  Counts pickups collected by the player
 */
UCLASS(abstract)
class ASideScrollingGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	/** Class of UI widget to spawn when the game starts */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<USideScrollingUI> UserInterfaceClass;

	/** User interface widget for the game */
	UPROPERTY(BlueprintReadOnly, Category="UI")
	TObjectPtr<USideScrollingUI> UserInterface;

	/** Number of pickups collected by the player */
	UPROPERTY(BlueprintReadOnly, Category="Pickups")
	int32 PickupsCollected = 0;

	/** Looping background music for the active game session. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BackgroundMusicComponent;

	/** Base volume for the piano music. Adjust on BP_SideScrollingGameMode. */
	UPROPERTY(EditDefaultsOnly, Category="Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BackgroundMusicVolume = 1.0f;

	FTimerHandle BackgroundMusicStartTimerHandle;

	/** Independent forest ambience layer, crossfaded between loop restarts. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ForestAmbienceComponent;

	/** Base volume for the forest ambience. Adjust on BP_SideScrollingGameMode. */
	UPROPERTY(EditDefaultsOnly, Category="Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ForestAmbienceVolume = 0.35f;

	FTimerHandle ForestAmbienceFadeOutTimerHandle;
	FTimerHandle ForestAmbienceRestartTimerHandle;
	float ForestAmbienceLoopDuration = 0.0f;
	float ForestAmbienceFadeDuration = 2.0f;

	void FadeOutForestAmbience();
	void RestartForestAmbience();
	void StartBackgroundMusic();

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Assigns a PlayerStart to a specific player */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:

	/** Determines how many local players should be spawned on game start */
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta = (ClampMin = 1, ClampMax = 4))
	int32 NumberOfLocalPlayers = 1;

	/** Used to assign players to different PlayerStarts in the level */
	int32 CurrentPlayerStartAssignment = 0;

public:

	/** Receives an interaction event from another actor */
	virtual void ProcessPickup();
};
