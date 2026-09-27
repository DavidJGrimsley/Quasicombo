#if WITH_DEV_AUTOMATION_TESTS

#include "QuasicomboQTESequence.h"
#include "QuasicomboQTEComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTESequenceTest,
	"Quasicombo.QTE.PromptSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboQTESequenceTest::RunTest(const FString& Parameters)
{
	const float BlockoutWindow = GetDefault<UQuasicomboQTEComponent>()->PromptWindowSeconds;
	TestEqual(TEXT("Playable blockout allows four seconds per prompt"), BlockoutWindow, 4.0f);
	FQuasicomboQTESequence Easy;
	Easy.Begin(false, 0.0, BlockoutWindow);
	TestEqual(TEXT("Easy prompt is still open just before four seconds"), Easy.Advance(3.9, false),
		EQuasicomboQTEProgress::None);
	TestEqual(TEXT("A late but valid press advances and gets a fresh window"), Easy.Submit(EQuasicomboQTEInput::Attack, 3.95, false),
		EQuasicomboQTEProgress::Advanced);
	TestTrue(TEXT("Second prompt has its own four-second window"), Easy.GetSecondsRemaining(7.9, false) > 0.0);

	FQuasicomboQTESequence Vacuum;
	Vacuum.Begin(false, 100.0, 0.9);
	TestEqual(TEXT("Vacuum first input is Attack"), Vacuum.GetExpectedInput(), EQuasicomboQTEInput::Attack);
	TestEqual(TEXT("First fresh press advances"), Vacuum.Submit(EQuasicomboQTEInput::Attack, 100.2, false),
		EQuasicomboQTEProgress::Advanced);
	TestEqual(TEXT("Vacuum second input is Dash"), Vacuum.GetExpectedInput(), EQuasicomboQTEInput::Dash);
	TestEqual(TEXT("Second fresh press advances"), Vacuum.Submit(EQuasicomboQTEInput::Dash, 100.5, false),
		EQuasicomboQTEProgress::Advanced);
	TestEqual(TEXT("Vacuum final input is Attack"), Vacuum.GetExpectedInput(), EQuasicomboQTEInput::Attack);
	TestEqual(TEXT("Three correct presses succeed"), Vacuum.Submit(EQuasicomboQTEInput::Attack, 101.1, false),
		EQuasicomboQTEProgress::Succeeded);
	TestFalse(TEXT("Success ends input ownership"), Vacuum.IsActive());
	TestEqual(TEXT("A fourth press cannot retrigger success"), Vacuum.Submit(EQuasicomboQTEInput::Attack, 101.2, false),
		EQuasicomboQTEProgress::None);

	FQuasicomboQTESequence Tau;
	Tau.Begin(true, 200.0, 0.9);
	TestEqual(TEXT("Tau first input is Dash"), Tau.GetExpectedInput(), EQuasicomboQTEInput::Dash);
	TestEqual(TEXT("Wrong input fails immediately"), Tau.Submit(EQuasicomboQTEInput::Attack, 200.1, false),
		EQuasicomboQTEProgress::Failed);
	TestFalse(TEXT("Failure ends input ownership"), Tau.IsActive());
	Tau.Begin(true, 210.0, 0.9);
	TestEqual(TEXT("Tau Dash advances"), Tau.Submit(EQuasicomboQTEInput::Dash, 210.1, false),
		EQuasicomboQTEProgress::Advanced);
	TestEqual(TEXT("Tau second input is Jump"), Tau.GetExpectedInput(), EQuasicomboQTEInput::Jump);
	TestEqual(TEXT("Tau Jump advances"), Tau.Submit(EQuasicomboQTEInput::Jump, 210.2, false),
		EQuasicomboQTEProgress::Advanced);
	TestEqual(TEXT("Tau final input is Attack"), Tau.GetExpectedInput(), EQuasicomboQTEInput::Attack);
	TestEqual(TEXT("Tau pattern succeeds"), Tau.Submit(EQuasicomboQTEInput::Attack, 210.3, false),
		EQuasicomboQTEProgress::Succeeded);

	FQuasicomboQTESequence Timeout;
	Timeout.Begin(false, 300.0, 0.9);
	TestEqual(TEXT("Prompt survives before deadline"), Timeout.Advance(300.89, false), EQuasicomboQTEProgress::None);
	TestEqual(TEXT("Prompt fails after deadline"), Timeout.Advance(300.91, false), EQuasicomboQTEProgress::Failed);
	Timeout.Begin(false, 310.0, 0.9);
	TestEqual(TEXT("Late correct input still fails"), Timeout.Submit(EQuasicomboQTEInput::Attack, 310.91, false),
		EQuasicomboQTEProgress::Failed);

	FQuasicomboQTESequence Paused;
	Paused.Begin(false, 400.0, 0.9);
	Paused.Advance(400.3, false);
	TestEqual(TEXT("Paused input is consumed without advancing"), Paused.Submit(EQuasicomboQTEInput::Attack, 420.3, true),
		EQuasicomboQTEProgress::None);
	TestTrue(TEXT("Pause preserves prompt"), Paused.IsActive());
	TestTrue(TEXT("Pause preserves remaining time"),
		FMath::IsNearlyEqual(Paused.GetSecondsRemaining(420.3, true), 0.6, 1e-9));
	TestEqual(TEXT("After unpause remaining time advances normally"), Paused.Advance(420.89, false),
		EQuasicomboQTEProgress::None);
	TestEqual(TEXT("After unpause deadline still applies"), Paused.Advance(420.91, false),
		EQuasicomboQTEProgress::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEInterruptionTest,
	"Quasicombo.QTE.Interruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboQTEInterruptionTest::RunTest(const FString& Parameters)
{
	UQuasicomboQTEComponent* Component = NewObject<UQuasicomboQTEComponent>();
	Component->Sequence.Begin(false, 100.0, 0.9);
	TestTrue(TEXT("Prompt owns input before external defeat"), Component->IsQTEActive());
	Component->HandleRunEnded(EQuasicomboRunOutcome::Defeat);
	TestFalse(TEXT("External defeat cancels prompt immediately"), Component->IsQTEActive());
	TestTrue(TEXT("Interruption marks result delivered"), Component->bResultDelivered);
	TestFalse(TEXT("No later prompt can deliver victory"), Component->SubmitPrompt(EQuasicomboQTEInput::Attack));
	Component->bResultDelivered = false;
	Component->bFinishingBeat = true;
	Component->HandleRunEnded(EQuasicomboRunOutcome::Defeat);
	TestFalse(TEXT("External defeat also cancels finishing beat"), Component->IsQTEActive());
	TestTrue(TEXT("Finishing beat cannot later deliver victory"), Component->bResultDelivered);
	return true;
}

#endif
