#if WITH_DEV_AUTOMATION_TESTS

#include "QuasicomboBoss.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboRunSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboBossFinisherTest,
	"Quasicombo.Boss.SixStrikesToVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FQuasicomboBossFinisherTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game) || !TestWorld.BeginPlayInTestWorld())
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	AQuasicomboBoss* Boss = World->SpawnActor<AQuasicomboBoss>();
	UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>();
	if (!TestNotNull(TEXT("Boss spawned"), Boss) || !TestNotNull(TEXT("Run subsystem exists"), Run))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	const float InitialDilation = UGameplayStatics::GetGlobalTimeDilation(Boss);
	APlayerController* PC = UGameplayStatics::GetPlayerController(Boss, 0);
	AActor* InitialViewTarget = PC ? PC->GetViewTarget() : nullptr;
	Boss->QTE->SuccessBeatSeconds = 0.0f;
	TestEqual(TEXT("Boss requires six strikes"), Boss->RequiredHits, 6);
	for (int32 Strike = 1; Strike <= 5; ++Strike)
	{
		Boss->ApplyDamage(20.0f, nullptr, Boss->GetActorLocation(), FVector::ZeroVector);
		TestEqual(FString::Printf(TEXT("Accepted strike %d"), Strike), Boss->GetAcceptedStrikes(), Strike);
		TestFalse(FString::Printf(TEXT("Strike %d does not start QTE"), Strike), Boss->QTE->IsQTEActive());
	}
	Boss->ApplyDamage(20.0f, nullptr, Boss->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Sixth strike is accepted"), Boss->GetAcceptedStrikes(), 6);
	TestTrue(TEXT("Sixth strike begins QTE"), Boss->QTE->IsQTEActive());
	TestEqual(TEXT("Sixth strike clears boss health"), Boss->CurrentHP, 0.0f);
	Boss->ApplyDamage(20.0f, nullptr, Boss->GetActorLocation(), FVector::ZeroVector);
	TestEqual(TEXT("Further overlaps do not add strikes"), Boss->GetAcceptedStrikes(), 6);
	// No route choices were made, so the deterministic vacuum state selects Attack-Dash-Attack.
	TestEqual(TEXT("Vacuum pattern begins with Attack"), Boss->QTE->GetExpectedInput(), EQuasicomboQTEInput::Attack);
	TestTrue(TEXT("First prompt consumed"), Boss->SubmitQTEInput(EQuasicomboQTEInput::Attack));
	TestTrue(TEXT("Second prompt consumed"), Boss->SubmitQTEInput(EQuasicomboQTEInput::Dash));
	TestTrue(TEXT("Third prompt consumed"), Boss->SubmitQTEInput(EQuasicomboQTEInput::Attack));
	TestEqual(TEXT("Correct QTE ends run in victory"), Run->GetOutcome(), EQuasicomboRunOutcome::Victory);
	TestFalse(TEXT("QTE no longer owns input"), Boss->QTE->IsQTEActive());
	TestTrue(TEXT("Time dilation is restored"),
		FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(Boss), InitialDilation));
	if (PC && InitialViewTarget)
	{
		TestEqual(TEXT("Prior camera view target is restored"), PC->GetViewTarget(), InitialViewTarget);
	}
	TestWorld.ForwardErrorMessages(this);
	return true;
}

#endif
