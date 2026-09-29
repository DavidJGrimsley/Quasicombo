#include "QuasicomboHUDActor.h"
#include "QuasicomboRunSubsystem.h"
#include "QuasicomboBoss.h"
#include "QuantumBossComponent.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboVictoryWidget.h"
#include "QuasicomboCreditsData.h"
#include "SideScrollingCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Draw the controller legend in Slate so the blockout does not need a third-party icon pack.
bool GetXboxPromptIcon(const FKey& Key, FString& Glyph, FLinearColor& Color, FVector2D& Size)
{
	Size = FVector2D(96.0f, 96.0f);
	if (Key == EKeys::Gamepad_FaceButton_Bottom) { Glyph = TEXT("A"); Color = FLinearColor(0.20f, 0.66f, 0.16f); }
	else if (Key == EKeys::Gamepad_FaceButton_Right) { Glyph = TEXT("B"); Color = FLinearColor(0.84f, 0.14f, 0.12f); }
	else if (Key == EKeys::Gamepad_FaceButton_Left) { Glyph = TEXT("X"); Color = FLinearColor(0.10f, 0.43f, 0.87f); }
	else if (Key == EKeys::Gamepad_FaceButton_Top) { Glyph = TEXT("Y"); Color = FLinearColor(0.92f, 0.67f, 0.05f); }
	else
	{
		Size = FVector2D(128.0f, 76.0f);
		Color = FLinearColor(0.86f, 0.88f, 0.92f);
		if (Key == EKeys::Gamepad_RightShoulder) Glyph = TEXT("RB");
		else if (Key == EKeys::Gamepad_LeftShoulder) Glyph = TEXT("LB");
		else if (Key == EKeys::Gamepad_RightTriggerAxis) Glyph = TEXT("RT");
		else if (Key == EKeys::Gamepad_LeftTriggerAxis) Glyph = TEXT("LT");
		else return false;
	}
	return true;
}
}

TSharedRef<SWidget> UQuasicomboHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;
		auto AddLine = [this, Canvas](const TCHAR* Name, FVector2D Position, int32 Size, const FLinearColor& Color)
		{
			UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
			FSlateFontInfo Font = Line->GetFont(); Font.Size = Size; Line->SetFont(Font);
			Line->SetColorAndOpacity(FSlateColor(Color));
			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Line);
			Slot->SetPosition(Position);
			Slot->SetAutoSize(true);
			return Line;
		};
		RunText = AddLine(TEXT("RunText"), FVector2D(25, 20), 22, FLinearColor::White);
		BossText = AddLine(TEXT("BossText"), FVector2D(25, 55), 19, FLinearColor(1.0f, 0.65f, 0.45f));
		auto AddBossBar = [this, Canvas](const TCHAR* Name, float Y, const FLinearColor& Color)
		{
			UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(Name));
			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Bar);
			Slot->SetPosition(FVector2D(25, Y));
			Slot->SetSize(FVector2D(350, 14));
			Bar->SetFillColorAndOpacity(Color);
			Bar->SetVisibility(ESlateVisibility::Collapsed);
			return Bar;
		};
		BossArmor.Add(AddBossBar(TEXT("BossArmor1"), 89, FLinearColor(0.1f, 0.7f, 1.0f)));
		BossArmor.Add(AddBossBar(TEXT("BossArmor2"), 108, FLinearColor(0.7f, 0.35f, 1.0f)));
		BossHealth = AddBossBar(TEXT("BossHealth"), 127, FLinearColor(0.8f, 0.12f, 0.08f));
		PromptText = AddLine(TEXT("PromptText"), FVector2D::ZeroVector, 32, FLinearColor::White);
		PromptText->SetJustification(ETextJustify::Center);
		if (UCanvasPanelSlot* PromptSlot = Cast<UCanvasPanelSlot>(PromptText->Slot))
		{
			PromptSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PromptSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PromptSlot->SetPosition(FVector2D(0.0f, 75.0f));
		}
		PromptIcon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptIcon"));
		PromptIcon->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 48.0f, FVector2f(96.0f, 96.0f)));
		PromptIcon->SetHorizontalAlignment(HAlign_Center);
		PromptIcon->SetVerticalAlignment(VAlign_Center);
		PromptIconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptIconText"));
		FSlateFontInfo IconFont = PromptIconText->GetFont(); IconFont.Size = 52; PromptIconText->SetFont(IconFont);
		PromptIconText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		PromptIcon->AddChild(PromptIconText);
		UCanvasPanelSlot* IconSlot = Canvas->AddChildToCanvas(PromptIcon);
		IconSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		IconSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		IconSlot->SetPosition(FVector2D(0.0f, -40.0f));
		IconSlot->SetSize(FVector2D(96.0f, 96.0f));
		PromptIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Super::RebuildWidget();
}

void UQuasicomboHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	AttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Variant_Combat/Input/Actions/IA_ComboAttack.IA_ComboAttack"));
	DashAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Variant_Platforming/Input/Actions/IA_Dash.IA_Dash"));
	JumpAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
}

FString UQuasicomboHUDWidget::DescribePromptAction(EQuasicomboQTEInput Input, FKey* OutGamepadKey) const
{
	if (OutGamepadKey) *OutGamepadKey = FKey();
	const UInputAction* Action = Input == EQuasicomboQTEInput::Attack ? AttackAction.Get() :
		Input == EQuasicomboQTEInput::Dash ? DashAction.Get() : JumpAction.Get();
	const FString Name = Input == EQuasicomboQTEInput::Attack ? TEXT("ATTACK") :
		Input == EQuasicomboQTEInput::Dash ? TEXT("DASH") : TEXT("JUMP");
	const APlayerController* PC = GetOwningPlayer();
	if (!Action || !PC || !PC->GetLocalPlayer()) return Name;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!InputSubsystem) return Name;
	FString Keyboard;
	for (const FKey& Key : InputSubsystem->QueryKeysMappedToAction(Action))
	{
		if (Key.IsGamepadKey())
		{
			if (OutGamepadKey && !OutGamepadKey->IsValid()) *OutGamepadKey = Key;
		}
		else if (Keyboard.IsEmpty()) Keyboard = Key.GetDisplayName(false).ToString();
	}
	return Keyboard.IsEmpty() ? Name : Name + TEXT("  |  ") + Keyboard;
}

void UQuasicomboHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	ASideScrollingCharacter* Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(UGameplayStatics::GetActorOfClass(this, AQuasicomboBoss::StaticClass()));
	if (!Run) return;
	RunText->SetText(FText::FromString(FString::Printf(TEXT("HP %.1f/%.1f   Time %.1fs   Combo %d   Best %d"),
		Player ? Player->GetCurrentHP() : 0.0f, Player ? Player->GetMaxHP() : 0.0f,
		Run->GetElapsedSeconds(), Player ? Player->GetHitComboCount() : 0, Run->GetHighestCombo())));
	const bool bBossActive = Boss && Boss->GetBossPhase() != EQuasicomboBossPhase::Waiting;
	BossText->SetText(FText::FromString(bBossActive ? Boss->GetEncounterStatus() : FString()));
	BossHealth->SetVisibility(bBossActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bBossActive) BossHealth->SetPercent(Boss->GetBaseHealthFraction());
	for (int32 Index = 0; Index < BossArmor.Num(); ++Index)
	{
		const bool bArmorVisible = bBossActive && Boss->GetArmorBars() > Index;
		BossArmor[Index]->SetVisibility(bArmorVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bArmorVisible) BossArmor[Index]->SetPercent(Boss->GetArmorFraction(Index));
	}
	bool bShowPromptIcon = false;
	if (Run->GetOutcome() == EQuasicomboRunOutcome::Victory) PromptText->SetText(FText::FromString(TEXT("VICTORY")));
	else if (Run->GetOutcome() == EQuasicomboRunOutcome::Defeat) PromptText->SetText(FText::FromString(TEXT("RUN LOST")));
	else if (Boss && Boss->GetBossPhase() == EQuasicomboBossPhase::Retry) PromptText->SetText(FText::FromString(TEXT("ONE MORE CHANCE")));
	else if (Boss && Boss->GetBossPhase() == EQuasicomboBossPhase::AwaitPickup) PromptText->SetText(FText::GetEmpty());
	else if (Boss && Boss->GetBossPhase() == EQuasicomboBossPhase::Evolution) PromptText->SetText(FText::FromString(Boss->GetEvolutionCue()));
	else if (Boss && Boss->QTE->IsFinishingBeat()) PromptText->SetText(FText::FromString(TEXT("FINISH!")));
	else if (Boss && Boss->QTE->IsQTEActive())
	{
		FKey GamepadKey;
		const FString Prompt = DescribePromptAction(Boss->QTE->GetExpectedInput(), &GamepadKey);
		PromptText->SetText(FText::FromString(FString::Printf(TEXT("Round %d/%d  |  %d/3  %s  %.1fs"), Boss->QTE->GetRoundNumber(), Boss->QTE->GetRoundCount(), Boss->QTE->GetPromptNumber(), *Prompt, Boss->QTE->GetSecondsRemaining())));
		FString Glyph;
		FLinearColor Color;
		FVector2D IconSize;
		if (GetXboxPromptIcon(GamepadKey, Glyph, Color, IconSize))
		{
			if (PromptIconText->GetText().ToString() != Glyph)
			{
				PromptIconText->SetText(FText::FromString(Glyph));
				FSlateFontInfo IconFont = PromptIconText->GetFont(); IconFont.Size = IconSize.X > 100.0f ? 40 : 52; PromptIconText->SetFont(IconFont);
				PromptIconText->SetColorAndOpacity(FSlateColor(Glyph == TEXT("Y") || IconSize.X > 100.0f ? FLinearColor::Black : FLinearColor::White));
				PromptIcon->SetBrushColor(Color);
				if (UCanvasPanelSlot* PromptIconSlot = Cast<UCanvasPanelSlot>(PromptIcon->Slot)) PromptIconSlot->SetSize(IconSize);
			}
			bShowPromptIcon = true;
		}
	}
	else if (Boss) PromptText->SetText(FText::FromString(Boss->GetEvolutionCue()));
	else PromptText->SetText(FText::GetEmpty());
	const ESlateVisibility IconVisibility = bShowPromptIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (PromptIcon->GetVisibility() != IconVisibility) PromptIcon->SetVisibility(IconVisibility);
}

