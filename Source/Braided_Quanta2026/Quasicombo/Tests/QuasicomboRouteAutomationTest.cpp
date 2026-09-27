#if WITH_DEV_AUTOMATION_TESTS

#include "Quasicombo/Route/QuasicomboRunSubsystem.h"
#include "Misc/AutomationTest.h"

namespace
{
// Reference table for the design contract, indexed by starting and ending lane.
// Its explicit words deliberately do not reuse the subsystem's traversal algorithm.
const TCHAR* const ForwardTransitions[3][3] =
{
	{TEXT(""), TEXT("1+"), TEXT("1+2+")},
	{TEXT("1-"), TEXT(""), TEXT("2+")},
	{TEXT("2-1-"), TEXT("2-"), TEXT("")}
};

FString ExpectedWord(const TArray<EQuasicomboLane>& History)
{
	FString Result;
	int32 Previous = static_cast<int32>(EQuasicomboLane::B);
	for (int32 Index = 0; Index < History.Num(); ++Index)
	{
		const int32 Next = static_cast<int32>(History[Index]);
		FString Transition(ForwardTransitions[Previous][Next]);
		if (Index == 1)
		{
			Transition.ReplaceInline(TEXT("+"), TEXT("#"));
			Transition.ReplaceInline(TEXT("-"), TEXT("+"));
			Transition.ReplaceInline(TEXT("#"), TEXT("-"));
		}
		Result += Transition;
		Previous = Next;
	}
	return Result;
}

FString ActualWord(const TArray<FQuasicomboBraidOperation>& Operations)
{
	FString Result;
	for (const FQuasicomboBraidOperation& Operation : Operations)
	{
		Result += FString::Printf(TEXT("%d%c"), Operation.Generator, Operation.Power > 0 ? TEXT('+') : TEXT('-'));
	}
	return Result;
}

FString RouteLabel(const TArray<EQuasicomboLane>& History)
{
	FString Result;
	for (EQuasicomboLane Lane : History)
	{
		Result.AppendChar(TEXT('A') + static_cast<int32>(Lane));
	}
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboRouteHistoryTest,
	"Quasicombo.Route.AllHistories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboRouteHistoryTest::RunTest(const FString& Parameters)
{
	int32 Completed = 0;
	for (int32 First = 0; First < 3; ++First)
	{
		for (int32 Second = 0; Second < 3; ++Second)
		{
			for (int32 Third = 0; Third < 3; ++Third)
			{
				UQuasicomboRunSubsystem* Run = NewObject<UQuasicomboRunSubsystem>();
				const EQuasicomboLane Lanes[] =
				{
					static_cast<EQuasicomboLane>(First),
					static_cast<EQuasicomboLane>(Second),
					static_cast<EQuasicomboLane>(Third)
				};
				TArray<EQuasicomboLane> Prefix;
				for (int32 Section = 1; Section <= 3; ++Section)
				{
					const EQuasicomboLane Lane = Lanes[Section - 1];
					const FString Before = RouteLabel(Prefix);
					TestFalse(FString::Printf(TEXT("%s cannot skip ahead to section %d"), *Before, Section + 1),
						Run->TryCommitRoute(Section + 1, Lane));
					TestTrue(FString::Printf(TEXT("%s section %d commits"), *Before, Section),
						Run->TryCommitRoute(Section, Lane));
					Prefix.Add(Lane);
					const FString Label = RouteLabel(Prefix);
					TestEqual(FString::Printf(TEXT("%s section count"), *Label), Run->GetCurrentSection(), Section);
					TestTrue(FString::Printf(TEXT("%s route history"), *Label), Run->GetRouteHistory() == Prefix);
					TestEqual(FString::Printf(TEXT("%s braid word"), *Label), ActualWord(Run->GetBraidOperations()), ExpectedWord(Prefix));
					TestFalse(FString::Printf(TEXT("%s duplicate overlap rejected"), *Label), Run->TryCommitRoute(Section, Lane));
					TestEqual(FString::Printf(TEXT("%s duplicate does not append crossings"), *Label),
						ActualWord(Run->GetBraidOperations()), ExpectedWord(Prefix));
				}
				TestFalse(FString::Printf(TEXT("%s cannot add a fourth choice"), *RouteLabel(Prefix)),
					Run->TryCommitRoute(4, EQuasicomboLane::B));
				++Completed;
			}
		}
	}
	TestEqual(TEXT("Every three-route history was checked"), Completed, 27);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQuasicomboRouteGuardTest,
	"Quasicombo.Route.GuardsAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FQuasicomboRouteGuardTest::RunTest(const FString& Parameters)
{
	UQuasicomboRunSubsystem* Run = NewObject<UQuasicomboRunSubsystem>();
	TestFalse(TEXT("Section zero rejected"), Run->TryCommitRoute(0, EQuasicomboLane::B));
	TestFalse(TEXT("Invalid lane rejected"), Run->TryCommitRoute(1, static_cast<EQuasicomboLane>(255)));
	TestEqual(TEXT("Invalid choices leave route empty"), Run->GetCurrentSection(), 0);
	TestTrue(TEXT("A1 accepted"), Run->TryCommitRoute(1, EQuasicomboLane::A));
	TestFalse(TEXT("Another section-one lane rejected"), Run->TryCommitRoute(1, EQuasicomboLane::C));
	TestTrue(TEXT("A2 repetition accepted"), Run->TryCommitRoute(2, EQuasicomboLane::A));
	TestTrue(TEXT("B3 accepted"), Run->TryCommitRoute(3, EQuasicomboLane::B));
	TestEqual(TEXT("Opposite g1 crossings remain chronological"), ActualWord(Run->GetBraidOperations()), FString(TEXT("1-1+")));
	TestEqual(TEXT("Repeated lane remains in history"), RouteLabel(Run->GetRouteHistory()), FString(TEXT("AAB")));

	UQuasicomboRunSubsystem* Other = NewObject<UQuasicomboRunSubsystem>();
	Other->TryCommitRoute(1, EQuasicomboLane::C);
	Other->TryCommitRoute(2, EQuasicomboLane::C);
	Other->TryCommitRoute(3, EQuasicomboLane::B);
	TestEqual(TEXT("Opposite g2 crossings remain chronological"), ActualWord(Other->GetBraidOperations()), FString(TEXT("2+2-")));

	UQuasicomboRunSubsystem* Repeated = NewObject<UQuasicomboRunSubsystem>();
	for (int32 Section = 1; Section <= 3; ++Section) Repeated->TryCommitRoute(Section, EQuasicomboLane::B);
	TestEqual(TEXT("BBB has no crossings but three committed choices"), Repeated->GetBraidOperations().Num(), 0);
	TestEqual(TEXT("BBB completes the three sections"), Repeated->GetCurrentSection(), 3);
	return true;
}

#endif
