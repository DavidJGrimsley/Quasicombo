#pragma once

#include "CoreMinimal.h"
#include "QuasicomboQTETypes.h"

/** The clock and input rules are separate from cameras/UI so they can be tested deterministically. */
enum class EQuasicomboQTEProgress : uint8
{
	None,
	Advanced,
	Succeeded,
	Failed
};

class FQuasicomboQTESequence
{
public:
	void Begin(bool bInTauPattern, double Now, double InWindowSeconds, int32 InRoundCount = 1)
	{
		bActive = true;
		bTauPattern = bInTauPattern;
		PromptIndex = 0;
		RoundCount = FMath::Clamp(InRoundCount, 1, 3);
		WindowSeconds = FMath::Max(0.1, InWindowSeconds);
		Deadline = Now + WindowSeconds;
		LastClockSample = Now;
	}

	void Cancel() { bActive = false; }
	bool IsActive() const { return bActive; }
	int32 GetPromptNumber() const { return PromptIndex % 3 + 1; }
	int32 GetRoundNumber() const { return FMath::Min(PromptIndex / 3 + 1, RoundCount); }
	int32 GetRoundCount() const { return RoundCount; }

	EQuasicomboQTEInput GetExpectedInput() const
	{
		if (!bActive) return EQuasicomboQTEInput::Invalid;
		if (PromptIndex % 3 == 2) return EQuasicomboQTEInput::Attack;
		if (PromptIndex % 3 == 1) return bTauPattern ? EQuasicomboQTEInput::Jump : EQuasicomboQTEInput::Dash;
		return bTauPattern ? EQuasicomboQTEInput::Dash : EQuasicomboQTEInput::Attack;
	}

	double GetSecondsRemaining(double Now, bool bPaused) const
	{
		if (!bActive) return 0.0;
		return FMath::Max(0.0, Deadline - (bPaused ? LastClockSample : Now));
	}

	EQuasicomboQTEProgress Advance(double Now, bool bPaused)
	{
		if (!bActive) return EQuasicomboQTEProgress::None;
		if (bPaused) Deadline += FMath::Max(0.0, Now - LastClockSample);
		LastClockSample = Now;
		if (bPaused) return EQuasicomboQTEProgress::None;
		if (Now > Deadline)
		{
			bActive = false;
			return EQuasicomboQTEProgress::Failed;
		}
		return EQuasicomboQTEProgress::None;
	}

	EQuasicomboQTEProgress Submit(EQuasicomboQTEInput Input, double Now, bool bPaused)
	{
		const EQuasicomboQTEProgress ClockResult = Advance(Now, bPaused);
		if (ClockResult != EQuasicomboQTEProgress::None) return ClockResult;
		if (!bActive || bPaused) return EQuasicomboQTEProgress::None;
		if (Input != GetExpectedInput())
		{
			bActive = false;
			return EQuasicomboQTEProgress::Failed;
		}
		++PromptIndex;
		if (PromptIndex == 3 * RoundCount)
		{
			bActive = false;
			return EQuasicomboQTEProgress::Succeeded;
		}
		Deadline = Now + WindowSeconds;
		return EQuasicomboQTEProgress::Advanced;
	}

private:
	bool bActive = false;
	bool bTauPattern = false;
	int32 PromptIndex = 0;
	int32 RoundCount = 1;
	double WindowSeconds = 4.0;
	double Deadline = 0.0;
	double LastClockSample = 0.0;
};
