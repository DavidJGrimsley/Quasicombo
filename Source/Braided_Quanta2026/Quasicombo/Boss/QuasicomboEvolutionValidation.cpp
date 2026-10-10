#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "QuasicomboBoss.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
// The validation script owns this offscreen editor. It never saves map actors.
TSharedPtr<SWindow> ValidationWindow;

void StartValidationPIE(const TArray<FString>& Args)
{
	if (!GEditor || GEditor->PlayWorld || Args.Num() != 2 || !FSlateApplication::Get().IsRenderingOffScreen())
		return;
	const int32 Width = FMath::Clamp(FCString::Atoi(*Args[0]), 640, 3840);
	const int32 Height = FMath::Clamp(FCString::Atoi(*Args[1]), 480, 2160);
	ValidationWindow = SNew(SWindow)
						   .ClientSize(FVector2D(Width, Height))
						   .Title(FText::FromString(TEXT("Evolution validation")))
						   .CreateTitleBar(false)
						   .UseOSWindowBorder(false)
						   .AdjustInitialSizeAndPositionForDPIScale(false);
	FSlateApplication::Get().AddWindow(ValidationWindow.ToSharedRef(), true);
	ValidationWindow->Resize(FVector2D(Width, Height));
	FRequestPlaySessionParams Params;
	Params.CustomPIEWindow = ValidationWindow;
	Params.EditorPlaySettings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
	Params.EditorPlaySettings->NewWindowWidth = Width;
	Params.EditorPlaySettings->NewWindowHeight = Height;
	Params.EditorPlaySettings->GameGetsMouseControl = false;
	Params.EditorPlaySettings->bShouldMinimizeEditorOnNonVRPIE = false;
	Params.bAllowOnlineSubsystem = false;
	GEditor->RequestPlaySession(Params);
}

void SaveValidationShot(const TArray<FString>& Args)
{
	if (!GEditor || !GEditor->PlayWorld || !ValidationWindow || Args.Num() != 1)
		return;
	auto* Viewport = GEditor->PlayWorld->GetGameViewport();
	if (!Viewport || !Viewport->GetGameViewportWidget())
		return;
	TArray<FColor> Pixels;
	FIntVector Size;
	if (!FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(), Pixels, Size))
		return;
	for (auto& Pixel : Pixels)
		Pixel.A = 255;
	TArray64<uint8> PNG;
	FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
	FFileHelper::SaveArrayToFile(PNG, *Args[0]);
	UE_LOG(LogTemp, Display, TEXT("Evolution viewport saved %dx%d: %s"), Size.X, Size.Y, *Args[0]);
}

void FrameValidationDebris()
{
	if (!GEditor || !GEditor->PlayWorld || !ValidationWindow) return;
	UWorld* World = GEditor->PlayWorld;
	auto* Boss = Cast<AQuasicomboBoss>(UGameplayStatics::GetActorOfClass(World, AQuasicomboBoss::StaticClass()));
	auto* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!Boss || !PC) return;
	auto* Camera = World->SpawnActor<ACameraActor>();
	if (!Camera) return;
	const FVector Target = Boss->GetActorLocation() + FVector(0, 0, 20);
	const FVector Eye = Target + FVector(120, 650, 150);
	Camera->SetActorLocation(Eye);
	Camera->SetActorRotation((Target - Eye).Rotation());
	int32 Width, Height;
	PC->GetViewportSize(Width, Height);
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	Camera->GetCameraComponent()->SetAspectRatio(Height > 0 ? float(Width) / Height : 1.777f);
	Camera->GetCameraComponent()->SetFieldOfView(60);
	PC->SetViewTarget(Camera);
}

FAutoConsoleCommand ValidationDebris(TEXT("Quasicombo.EvolutionValidationDebrisView"), TEXT("Frame fallen armor in the offscreen validation viewport."),
	FConsoleCommandDelegate::CreateStatic(&FrameValidationDebris));
FAutoConsoleCommand ValidationPIE(TEXT("Quasicombo.EvolutionValidationPIE"),
								  TEXT("Start an offscreen PIE viewport for rendered validation."),
								  FConsoleCommandWithArgsDelegate::CreateStatic(&StartValidationPIE));
FAutoConsoleCommand ValidationShot(TEXT("Quasicombo.EvolutionValidationShot"), TEXT("Save the validation PIE viewport with its HUD."),
								   FConsoleCommandWithArgsDelegate::CreateStatic(&SaveValidationShot));
}
#endif
