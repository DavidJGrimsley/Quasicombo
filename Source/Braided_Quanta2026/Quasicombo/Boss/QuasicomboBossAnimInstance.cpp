#include "QuasicomboBossAnimInstance.h"
#include "QuasicomboBoss.h"
#include "QuasicomboEvolutionComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_ApplyAdditive.h"

struct FQuasicomboBossAnimProxy : FAnimInstanceProxy
{
	FAnimNode_SequencePlayer_Standalone Idle;
	FAnimNode_SequencePlayer_Standalone Walk;
	FAnimNode_TwoWayBlend Movement;
	FAnimNode_Slot Slot;
	FAnimNode_SequencePlayer_Standalone AttachmentPose;
	FAnimNode_ApplyAdditive Attachment;
	float ReactionWeight = 1.0f;
	float Speed = 0.0f;
	bool bWasDefeated = false;
	bool bWasDazed = false;
	explicit FQuasicomboBossAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
	virtual void Initialize(UAnimInstance* Instance) override
	{
		FAnimInstanceProxy::Initialize(Instance);
		Idle.SetLoopAnimation(true);
		Walk.SetLoopAnimation(true);
		Movement.A.SetLinkNode(&Idle);
		Movement.B.SetLinkNode(&Walk);
		Slot.Source.SetLinkNode(&Movement);
		Slot.SlotName = TEXT("DefaultSlot");
		Slot.bAlwaysUpdateSourcePose = true;
		AttachmentPose.SetLoopAnimation(false);
		AttachmentPose.SetPlayRate(0.0f);
		Attachment.Base.SetLinkNode(&Slot);
		Attachment.Additive.SetLinkNode(&AttachmentPose);
		Attachment.Alpha = 0.0f;
		Attachment.Initialize_AnyThread(FAnimationInitializeContext(this));
	}
	virtual void PreUpdate(UAnimInstance *Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		if (const AQuasicomboBoss *Boss = Cast<AQuasicomboBoss>(Instance->TryGetPawnOwner()))
		{
			ReactionWeight = Boss->GetReactionAnimationWeight();
			if (auto* Evolution = Boss->Evolution.Get())
			{
				AttachmentPose.SetSequence(Evolution->AttachmentAnimation.Get());
				AttachmentPose.SetAccumulatedTime(FMath::Min(Evolution->GetAttachmentAge(), 0.28f));
				Attachment.Alpha = Evolution->GetStage() == EQuasicomboEvolutionStage::ArmorAssembly &&
					Evolution->GetAttachmentAge() < 0.28f && AttachmentPose.GetSequence() &&
					AttachmentPose.GetSequence()->IsValidAdditive() ? 1.0f : 0.0f;
			}
			const bool bDefeated =
				(Boss->GetBossPhase() == EQuasicomboBossPhase::AwaitPickup || Boss->GetBossPhase() == EQuasicomboBossPhase::Complete) &&
				Boss->CurrentHP <= 0.0f && Boss->DefeatAnimation;
			const bool bDazed = Boss->GetBossPhase() == EQuasicomboBossPhase::QTE && Boss->DazedAnimation;
			Idle.SetSequence(bDefeated ? Boss->DefeatAnimation : bDazed ? Boss->DazedAnimation : Boss->IdleAnimation);
			if (bDefeated != bWasDefeated || bDazed != bWasDazed) Idle.SetAccumulatedTime(0.0f);
			bWasDefeated = bDefeated;
			bWasDazed = bDazed;
			Idle.SetLoopAnimation(!bDefeated);
			Walk.SetSequence(Boss->WalkAnimation);
			Speed = bDefeated || bDazed ? 0.0f : Boss->GetVelocity().Size2D();
			Walk.SetPlayRate(FMath::Clamp(Speed / 150.0f, 0.6f, 1.8f));
		}
	}
	virtual void CacheBones() override { Attachment.CacheBones_AnyThread(FAnimationCacheBonesContext(this)); }
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
	{
		Movement.Alpha = FMath::FInterpTo(Movement.Alpha, Speed > 8.0f ? 1.0f : 0.0f, Context.GetDeltaTime(), 8.0f);
		Attachment.Update_AnyThread(Context);
	}
	virtual bool Evaluate(FPoseContext &Output) override
	{
		Attachment.Evaluate_AnyThread(Output);
		if (ReactionWeight < 1.0f)
		{
			FPoseContext Base(Output);
			Movement.Evaluate_AnyThread(Base);
			FAnimationPoseData BaseData(Base), OutputData(Output);
			FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutputData, BaseData, ReactionWeight);
		}
		return true;
	}
};

FAnimInstanceProxy *UQuasicomboBossAnimInstance::CreateAnimInstanceProxy()
{
	return new FQuasicomboBossAnimProxy(this);
}
void UQuasicomboBossAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