int32 UQuasicomboHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 ContentLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);
	UWorld* World = GetWorld();
	const UQuasicomboRunSubsystem* Run = World ? World->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	const FVector2D ViewSize = AllottedGeometry.GetLocalSize();
	if (!Run || ViewSize.X < 590.0f || ViewSize.Y < 340.0f) return ContentLayer;
	const AQuasicomboBoss* Boss = Cast<AQuasicomboBoss>(UGameplayStatics::GetActorOfClass(this, AQuasicomboBoss::StaticClass()));
	if (Boss && Boss->QTE && Boss->QTE->IsQTEActive()) return ContentLayer;

	const float Width = FMath::Min(475.0f, ViewSize.X - 50.0f);
	const float Left = ViewSize.X - Width - 20.0f;
	const float Top = ViewSize.X < 990.0f ? 140.0f : 17.0f;
	const float TrackY[3] = {Top + 39.0f, Top + 68.0f, Top + 97.0f};
	const FLinearColor StrandColors[3] =
	{
		FLinearColor(0.20f, 0.78f, 1.0f),
		FLinearColor(1.0f, 0.82f, 0.24f),
		FLinearColor(0.82f, 0.47f, 1.0f)
	};
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12.0f);
	auto DrawText = [&](const FString& Value, FVector2D Position, FLinearColor Color)
	{
		FSlateDrawElement::MakeText(OutDrawElements, ContentLayer + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(Width, 20.0f), FSlateLayoutTransform(Position)),
			Value, Font, ESlateDrawEffect::None, Color);
	};
	auto DrawLine = [&](float X0, float Y0, float X1, float Y1, const FLinearColor& Color)
	{
		TArray<FVector2f> Points;
		Points.Add(FVector2f(X0, Y0));
		Points.Add(FVector2f(X1, Y1));
		FSlateDrawElement::MakeLines(OutDrawElements, ContentLayer + 1,
			AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, 3.0f);
	};
	for (int32 Position = 0; Position < 3; ++Position)
	{
		DrawText(FString::Printf(TEXT("%c"), TEXT('A') + Position), FVector2D(Left, TrackY[Position] - 10.0f), StrandColors[Position]);
	}
	const float StartX = Left + 26.0f;
	const float EndX = Left + Width - 8.0f;
	const TArray<FQuasicomboBraidOperation> Operations = Run->GetBraidOperations();
	int32 StrandAtPosition[3] = {0, 1, 2};
	const int32 StepCount = FMath::Max(1, Operations.Num());
	for (int32 Step = 0; Step < StepCount; ++Step)
	{
		const float X0 = FMath::Lerp(StartX, EndX, static_cast<float>(Step) / StepCount);
		const float X1 = FMath::Lerp(StartX, EndX, static_cast<float>(Step + 1) / StepCount);
		const FQuasicomboBraidOperation* Operation = Operations.IsValidIndex(Step) ? &Operations[Step] : nullptr;
		const int32 Upper = Operation ? Operation->Generator - 1 : -1;
		if (Operation && (Upper < 0 || Upper > 1)) continue;
		// Draw the lower/under strand with a small break, then the over strand across it.
		for (int32 Position = 0; Position < 3; ++Position)
		{
			if (Operation && (Position == Upper || Position == Upper + 1)) continue;
			DrawLine(X0, TrackY[Position], X1, TrackY[Position], StrandColors[StrandAtPosition[Position]]);
		}
		if (Operation)
		{
			const int32 OverPosition = Operation->Power > 0 ? Upper : Upper + 1;
			const int32 UnderPosition = Operation->Power > 0 ? Upper + 1 : Upper;
			const float MidX = (X0 + X1) * 0.5f;
			const float GapX = FMath::Min(6.0f, (X1 - X0) * 0.13f);
			const float MidY = (TrackY[Upper] + TrackY[Upper + 1]) * 0.5f;
			const int32 UnderTarget = UnderPosition == Upper ? Upper + 1 : Upper;
			const int32 OverTarget = OverPosition == Upper ? Upper + 1 : Upper;
			DrawLine(X0, TrackY[UnderPosition], MidX - GapX,
				FMath::Lerp(TrackY[UnderPosition], MidY, 1.0f - 2.0f * GapX / (X1 - X0)),
				StrandColors[StrandAtPosition[UnderPosition]]);
			DrawLine(MidX + GapX,
				FMath::Lerp(MidY, TrackY[UnderTarget], 2.0f * GapX / (X1 - X0)),
				X1, TrackY[UnderTarget], StrandColors[StrandAtPosition[UnderPosition]]);
			DrawLine(X0, TrackY[OverPosition], X1, TrackY[OverTarget], StrandColors[StrandAtPosition[OverPosition]]);
			DrawText(FString::Printf(TEXT("s%d%c"), Operation->Generator, Operation->Power > 0 ? TEXT('+') : TEXT('-')),
				FVector2D(MidX - 12.0f, Top + 13.0f), FLinearColor::White);
			Swap(StrandAtPosition[Upper], StrandAtPosition[Upper + 1]);
		}
	}
	const FString Probabilities = Boss && Boss->Quantum && Boss->Quantum->HasQuantumState()
		? FString::Printf(TEXT("Vacuum: %.0f%%   Tau: %.0f%%"),
			Boss->Quantum->GetVacuumProbability() * 100.0, Boss->Quantum->GetTauProbability() * 100.0)
		: TEXT("Vacuum: --   Tau: --");
	DrawText(Probabilities, FVector2D(Left, Top + 124.0f), FLinearColor(0.55f, 0.9f, 1.0f));
	FString RouteLetters;
	for (EQuasicomboLane Lane : Run->GetRouteHistory())
	{
		if (!RouteLetters.IsEmpty()) RouteLetters += TEXT(", ");
		RouteLetters.AppendChar(TEXT("ABC")[static_cast<int32>(Lane)]);
	}
	const FString Routes = TEXT("Routes: ") + RouteLetters;
	FString Braid = TEXT("Braid:");
	for (const FQuasicomboBraidOperation& Operation : Operations)
	{
		Braid += FString::Printf(TEXT(" s%d%s"), Operation.Generator, Operation.Power < 0 ? TEXT("-") : TEXT("+"));
	}
	DrawText(Routes + TEXT("     ") + Braid, FVector2D(Left, Top + 146.0f), FLinearColor::White);
	return ContentLayer + 2;
}

