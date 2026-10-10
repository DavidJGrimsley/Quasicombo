// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "SideScrollingUI.h"
#include "SideScrollingPickup.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "QuasicomboMusicComponent.h"
#include "QuasicomboLoadoutSubsystem.h"
#include "QuasicomboRunSubsystem.h"

bool ASideScrollingGameMode::ShouldShowFrontend() const
{
	const auto* Loadout = UQuasicomboLoadoutSubsystem::GetLoadout(const_cast<ASideScrollingGameMode*>(this));
	const UGameInstance* Instance = GetGameInstance();
	return FrontendDirectorClass && Loadout && Instance && Instance->GetFirstGamePlayer() && !Loadout->HasStartedSession();
}

void ASideScrollingGameMode::StartGameplay()
{
	if (auto* Run = UQuasicomboRunSubsystem::GetRun(this)) Run->StartRun();
	StartBackgroundMusic();
}

void ASideScrollingGameMode::HandleAudioSettingsChanged()
{
	const auto* Loadout = UQuasicomboLoadoutSubsystem::GetLoadout(this);
	if (ForestAmbienceComponent) ForestAmbienceComponent->SetVolumeMultiplier(ForestAmbienceVolume * (Loadout ? Loadout->GetUserAmbienceVolume() : 1.0f));
}

void ASideScrollingGameMode::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* Loadout = UQuasicomboLoadoutSubsystem::GetLoadout(this)) Loadout->OnAudioSettingsChanged.RemoveDynamic(this, &ASideScrollingGameMode::HandleAudioSettingsChanged);
	GetWorldTimerManager().ClearTimer(BackgroundMusicStartTimerHandle);
	Super::EndPlay(Reason);
}

ASideScrollingGameMode::ASideScrollingGameMode()
{
	MusicController = CreateDefaultSubobject<UQuasicomboMusicComponent>(TEXT("MusicController"));
}

void ASideScrollingGameMode::BeginPlay()
{
	Super::BeginPlay();

	// The forest remains live during the frontend; the director starts route music.
	const bool bFrontend = ShouldShowFrontend();
	if (auto* Loadout = UQuasicomboLoadoutSubsystem::GetLoadout(this)) Loadout->OnAudioSettingsChanged.AddUniqueDynamic(this, &ASideScrollingGameMode::HandleAudioSettingsChanged);
	if (USoundWave* ForestAmbience = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/forest_loop.forest_loop")))
	{
		// Audio keeps real time while gameplay timers slow during cinematics or stalls.
		// Loop on the audio device so the forest never waits for a gameplay restart timer.
		ForestAmbience->bLooping = true;
		ForestAmbienceComponent = UGameplayStatics::SpawnSound2D(GetWorld(), ForestAmbience, ForestAmbienceVolume, 1.0f, 0.0f, nullptr, false, false);

		if (ForestAmbienceComponent)
		{
			constexpr float ForestIntroFadeDuration = 3.0f;
			ForestAmbienceComponent->FadeIn(ForestIntroFadeDuration, 1.0f);
			if (!bFrontend) GetWorld()->GetTimerManager().SetTimer(
				BackgroundMusicStartTimerHandle,
				this,
				&ASideScrollingGameMode::StartBackgroundMusic,
				ForestIntroFadeDuration,
				false);
		}
	}
	HandleAudioSettingsChanged();
	if (!bFrontend && !GetWorld()->GetTimerManager().IsTimerActive(BackgroundMusicStartTimerHandle))
		StartBackgroundMusic();

	// create the game UI
	APlayerController* OwningPlayer = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	
	UserInterface = CreateWidget<USideScrollingUI>(OwningPlayer, UserInterfaceClass);

	// create each additional local player.
	// Player 0 will be created automatically as part of regular game init
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}
	if (bFrontend && !GetWorld()->SpawnActor<AActor>(FrontendDirectorClass))
	{
		UE_LOG(LogTemp, Error, TEXT("Quasicombo frontend could not spawn; starting gameplay."));
		StartGameplay();
	}
}

void ASideScrollingGameMode::StartBackgroundMusic()
{
	MusicController->StartLevelMusic(BackgroundMusicVolume);
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
