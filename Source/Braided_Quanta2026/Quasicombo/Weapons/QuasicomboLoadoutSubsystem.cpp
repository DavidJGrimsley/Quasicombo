#include "QuasicomboLoadoutSubsystem.h"
#include "QuasicomboRunSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

UQuasicomboLoadoutSubsystem* UQuasicomboLoadoutSubsystem::GetLoadout(UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	return Instance ? Instance->GetSubsystem<UQuasicomboLoadoutSubsystem>() : nullptr;
}

void UQuasicomboLoadoutSubsystem::SetUserAudioVolumes(float Music, float Ambience)
{
	UserMusicVolume = FMath::IsFinite(Music) ? FMath::Clamp(Music, 0.0f, 1.0f) : 1.0f;
	UserAmbienceVolume = FMath::IsFinite(Ambience) ? FMath::Clamp(Ambience, 0.0f, 1.0f) : 1.0f;
	OnAudioSettingsChanged.Broadcast();
}

namespace
{
FLinearColor ClampAppearanceColor(FLinearColor Color)
{
	return FLinearColor(FMath::Clamp(Color.R, 0.0f, 1.0f),
		FMath::Clamp(Color.G, 0.0f, 1.0f), FMath::Clamp(Color.B, 0.0f, 1.0f), 1.0f);
}
}

bool UQuasicomboLoadoutSubsystem::ReplayWithBusterCannon(UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const auto* Run = World ? World->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (bTravelRequested || !Run || Run->GetOutcome() != EQuasicomboRunOutcome::Victory || !Run->IsVictoryPresentationReady()) return false;
	bTravelRequested = true;
	bReplayCannon = true;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetPause(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		PC->ResetIgnoreMoveInput();
		PC->ResetIgnoreLookInput();
		PC->FlushPressedKeys();
	}
	UGameplayStatics::OpenLevel(World, TEXT("Lvl_Main"));
	return true;
}

void UQuasicomboLoadoutSubsystem::SetMikeLightColor(FLinearColor Color)
{
	LightColor = ClampAppearanceColor(Color);
	OnMikeLightColorChanged.Broadcast(LightColor);
}

void UQuasicomboLoadoutSubsystem::SetMikeBodyColor(FLinearColor Color)
{
	Color = ClampAppearanceColor(Color);
	BodyRegions[0] = ClampAppearanceColor(Color * 0.55f);
	BodyRegions[1] = Color;
	BodyRegions[2] = ClampAppearanceColor(Color * 1.6f);
	bCustomBodyPalette = true;
	OnMikeBodyPaletteChanged.Broadcast();
}

void UQuasicomboLoadoutSubsystem::SetMikeBodyRegionColor(EMikeBodyRegion Region, FLinearColor Color)
{
	const int32 Index = static_cast<int32>(Region);
	if (Index < 0 || Index >= 3) return;
	BodyRegions[Index] = ClampAppearanceColor(Color);
	bCustomBodyPalette = true;
	OnMikeBodyPaletteChanged.Broadcast();
}

FLinearColor UQuasicomboLoadoutSubsystem::GetMikeBodyRegionColor(EMikeBodyRegion Region) const
{
	const int32 Index = static_cast<int32>(Region);
	return Index >= 0 && Index < 3 ? BodyRegions[Index] : FLinearColor::White;
}

FLinearColor UQuasicomboLoadoutSubsystem::GetMikeSparkColor() const
{
	const FLinearColor Base = BodyRegions[1];
	const float Brightest = FMath::Max3(Base.R, Base.G, Base.B);
	const float Gain = Brightest > 0.001f ? FMath::Max(1.0f, 0.7f / Brightest) : 1.0f;
	return ClampAppearanceColor(Base * Gain);
}
