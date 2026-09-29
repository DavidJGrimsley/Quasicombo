// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "SideScrollingUI.h"
#include "SideScrollingPickup.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"

void ASideScrollingGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Begin with the forest layer, then bring in the piano after the five-second intro.
	if (USoundWave* ForestAmbience = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/forest_loop.forest_loop")))
	{
		ForestAmbience->bLooping = false;
		ForestAmbienceLoopDuration = FMath::Max(3.0f, ForestAmbience->GetDuration());
		ForestAmbienceFadeDuration = FMath::Min(2.0f, ForestAmbienceLoopDuration * 0.25f);
		ForestAmbienceComponent = UGameplayStatics::SpawnSound2D(GetWorld(), ForestAmbience, ForestAmbienceVolume, 1.0f, 0.0f, nullptr, false, false);

		if (ForestAmbienceComponent)
		{
			constexpr float ForestIntroFadeDuration = 3.0f;
			ForestAmbienceComponent->FadeIn(ForestIntroFadeDuration, 1.0f);
			GetWorld()->GetTimerManager().SetTimer(
				BackgroundMusicStartTimerHandle,
				this,
				&ASideScrollingGameMode::StartBackgroundMusic,
				ForestIntroFadeDuration,
				false);
			GetWorld()->GetTimerManager().SetTimer(
				ForestAmbienceFadeOutTimerHandle,
				this,
				&ASideScrollingGameMode::FadeOutForestAmbience,
				FMath::Max(0.1f, ForestAmbienceLoopDuration - ForestAmbienceFadeDuration),
				false);
		}
	}

	// create the game UI
	APlayerController* OwningPlayer = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	
	UserInterface = CreateWidget<USideScrollingUI>(OwningPlayer, UserInterfaceClass);

	// create each additional local player.
	// Player 0 will be created automatically as part of regular game init
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}
}

void ASideScrollingGameMode::StartBackgroundMusic()
{
	if (USoundWave* BackgroundMusic = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/Quantum_Ambience.Quantum_Ambience")))
	{
		BackgroundMusic->bLooping = true;
		BackgroundMusicComponent = UGameplayStatics::SpawnSound2D(GetWorld(), BackgroundMusic, BackgroundMusicVolume, 1.0f, 0.0f, nullptr, false, false);
	}
}

void ASideScrollingGameMode::FadeOutForestAmbience()
{
	if (ForestAmbienceComponent)
	{
		ForestAmbienceComponent->FadeOut(ForestAmbienceFadeDuration, 0.0f);
		GetWorld()->GetTimerManager().SetTimer(
			ForestAmbienceRestartTimerHandle,
			this,
			&ASideScrollingGameMode::RestartForestAmbience,
			ForestAmbienceFadeDuration,
			false);
	}
}

void ASideScrollingGameMode::RestartForestAmbience()
{
	if (ForestAmbienceComponent)
	{
		ForestAmbienceComponent->Stop();
		ForestAmbienceComponent->FadeIn(ForestAmbienceFadeDuration, 1.0f);
		GetWorld()->GetTimerManager().SetTimer(
			ForestAmbienceFadeOutTimerHandle,
			this,
			&ASideScrollingGameMode::FadeOutForestAmbience,
			FMath::Max(0.1f, ForestAmbienceLoopDuration - ForestAmbienceFadeDuration),
			false);
	}
}

AActor* ASideScrollingGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// build the current player tag
	FName PlayerTag = FName(*FString::Printf(TEXT("Player%d"), CurrentPlayerStartAssignment));

	// find all player starts with the matching player tag
	TArray<AActor*> PlayerStarts;

	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), PlayerTag, PlayerStarts);

	// increment the player start assignment index
	++CurrentPlayerStartAssignment;

	// if no PlayerStarts were found, default to all PlayerStarts instead
	if (PlayerStarts.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);
	}

	// have we found at least one PlayerStart?
	if (!PlayerStarts.IsEmpty())
	{
		return PlayerStarts[ FMath::RandRange(0, PlayerStarts.Num() - 1) ];
	}

	// no PlayerStarts in the level
	return nullptr;
}

void ASideScrollingGameMode::ProcessPickup()
{
	// increment the pickups counter
	++PickupsCollected;

	if (UserInterface)
	{
		// if this is the first pickup we collect, show the UI
		if (PickupsCollected == 1)
		{
		
			UserInterface->AddToViewport(0);
		}

		// update the pickups counter on the UI
		UserInterface->UpdatePickups(PickupsCollected);
	}
	
}
