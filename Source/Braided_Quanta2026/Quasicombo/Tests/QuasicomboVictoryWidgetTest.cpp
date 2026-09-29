#if WITH_DEV_AUTOMATION_TESTS

#include "QuasicomboCreditsData.h"
#include "QuasicomboVictoryWidget.h"
#include "QuasicomboRunSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboVictoryWidgetTest, "Quasicombo.UI.VictoryFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboVictoryWidgetTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld())
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	UQuasicomboCreditsData* Credits = LoadObject<UQuasicomboCreditsData>(nullptr,
		TEXT("/Game/UI/DA_QuasicomboCredits.DA_QuasicomboCredits"));
	if (!TestNotNull(TEXT("Run subsystem"), Run) ||
		!TestNotNull(TEXT("Cooked credits asset"), Credits))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	TestEqual(TEXT("Current credits have three entries"), Credits->Lines.Num(), 3);
	Run->RecordFinisherMeasurement(true, true, 0.25, 0.75, false);
	Run->EndRun(EQuasicomboRunOutcome::Victory);
	const int32 FrozenScore = Run->GetVictoryScore();
	Run->RecordCombo(99);
	TestEqual(TEXT("Score stays frozen after victory"), Run->GetVictoryScore(), FrozenScore);
	TestEqual(TEXT("Best combo stays frozen after victory"), Run->GetHighestCombo(), 0);
	UQuasicomboVictoryWidget* Widget = NewObject<UQuasicomboVictoryWidget>(World);
	if (!TestNotNull(TEXT("Victory widget created"), Widget))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	Widget->SetCreditsData(Credits);
	TestTrue(TEXT("Victory widget initialized"), Widget->Initialize());
	Widget->TakeWidget();
	UBorder* Menu = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("VictoryMenuPage")));
	UBorder* Technical = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("TechnicalPage")));
	UBorder* CreditsPage = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("CreditsPage")));
	UButton* Info = Cast<UButton>(Widget->GetWidgetFromName(TEXT("TechnicalInfoButton")));
	UButton* Continue = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ContinueCreditsButton")));
	UButton* Back = Cast<UButton>(Widget->GetWidgetFromName(TEXT("CreditsBackButton")));
	UButton* Replay = Cast<UButton>(Widget->GetWidgetFromName(TEXT("ReplayButton")));
	UTextBlock* Total = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("VictoryScore")));
	if (!TestNotNull(TEXT("Menu page"), Menu) || !TestNotNull(TEXT("Technical page"), Technical) ||
		!TestNotNull(TEXT("Credits page"), CreditsPage) || !TestNotNull(TEXT("Info button"), Info) ||
		!TestNotNull(TEXT("Continue button"), Continue) || !TestNotNull(TEXT("Back button"), Back) ||
		!TestNotNull(TEXT("Replay button"), Replay) || !TestNotNull(TEXT("Score text"), Total))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	TestTrue(TEXT("Frozen score is displayed"), Total->GetText().ToString().Contains(TEXT("SCORE ")));
	TestEqual(TEXT("Menu starts visible"), Menu->GetVisibility(), ESlateVisibility::Visible);
	Replay->OnClicked.Broadcast();
	TestEqual(TEXT("Replay is an intentional no-op"), Menu->GetVisibility(), ESlateVisibility::Visible);
	Info->OnClicked.Broadcast();
	TestEqual(TEXT("Info opens the technical page"), Technical->GetVisibility(), ESlateVisibility::Visible);
	Continue->OnClicked.Broadcast();
	TestEqual(TEXT("Continue starts credits"), CreditsPage->GetVisibility(), ESlateVisibility::Visible);
	Back->OnClicked.Broadcast();
	TestEqual(TEXT("Credits return to victory"), Menu->GetVisibility(), ESlateVisibility::Visible);
	Widget->RemoveFromParent();
	Fixture.ForwardErrorMessages(this);
	return true;
}

#endif
