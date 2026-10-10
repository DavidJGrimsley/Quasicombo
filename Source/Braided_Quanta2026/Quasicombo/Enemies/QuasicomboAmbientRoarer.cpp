#include "QuasicomboAmbientRoarer.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UQuasicomboAmbientRoarComponent::UQuasicomboAmbientRoarComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.f / 30.f;
    IdleAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_Idle.ANIM_Megaspikan_Idle"));
    RoarAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_Roar.ANIM_Megaspikan_Roar"));
    TurnLeftAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Left.ANIM_Megaspikan_Turn45Left"));
    TurnRightAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Right.ANIM_Megaspikan_Turn45Right"));
}

USkeletalMeshComponent* UQuasicomboAmbientRoarComponent::FindBody() const
{
    if (auto* Actor = Cast<ASkeletalMeshActor>(GetOwner())) return Actor->GetSkeletalMeshComponent();
    return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UQuasicomboAmbientRoarComponent::PlayIdle()
{
    if (auto* Body = FindBody(); Body && IdleAnimation)
    {
        Body->PlayAnimation(IdleAnimation, true);
        Body->SetPlayRate(1);
    }
}

void UQuasicomboAmbientRoarComponent::OnRegister()
{
    Super::OnRegister();
    if (GetOwner()) GetOwner()->Tags.AddUnique(TEXT("QC_Background"));
    if (!GetWorld() || !GetWorld()->IsGameWorld()) PlayIdle();
}

void UQuasicomboAmbientRoarComponent::ScheduleRoar()
{
    const float Minimum = FMath::Max(1.f, MinimumRoarInterval);
    TimeUntilNextRoar = FMath::FRandRange(Minimum, FMath::Max(Minimum, MaximumRoarInterval));
}

void UQuasicomboAmbientRoarComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Body = FindBody()) Body->SetMobility(EComponentMobility::Movable);
    bRoaring = false;
    bReadjusting = false;
    bNextTurnLeft = true;
    TimeUntilReadjust = FMath::Max(1.f, ReadjustInterval);
    PlayIdle();
    ScheduleRoar();
}

void UQuasicomboAmbientRoarComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, TickType, Function);
    if (!GetWorld() || !GetWorld()->IsGameWorld()) return;
    TimeUntilNextRoar -= Delta;
    TimeUntilReadjust -= Delta;
    if (bRoaring)
    {
        RoarRemaining -= Delta;
        if (RoarRemaining <= 0) { bRoaring = false; PlayIdle(); }
        return;
    }
    if (bReadjusting)
    {
        TurnAge += Delta;
        FRotator Rotation = TurnStartRotation;
        Rotation.Yaw += TurnYaw * FMath::Clamp(TurnAge / TurnDuration, 0.f, 1.f);
        GetOwner()->SetActorRotation(Rotation);
        if (TurnAge >= TurnDuration) { bReadjusting = false; PlayIdle(); }
        return;
    }
    if (TimeUntilReadjust <= 0)
    {
        UAnimSequence* Turn = bNextTurnLeft ? TurnLeftAnimation.Get() : TurnRightAnimation.Get();
        TimeUntilReadjust = FMath::Max(1.f, ReadjustInterval);
        if (auto* Body = FindBody(); Body && Turn && Turn->GetPlayLength() > 0)
        {
            bReadjusting = true;
            TurnAge = 0;
            TurnDuration = Turn->GetPlayLength();
            TurnStartRotation = GetOwner()->GetActorRotation();
            TurnYaw = (bNextTurnLeft ? -1.f : 1.f) * ReadjustAngle;
            bNextTurnLeft = !bNextTurnLeft;
            Body->PlayAnimation(Turn, false);
            Body->SetPlayRate(1);
            return;
        }
    }
    if (TimeUntilNextRoar > 0) return;
    if (auto* Body = FindBody(); Body && RoarAnimation)
    {
        bRoaring = true;
        RoarRemaining = RoarAnimation->GetPlayLength();
        Body->PlayAnimation(RoarAnimation, false);
        Body->SetPlayRate(1);
    }
    ScheduleRoar(); // Random interval is measured between roar starts.
}

void UQuasicomboAmbientRoarComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bRoaring = false;
    bReadjusting = false;
    TimeUntilNextRoar = 0;
    TimeUntilReadjust = 0;
    Super::EndPlay(Reason);
}

AQuasicomboAmbientRoarer::AQuasicomboAmbientRoarer()
{
    AmbientRoar = CreateDefaultSubobject<UQuasicomboAmbientRoarComponent>(TEXT("AmbientRoar"));
}
