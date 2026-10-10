#pragma once
#include "CoreMinimal.h"
#include "QuasicomboQTETypes.h"

enum class EQuasicomboQTEProgress : uint8 { None, Advanced, Succeeded, Failed, Charging };

/** One combat round. Real-time clocks stop for animation holds and game pause. */
class FQuasicomboQTESequence
{
public:
    static constexpr double ChargeHoldSeconds=1.0;
    static constexpr double ReleaseWindowSeconds=1.2;
    void Begin(bool bTau, double Now, double Window, int32 Rounds=1, int32 Count=3, int32 Round=1)
    {
        bActive=true; bTauPattern=bTau; bCharging=bHeld=false; PromptIndex=0;
        PromptCount=FMath::Clamp(Count,2,6); RoundCount=FMath::Clamp(Rounds,1,3);
        RoundNumber=FMath::Clamp(Round,1,RoundCount);
        WindowSeconds=FMath::Max(0.1,Window); Deadline=Now+WindowSeconds; LastClockSample=Now;
    }
    void Cancel() { bActive=bCharging=bHeld=false; }
    bool IsActive() const { return bActive; }
    bool IsCharging() const { return bCharging; }
    bool IsHeld() const { return bHeld; }
    int32 GetPromptNumber() const { return FMath::Min(PromptIndex+1,PromptCount); }
    int32 GetPromptCount() const { return PromptCount; }
    int32 GetRoundNumber() const { return RoundNumber; }
    int32 GetRoundCount() const { return RoundCount; }
    EQuasicomboQTEInput GetExpectedInput() const
    {
        if (!bActive) return EQuasicomboQTEInput::Invalid;
        // Quantum selection still chooses rounds/counts; both outcomes use the
        // actual action buttons: A, B, Y, RT, B, X.
        const EQuasicomboQTEInput Inputs[]={EQuasicomboQTEInput::Jump,EQuasicomboQTEInput::Attack,
            EQuasicomboQTEInput::Chomp,EQuasicomboQTEInput::Claw,EQuasicomboQTEInput::Attack,EQuasicomboQTEInput::Fire};
        return Inputs[PromptIndex];
    }
    double GetSecondsRemaining(double Now,bool bPaused) const
    {
        return bActive ? FMath::Max(0.0,Deadline-((bPaused||bHeld)?LastClockSample:Now)) : 0.0;
    }
    double GetChargeSeconds(double Now,bool bPaused) const
    {
        return bCharging ? FMath::Max(0.0,((bPaused||bHeld)?LastClockSample:Now)-ChargeStartedAt) : 0.0;
    }
    void Hold(double Now) { bHeld=true; LastClockSample=Now; }
    void OpenNextPrompt(double Now) { bHeld=false; Deadline=Now+WindowSeconds; LastClockSample=Now; }
    EQuasicomboQTEProgress Advance(double Now,bool bPaused)
    {
        if (!bActive) return EQuasicomboQTEProgress::None;
        if (bPaused||bHeld)
        {
            const double Shift=FMath::Max(0.0,Now-LastClockSample);
            Deadline+=Shift; if(bCharging) ChargeStartedAt+=Shift;
        }
        LastClockSample=Now;
        if(!bPaused&&!bHeld&&Now>Deadline) { Cancel(); return EQuasicomboQTEProgress::Failed; }
        return EQuasicomboQTEProgress::None;
    }
    EQuasicomboQTEProgress Submit(EQuasicomboQTEInput Input,double Now,bool bPaused,
        EQuasicomboQTEInputEdge Edge=EQuasicomboQTEInputEdge::Press)
    {
        const auto Clock=Advance(Now,bPaused);
        if(Clock!=EQuasicomboQTEProgress::None) return Clock;
        if(!bActive||bPaused||bHeld) return EQuasicomboQTEProgress::None;
        if(Edge!=EQuasicomboQTEInputEdge::Press&&!bCharging) return EQuasicomboQTEProgress::None;
        if(Input!=GetExpectedInput()||Edge==EQuasicomboQTEInputEdge::Cancel)
        { Cancel(); return EQuasicomboQTEProgress::Failed; }
        if(Input==EQuasicomboQTEInput::Fire)
        {
            if(Edge==EQuasicomboQTEInputEdge::Press)
            {
                if(bCharging) return EQuasicomboQTEProgress::None;
                bCharging=true; ChargeStartedAt=Now; Deadline=Now+ChargeHoldSeconds+ReleaseWindowSeconds;
                return EQuasicomboQTEProgress::Charging;
            }
            if(Now-ChargeStartedAt<ChargeHoldSeconds) { Cancel(); return EQuasicomboQTEProgress::Failed; }
            bCharging=false;
        }
        ++PromptIndex;
        if(PromptIndex>=PromptCount) { bActive=false; return EQuasicomboQTEProgress::Succeeded; }
        Deadline=Now+WindowSeconds;
        return EQuasicomboQTEProgress::Advanced;
    }
private:
    bool bActive=false,bTauPattern=false,bCharging=false,bHeld=false;
    int32 PromptIndex=0,PromptCount=3,RoundCount=1,RoundNumber=1;
    double WindowSeconds=1.5,Deadline=0,LastClockSample=0,ChargeStartedAt=0;
};
