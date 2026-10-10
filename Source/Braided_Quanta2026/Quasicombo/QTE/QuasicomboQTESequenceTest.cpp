#if WITH_DEV_AUTOMATION_TESTS
#include "QuasicomboQTESequence.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboQTERules.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTESequenceTest,"Quasicombo.QTE.PromptSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTESequenceTest::RunTest(const FString&)
{
    TestEqual(TEXT("Real-time prompt window"),GetDefault<UQuasicomboQTEComponent>()->PromptWindowSeconds,1.5f);
    for(bool bTau : {false,true}) for(int32 Count=2;Count<=6;++Count)
    {
        FQuasicomboQTESequence S; S.Begin(bTau,100,1.5,3,Count,2);
        for(int32 Beat=1;Beat<=Count;++Beat)
        {
            const double Now=100+Beat*.1;
            TestEqual(TEXT("One-based beat"),S.GetPromptNumber(),Beat);
            TestEqual(TEXT("One round per combat depletion"),S.GetRoundNumber(),2);
            if(Beat==1) TestEqual(TEXT("First input is jump in both families"),S.GetExpectedInput(),EQuasicomboQTEInput::Jump);
            if(Beat==2) TestEqual(TEXT("Second input is the uppercut attack"),S.GetExpectedInput(),EQuasicomboQTEInput::Attack);
            if(Beat==3) TestEqual(TEXT("Chomp always uses Y"),S.GetExpectedInput(),EQuasicomboQTEInput::Chomp);
            if(Beat==4) TestEqual(TEXT("Claw always uses RT"),S.GetExpectedInput(),EQuasicomboQTEInput::Claw);
            if(Beat==5) TestEqual(TEXT("Final uppercut uses B"),S.GetExpectedInput(),EQuasicomboQTEInput::Attack);
            if(Beat==6)
            {
                TestEqual(TEXT("Cannon starts holding"),S.Submit(EQuasicomboQTEInput::Fire,Now,false),EQuasicomboQTEProgress::Charging);
                TestEqual(TEXT("Repeated fire adds no beat"),S.Submit(EQuasicomboQTEInput::Fire,Now+.3,false),EQuasicomboQTEProgress::None);
                TestEqual(TEXT("Charged release completes"),S.Submit(EQuasicomboQTEInput::Fire,Now+1.1,false,EQuasicomboQTEInputEdge::Release),EQuasicomboQTEProgress::Succeeded);
            }
            else TestEqual(TEXT("Selected prefix ends on its last move"),S.Submit(S.GetExpectedInput(),Now,false),
                Beat==Count?EQuasicomboQTEProgress::Succeeded:EQuasicomboQTEProgress::Advanced);
        }
        TestFalse(TEXT("No automatic extra move"),S.IsActive());
    }
    FQuasicomboQTESequence S; S.Begin(false,0,1.5);
    TestEqual(TEXT("Attack before takeoff fails"),S.Submit(EQuasicomboQTEInput::Attack,.1,false),EQuasicomboQTEProgress::Failed);
    S.Begin(false,0,1.5); S.Hold(.1);
    TestEqual(TEXT("Animation hold protects deadline"),S.Advance(10,false),EQuasicomboQTEProgress::None);
    TestEqual(TEXT("Mashing during action cannot advance"),S.Submit(EQuasicomboQTEInput::Attack,10.1,false),EQuasicomboQTEProgress::None);
    S.OpenNextPrompt(11);
    TestEqual(TEXT("Transition opens a full new window"),S.GetSecondsRemaining(11,false),1.5);
    TestEqual(TEXT("Prompt timeout"),S.Advance(12.51,false),EQuasicomboQTEProgress::Failed);
    S.Begin(false,0,1.5); S.Advance(.3,false); S.Advance(20.3,true);
    TestTrue(TEXT("Pause retains remaining window"),FMath::IsNearlyEqual(S.GetSecondsRemaining(20.3,true),1.2,1e-9));
    TestEqual(TEXT("Resume timeout"),S.Advance(21.51,false),EQuasicomboQTEProgress::Failed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTERulesTest,"Quasicombo.QTE.ProgressiveRounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTERulesTest::RunTest(const FString&)
{
    TestEqual(TEXT("Vacuum minimum"),QuasicomboQTERules::FinalPromptCount(0,false),2);
    TestEqual(TEXT("No cannon maximum"),QuasicomboQTERules::FinalPromptCount(1,false),5);
    TestEqual(TEXT("Cannon maximum"),QuasicomboQTERules::FinalPromptCount(1,true),6);
    TestEqual(TEXT("Balanced without cannon"),QuasicomboQTERules::FinalPromptCount(.5,false),4);
    for(int32 Final=2;Final<=6;++Final) for(int32 Rounds=1;Rounds<=3;++Rounds)
        for(int32 Round=1;Round<=Rounds;++Round)
            TestEqual(TEXT("Progressive round length"),QuasicomboQTERules::RoundPromptCount(Final,Round,Rounds),FMath::Max(2,Final-(Rounds-Round)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEChargeTest,"Quasicombo.QTE.ChargeBoundaries",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEChargeTest::RunTest(const FString&)
{
    auto Cannon=[]()
    {
        FQuasicomboQTESequence S; S.Begin(false,100,1.5,1,6);
        for(int32 I=0;I<5;++I) S.Submit(S.GetExpectedInput(),100+I*.1,false);
        return S;
    };
    auto S=Cannon();
    TestEqual(TEXT("Must begin hold before 1.5 seconds"),S.Submit(EQuasicomboQTEInput::Fire,101.901,false),EQuasicomboQTEProgress::Failed);
    S=Cannon(); S.Submit(EQuasicomboQTEInput::Fire,100.5,false);
    TestEqual(TEXT("Early release fails"),S.Submit(EQuasicomboQTEInput::Fire,101.499,false,EQuasicomboQTEInputEdge::Release),EQuasicomboQTEProgress::Failed);
    S=Cannon(); S.Submit(EQuasicomboQTEInput::Fire,100.5,false);
    TestEqual(TEXT("Exact one-second release succeeds"),S.Submit(EQuasicomboQTEInput::Fire,101.5,false,EQuasicomboQTEInputEdge::Release),EQuasicomboQTEProgress::Succeeded);
    S=Cannon(); S.Submit(EQuasicomboQTEInput::Fire,100.5,false);
    TestEqual(TEXT("The extended release window accepts a later release"),S.Submit(EQuasicomboQTEInput::Fire,102.6,false,EQuasicomboQTEInputEdge::Release),EQuasicomboQTEProgress::Succeeded);
    S=Cannon(); S.Submit(EQuasicomboQTEInput::Fire,100.5,false);
    TestEqual(TEXT("Release window expires at 2.2 seconds"),S.Submit(EQuasicomboQTEInput::Fire,102.701,false,EQuasicomboQTEInputEdge::Release),EQuasicomboQTEProgress::Failed);
    S=Cannon(); S.Submit(EQuasicomboQTEInput::Fire,100.5,false);
    TestEqual(TEXT("Cancellation fails"),S.Submit(EQuasicomboQTEInput::Fire,100.6,false,EQuasicomboQTEInputEdge::Cancel),EQuasicomboQTEProgress::Failed);
    S=Cannon(); S.Submit(EQuasicomboQTEInput::Fire,100.5,false); S.Advance(100.8,false); S.Advance(120.8,true);
    TestTrue(TEXT("Pause freezes charge progress"),FMath::IsNearlyEqual(S.GetChargeSeconds(120.8,true),.3,1e-9));
    TestEqual(TEXT("Release works after resumed charge"),S.Submit(EQuasicomboQTEInput::Fire,121.6,false,EQuasicomboQTEInputEdge::Release),EQuasicomboQTEProgress::Succeeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuasicomboQTEInterruptionTest,"Quasicombo.QTE.Interruption",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FQuasicomboQTEInterruptionTest::RunTest(const FString&)
{
    auto* C=NewObject<UQuasicomboQTEComponent>();
    C->Sequence.Begin(false,100,1.5); C->bOwningInput=true;
    TestTrue(TEXT("Owns input before interruption"),C->IsQTEActive());
    C->HandleRunEnded(EQuasicomboRunOutcome::Defeat);
    TestFalse(TEXT("Interruption cancels input immediately"),C->IsQTEActive());
    TestTrue(TEXT("Cannot deliver late result"),C->bResultDelivered);
    TestFalse(TEXT("Later press cannot win"),C->SubmitPrompt(EQuasicomboQTEInput::Attack));
    return true;
}
#endif
