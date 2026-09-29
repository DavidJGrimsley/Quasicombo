#include "QuasicomboVictoryWidget.h"
#include "QuasicomboCreditsData.h"
#include "QuasicomboRunSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Name, int32 FontSize,
	const FLinearColor& Color, bool bCenter = true)
{
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetAutoWrapText(true);
	Text->SetJustification(bCenter ? ETextJustify::Center : ETextJustify::Left);
	UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Text);
	Slot->SetPadding(FMargin(0.0f, 5.0f));
	return Text;
}

UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Name, const TCHAR* Label)
{
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
	Button->SetBackgroundColor(FLinearColor(0.12f, 0.30f, 0.43f, 1.0f));
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 20;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text->SetJustification(ETextJustify::Center);
	Text->SetAutoWrapText(true);
	Text->SetText(FText::FromString(Label));
	Button->AddChild(Text);
	UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button);
	Slot->SetPadding(FMargin(0.0f, 6.0f));
	return Button;
}

FString FormatWord(const TArray<FQuasicomboBraidOperation>& Operations)
{
	if (Operations.IsEmpty()) return TEXT("none");
	FString Result;
	for (const FQuasicomboBraidOperation& Operation : Operations)
	{
		if (!Result.IsEmpty()) Result += TEXT("  ");
		Result += FString::Printf(TEXT("s%d%s"), Operation.Generator, Operation.Power < 0 ? TEXT("-") : TEXT("+"));
	}
	return Result;
}

FString FormatBreakdown(const FQuasicomboScoreBreakdown& Score)
{
	return FString::Printf(TEXT("Time +%d    Best Combo +%d\nNet Braid +%d    Quantum +%d"),
		Score.TimePoints, Score.ComboPoints, Score.BraidPoints, Score.QuantumPoints);
}
}

void UQuasicomboVictoryWidget::SetCreditsData(UQuasicomboCreditsData* InCreditsData)
{
	CreditsData = InCreditsData;
	if (CreditsContent) FillCredits();
}

TSharedRef<SWidget> UQuasicomboVictoryWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VictoryBackdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.05f, 0.88f));
		UCanvasPanelSlot* BackdropSlot = Canvas->AddChildToCanvas(Backdrop);
		BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackdropSlot->SetOffsets(FMargin(0.0f));
		auto AddPage = [this, Canvas](const TCHAR* Name, UVerticalBox*& Content) -> UBorder*
		{
			UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(Name));
			Border->SetBrushColor(FLinearColor(0.035f, 0.08f, 0.13f, 0.97f));
			Border->SetPadding(FMargin(18.0f));
			Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Border->AddChild(Content);
			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Border);
			Slot->SetAnchors(FAnchors(0.16f, 0.08f, 0.84f, 0.92f));
			Slot->SetOffsets(FMargin(0.0f));
			return Border;
		};
		UVerticalBox* MenuContent = nullptr;
		MenuPage = AddPage(TEXT("VictoryMenuPage"), MenuContent);
		MenuPage->SetVerticalAlignment(VAlign_Top);
		ScoreText = AddText(WidgetTree, MenuContent, TEXT("VictoryScore"), 32,
			FLinearColor(1.0f, 0.9f, 0.4f));
		UTextBlock* Title = AddText(WidgetTree, MenuContent, TEXT("VictoryTitle"), 38,
			FLinearColor::White);
		Title->SetText(FText::FromString(TEXT("VICTORY")));
		BreakdownText = AddText(WidgetTree, MenuContent, TEXT("VictoryBreakdown"), 18,
			FLinearColor(0.74f, 0.88f, 1.0f));
		InfoButton = AddButton(WidgetTree, MenuContent, TEXT("TechnicalInfoButton"), TEXT("See technical info and credits"));
		ReplayButton = AddButton(WidgetTree, MenuContent, TEXT("ReplayButton"), TEXT("Replay level with Cannon Blaster"));
		QuitButton = AddButton(WidgetTree, MenuContent, TEXT("QuitButton"), TEXT("Quit"));

		UVerticalBox* TechnicalContent = nullptr;
		TechnicalPage = AddPage(TEXT("TechnicalPage"), TechnicalContent);
		UTextBlock* TechnicalTitle = AddText(WidgetTree, TechnicalContent, TEXT("TechnicalTitle"), 32,
			FLinearColor(0.55f, 0.9f, 1.0f));
		TechnicalTitle->SetText(FText::FromString(TEXT("QUANTUM BRAIDING")));
		TechnicalScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TechnicalScroll"));
		TechnicalText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TechnicalBody"));
		FSlateFontInfo TechnicalFont = TechnicalText->GetFont();
		TechnicalFont.Size = 18;
		TechnicalText->SetFont(TechnicalFont);
		TechnicalText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		TechnicalText->SetAutoWrapText(true);
		TechnicalScroll->AddChild(TechnicalText);
		UVerticalBoxSlot* TechnicalSlot = TechnicalContent->AddChildToVerticalBox(TechnicalScroll);
		TechnicalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TechnicalSlot->SetPadding(FMargin(0.0f, 8.0f));
		ContinueButton = AddButton(WidgetTree, TechnicalContent, TEXT("ContinueCreditsButton"), TEXT("Continue to credits"));
		TechnicalBackButton = AddButton(WidgetTree, TechnicalContent, TEXT("TechnicalBackButton"), TEXT("Back to victory"));

		UVerticalBox* CreditsPageContent = nullptr;
		CreditsPage = AddPage(TEXT("CreditsPage"), CreditsPageContent);
		UTextBlock* CreditsTitle = AddText(WidgetTree, CreditsPageContent, TEXT("CreditsTitle"), 34,
			FLinearColor(1.0f, 0.9f, 0.4f));
		CreditsTitle->SetText(FText::FromString(TEXT("CREDITS")));
		CreditsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CreditsScroll"));
		CreditsContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CreditsScroll->AddChild(CreditsContent);
		UVerticalBoxSlot* CreditsSlot = CreditsPageContent->AddChildToVerticalBox(CreditsScroll);
		CreditsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CreditsSlot->SetPadding(FMargin(0.0f, 8.0f));
		CreditsBackButton = AddButton(WidgetTree, CreditsPageContent, TEXT("CreditsBackButton"), TEXT("Back to victory"));
	}
	return Super::RebuildWidget();
}

void UQuasicomboVictoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	InfoButton->OnClicked.AddDynamic(this, &UQuasicomboVictoryWidget::ShowTechnical);
	ReplayButton->OnClicked.AddDynamic(this, &UQuasicomboVictoryWidget::ReplayNoOp);
	QuitButton->OnClicked.AddDynamic(this, &UQuasicomboVictoryWidget::QuitGame);
	ContinueButton->OnClicked.AddDynamic(this, &UQuasicomboVictoryWidget::ShowCredits);
	TechnicalBackButton->OnClicked.AddDynamic(this, &UQuasicomboVictoryWidget::BackToMenu);
	CreditsBackButton->OnClicked.AddDynamic(this, &UQuasicomboVictoryWidget::BackToMenu);
	if (const UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		FillResults(Run);
	}
	FillCredits();
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	ShowPage(EPage::Menu);
}

void UQuasicomboVictoryWidget::FillResults(const UQuasicomboRunSubsystem* Run)
{
	const FQuasicomboScoreBreakdown Score = Run->GetVictoryScoreBreakdown();
	ScoreText->SetText(FText::FromString(FString::Printf(TEXT("SCORE %d"), Score.Total)));
	BreakdownText->SetText(FText::FromString(FormatBreakdown(Score)));
	FString Route;
	for (EQuasicomboLane Lane : Run->GetRouteHistory())
	{
		if (!Route.IsEmpty()) Route += TEXT(", ");
		Route.AppendChar(TEXT('A') + static_cast<int32>(Lane));
	}
	if (Route.IsEmpty()) Route = TEXT("none");
	const TArray<FQuasicomboBraidOperation> RawBraid = Run->GetBraidOperations();
	const TArray<FQuasicomboBraidOperation> NetBraid = UQuasicomboRunSubsystem::ReduceBraid(RawBraid);
	const FString Vacuum = Run->HasFinalQuantumState()
		? FString::Printf(TEXT("%.0f%%"), Run->GetFinalVacuumProbability() * 100.0) : TEXT("--");
	const FString Tau = Run->HasFinalQuantumState()
		? FString::Printf(TEXT("%.0f%%"), Run->GetFinalTauProbability() * 100.0) : TEXT("--");
	const TCHAR* Outcome = Run->GetMeasuredOutcome() == EQuasicomboMeasuredOutcome::Tau ? TEXT("Tau") :
		Run->GetMeasuredOutcome() == EQuasicomboMeasuredOutcome::Vacuum ? TEXT("Vacuum") : TEXT("Unavailable");
	const FString Source = Run->HasFinalQuantumState()
		? (Run->UsedQuantumFallback() ? TEXT("validated route fixture fallback") : TEXT("Quantum API"))
		: TEXT("unavailable");
	const FString Details = FString::Printf(
		TEXT("Quantum braiding comes from topological quantum computing: exchanging special particles called anyons in different orders can change their shared quantum state. This game models the exchanges of three Fibonacci anyons with colored route strands, and your crossings set the Vacuum and Tau probabilities that shape the boss.\n\n"
		"HOW THE GAME'S BRAID WORKS\n"
		"You begin on strand B and choose A, B, or C at each of three junctions. Each time two strands trade places, we record which pair crossed (s1 for top and middle, s2 for middle and bottom) and its direction (+ or -). The second junction reverses that direction. The crossings must be read in order because changing their order can change the resulting state.\n\n"
		"The resulting Vacuum and Tau probabilities tune the boss. More Tau makes the boss quicker and more aggressive. Highest combo and damage taken also influence the boss's time evolution. At the finisher, the game samples Vacuum or Tau from the final state to choose the QTE pattern.\n\n"
		"THIS RUN\n"
		"Route: %s\nRecorded braid: %s\nReduced braid: %s (%d net crossings)\n"
		"Final Vacuum: %s    Final Tau: %s\nQuantum state source: %s\nSampled finisher: %s\n\n"
		"SCORE\nElapsed: %.1fs    Best Combo: %d\n"
		"Time: +%d    Combo: +%d\nNet braid: +%d    Quantum: +%d\nTOTAL: %d\n\n"
		"Score uses the reduced braid after adjacent opposite crossings cancel. The recorded braid is still used for the quantum calculation. A Tau finisher adds 2,000 points; Vacuum adds none."),
		*Route, *FormatWord(RawBraid), *FormatWord(NetBraid), Score.NetCrossings,
		*Vacuum, *Tau, *Source, Outcome, Run->GetElapsedSeconds(), Run->GetHighestCombo(),
		Score.TimePoints, Score.ComboPoints, Score.BraidPoints, Score.QuantumPoints, Score.Total);
	TechnicalText->SetText(FText::FromString(Details));
}

