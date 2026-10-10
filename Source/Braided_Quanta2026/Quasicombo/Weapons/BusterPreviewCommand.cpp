#if WITH_EDITOR
#include "BusterCannonComponent.h"
#include "SideScrollingCharacter.h"
#include "QuasicomboLoadoutSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace
{
UWorld* ResolvePreviewWorld(UWorld* World)
{
	if (World && World->IsGameWorld()) return World;
	if (GEngine)
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (UWorld* Candidate = Context.World(); Candidate && Candidate->IsGameWorld())
				return Candidate;
	return nullptr;
}

bool ParseRGB(const TArray<FString>& Args, int32 Offset, FLinearColor& Out)
{
	if (Args.Num() < Offset + 3) return false;
	float R, G, B;
	if (!LexTryParseString(R, *Args[Offset]) || !LexTryParseString(G, *Args[Offset + 1]) ||
		!LexTryParseString(B, *Args[Offset + 2])) return false;
	Out = FLinearColor(R, G, B);
	return true;
}

void PreviewBuster(const TArray<FString>& Args, UWorld* World)
{
	World = ResolvePreviewWorld(World);
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("Buster preview needs an active PIE session. Press Play, then rerun Quasicombo.BusterPreview acquire tsunami|galactic|both."));
		return;
	}
	auto* Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(World,0));
	if (!Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("Buster preview could not find Mike in the active game world."));
		return;
	}
	auto* Cannon = Player->GetBusterCannon();
	if (Args.Num() > 0 && Args[0].Equals(TEXT("travel"), ESearchCase::IgnoreCase))
	{
		bool bHandled = false;
		if (Args.Num() == 2 && Args[1].Equals(TEXT("original"), ESearchCase::IgnoreCase)) bHandled = Cannon->PreviewTravel(true);
		else if (Args.Num() == 2 && Args[1].Equals(TEXT("custom"), ESearchCase::IgnoreCase)) bHandled = Cannon->PreviewTravel(false);
		else if (Args.Num() == 3 && Args[1].Equals(TEXT("size"), ESearchCase::IgnoreCase))
		{
			float Size;
			if (LexTryParseString(Size, *Args[2])) bHandled = Cannon->PreviewTravelSize(Size);
		}
		if (!bHandled) UE_LOG(LogTemp, Warning, TEXT("Use Quasicombo.BusterPreview travel original|custom or travel size <cm> (default 114) during PIE."));
	}
	else if (Args.Contains(TEXT("acquire")))
	{
		const bool bGalactic = Args.Contains(TEXT("galactic"));
		const bool bBoth = Args.Contains(TEXT("both"));
		if (!Cannon->PreviewAcquisition(
			FTransform(FRotator::ZeroRotator, Player->GetActorLocation() + FVector(150,0,30)),
			bBoth ? EBusterEquipAura::Both :
				(bGalactic ? EBusterEquipAura::GalacticField : EBusterEquipAura::Tsunami)))
			UE_LOG(LogTemp, Warning, TEXT("Buster equip preview could not start (pickup already active?)"));
	}
	else if (Args.Contains(TEXT("pink"))) Cannon->SetMikeLightColor(FLinearColor(1,0.05f,0.3f));
	else Cannon->Equip();
}
FAutoConsoleCommandWithWorldAndArgs PreviewCommand(TEXT("Quasicombo.BusterPreview"),
	TEXT("Editor/PIE only: equip, acquire tsunami|galactic|both, pink, travel original|custom, travel size <cm> (default 114)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PreviewBuster));

void PreviewBusterColor(const TArray<FString>& Args, UWorld* World)
{
	World = ResolvePreviewWorld(World);
	if (!World || Args.IsEmpty()) return;
	auto* Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Player || !Player->GetBusterCannon()) return;
	const FString Name = Args[0].ToLower();
	if (Name == TEXT("blue")) Player->GetBusterCannon()->SetMikeLightColor(FLinearColor(0.175f, 0.367f, 1.0f));
	else if (Name == TEXT("cyan")) Player->GetBusterCannon()->SetMikeLightColor(FLinearColor(0.05f, 0.9f, 1.0f));
	else if (Name == TEXT("red")) Player->GetBusterCannon()->SetMikeLightColor(FLinearColor(1.0f, 0.05f, 0.08f));
	else
	{
		FLinearColor Color;
		if (ParseRGB(Args, 0, Color)) Player->GetBusterCannon()->SetMikeLightColor(Color);
	}
}
FAutoConsoleCommandWithWorldAndArgs ColorCommand(TEXT("Quasicombo.BusterColor"),
	TEXT("Editor/PIE only: blue, cyan, red, or R G B (0-1) Mike lights and Thunder core."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PreviewBusterColor));

void PreviewBodyColor(const TArray<FString>& Args, UWorld* World)
{
	World = ResolvePreviewWorld(World);
	if (!World) return;
	FLinearColor Color;
	if (!ParseRGB(Args, 0, Color)) return;
	if (auto* GI = World->GetGameInstance())
		if (auto* State = GI->GetSubsystem<UQuasicomboLoadoutSubsystem>()) State->SetMikeBodyColor(Color);
}
FAutoConsoleCommandWithWorldAndArgs BodyColorCommand(TEXT("Quasicombo.BodyColor"),
	TEXT("Editor/PIE only: R G B (0-1) for Mike's body palette, Buster shell, and Thunder sparks."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PreviewBodyColor));

void PreviewBodyRegionColor(const TArray<FString>& Args, UWorld* World)
{
	World = ResolvePreviewWorld(World);
	if (!World || Args.IsEmpty()) return;
	FLinearColor Color;
	if (!ParseRGB(Args, 1, Color)) return;
	const FString Region = Args[0].ToUpper();
	const EMikeBodyRegion Mask = Region == TEXT("R") ? EMikeBodyRegion::ColorR :
		Region == TEXT("G") ? EMikeBodyRegion::ColorG : EMikeBodyRegion::ColorB;
	if (Region != TEXT("R") && Region != TEXT("G") && Region != TEXT("B")) return;
	if (auto* GI = World->GetGameInstance())
		if (auto* State = GI->GetSubsystem<UQuasicomboLoadoutSubsystem>()) State->SetMikeBodyRegionColor(Mask, Color);
}
FAutoConsoleCommandWithWorldAndArgs BodyRegionCommand(TEXT("Quasicombo.BodyRegionColor"),
	TEXT("Editor/PIE only: R|G|B R G B (0-1) for one Radical Mike body mask."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PreviewBodyRegionColor));
}
#endif