AQuasicomboHUDActor::AQuasicomboHUDActor()
{
	static ConstructorHelpers::FObjectFinder<UQuasicomboCreditsData> CreditsFinder(
		TEXT("/Game/UI/DA_QuasicomboCredits.DA_QuasicomboCredits"));
	if (CreditsFinder.Succeeded()) CreditsData = CreditsFinder.Object;
}

void AQuasicomboHUDActor::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		Widget = CreateWidget<UQuasicomboHUDWidget>(PC, UQuasicomboHUDWidget::StaticClass());
		if (Widget) Widget->AddToViewport(5);
	}
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		Run->OnRunEnded.AddDynamic(this, &AQuasicomboHUDActor::HandleRunEnded);
		if (Run->GetOutcome() == EQuasicomboRunOutcome::Victory) HandleRunEnded(EQuasicomboRunOutcome::Victory);
	}
}

void AQuasicomboHUDActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>())
		{
			Run->OnRunEnded.RemoveDynamic(this, &AQuasicomboHUDActor::HandleRunEnded);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AQuasicomboHUDActor::HandleRunEnded(EQuasicomboRunOutcome Outcome)
{
	if (Outcome != EQuasicomboRunOutcome::Victory || VictoryWidget) return;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		VictoryWidget = CreateWidget<UQuasicomboVictoryWidget>(PC, UQuasicomboVictoryWidget::StaticClass());
		if (VictoryWidget)
		{
			VictoryWidget->SetCreditsData(CreditsData);
			VictoryWidget->AddToViewport(10);
		}
	}
}
