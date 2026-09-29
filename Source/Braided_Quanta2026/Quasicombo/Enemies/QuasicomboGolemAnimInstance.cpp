#include "QuasicomboGolemAnimInstance.h"
#include "QuasicomboEnemy.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "AnimNodes/AnimNode_Slot.h"

struct FQuasicomboGolemAnimProxy : FAnimInstanceProxy
{
    FAnimNode_SequencePlayer_Standalone Idle;
    FAnimNode_SequencePlayer_Standalone Walk;
    FAnimNode_TwoWayBlend Movement;
    FAnimNode_Slot Slot;
    float Speed = 0.0f;

    explicit FQuasicomboGolemAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

    virtual void Initialize(UAnimInstance* Instance) override
    {
        FAnimInstanceProxy::Initialize(Instance);
        Idle.SetLoopAnimation(true);
        Walk.SetLoopAnimation(true);
        Movement.A.SetLinkNode(&Idle);
        Movement.B.SetLinkNode(&Walk);
        Slot.Source.SetLinkNode(&Movement);
        Slot.SlotName = TEXT("DefaultSlot");
        Slot.Initialize_AnyThread(FAnimationInitializeContext(this));
    }

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        if (const AQuasicomboEnemy* Enemy = Cast<AQuasicomboEnemy>(Instance->TryGetPawnOwner()))
        {
            Idle.SetSequence(Enemy->GetGolemIdleAnimation());
            Walk.SetSequence(Enemy->GetGolemWalkAnimation());
            Speed = Enemy->GetVelocity().Size2D();
            Walk.SetPlayRate(FMath::Clamp(Speed / 180.0f, 0.6f, 1.8f));
        }
    }

    virtual void CacheBones() override { Slot.CacheBones_AnyThread(FAnimationCacheBonesContext(this)); }

    virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
    {
        Movement.Alpha = FMath::FInterpTo(Movement.Alpha, Speed > 8.0f ? 1.0f : 0.0f, Context.GetDeltaTime(), 8.0f);
        Slot.Update_AnyThread(Context);
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        Slot.Evaluate_AnyThread(Output);
        return true;
    }
};

FAnimInstanceProxy* UQuasicomboGolemAnimInstance::CreateAnimInstanceProxy()
{
    return new FQuasicomboGolemAnimProxy(this);
}

void UQuasicomboGolemAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
    delete Proxy;
}