void UQuasicomboVictoryWidget::FillCredits()
{
	if (!CreditsContent) return;
	CreditsContent->ClearChildren();
	USpacer* StartSpace = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	StartSpace->SetSize(FVector2D(1.0f, 500.0f));
	CreditsContent->AddChildToVerticalBox(StartSpace);
	if (CreditsData && !CreditsData->Lines.IsEmpty())
	{
		for (int32 Index = 0; Index < CreditsData->Lines.Num(); ++Index)
		{
			const FString Name = FString::Printf(TEXT("CreditLine_%d"), Index);
			UTextBlock* Credit = AddText(WidgetTree, CreditsContent, *Name, 24, FLinearColor::White);
			Credit->SetText(CreditsData->Lines[Index]);
		}
	}
	else
	{
		UTextBlock* Missing = AddText(WidgetTree, CreditsContent, TEXT("CreditsUnavailable"), 22, FLinearColor::White);
		Missing->SetText(FText::FromString(TEXT("Credits unavailable")));
	}
	USpacer* EndSpace = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	EndSpace->SetSize(FVector2D(1.0f, 500.0f));
	CreditsContent->AddChildToVerticalBox(EndSpace);
}

void UQuasicomboVictoryWidget::ShowPage(EPage NewPage)
{
	Page = NewPage;
	MenuPage->SetVisibility(Page == EPage::Menu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	TechnicalPage->SetVisibility(Page == EPage::Technical ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CreditsPage->SetVisibility(Page == EPage::Credits ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	bPendingFocus = true;
	if (Page == EPage::Menu)
	{
		MenuSelection = 0;
	}
	else if (Page == EPage::Technical)
	{
		TechnicalScroll->SetScrollOffset(0.0f);
	}
	else
	{
		CreditsEndWait = 0.0f;
		CreditsScroll->SetScrollOffset(0.0f);
	}
}

void UQuasicomboVictoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bPendingFocus)
	{
		bPendingFocus = false;
		if (Page == EPage::Menu) InfoButton->SetKeyboardFocus();
		else if (Page == EPage::Technical) ContinueButton->SetKeyboardFocus();
		else CreditsBackButton->SetKeyboardFocus();
	}
	if (Page != EPage::Credits || !CreditsScroll) return;
	const float End = CreditsScroll->GetScrollOffsetOfEnd();
	if (End <= 0.0f) return;
	const float Next = FMath::Min(End, CreditsScroll->GetScrollOffset() + 80.0f * InDeltaTime);
	CreditsScroll->SetScrollOffset(Next);
	if (Next >= End - 1.0f)
	{
		CreditsEndWait += InDeltaTime;
		if (CreditsEndWait >= 2.0f) ShowPage(EPage::Menu);
	}
}

FReply UQuasicomboVictoryWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (Page != EPage::Menu) BackToMenu();
		return FReply::Handled();
	}
	const bool bUp = Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up;
	const bool bDown = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down;
	if (bUp || bDown)
	{
		if (Page == EPage::Menu)
		{
			MenuSelection = (MenuSelection + (bDown ? 1 : 2)) % 3;
			UButton* Buttons[] = {InfoButton, ReplayButton, QuitButton};
			Buttons[MenuSelection]->SetKeyboardFocus();
		}
		else if (Page == EPage::Technical)
		{
			TechnicalScroll->SetScrollOffset(FMath::Max(0.0f,
				TechnicalScroll->GetScrollOffset() + (bDown ? 75.0f : -75.0f)));
		}
		else
		{
			CreditsScroll->SetScrollOffset(FMath::Max(0.0f,
				CreditsScroll->GetScrollOffset() + (bDown ? 75.0f : -75.0f)));
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UQuasicomboVictoryWidget::ShowTechnical() { ShowPage(EPage::Technical); }
void UQuasicomboVictoryWidget::ReplayNoOp() { /* Replay becomes available with the Cannon Blaster feature. */ }
void UQuasicomboVictoryWidget::ShowCredits() { ShowPage(EPage::Credits); }
void UQuasicomboVictoryWidget::BackToMenu() { ShowPage(EPage::Menu); }

void UQuasicomboVictoryWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
