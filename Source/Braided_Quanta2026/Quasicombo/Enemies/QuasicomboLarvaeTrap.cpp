#include "QuasicomboLarvaeTrap.h"
#include "SideScrollingCharacter.h"
#include "QuasicomboRunSubsystem.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UQuasicomboLarvaeTrapComponent::UQuasicomboLarvaeTrapComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    EmergenceAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/MegaspikanLarvae/Animations/ANIM_MegaspikanLarvae_GetOutOfTheGround.ANIM_MegaspikanLarvae_GetOutOfTheGround"));
    SpitAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Megaspikan/MegaspikanLarvae/Animations/ANIM_MegaspikanLarvae_SpitAttack.ANIM_MegaspikanLarvae_SpitAttack"));
}

USkeletalMeshComponent* UQuasicomboLarvaeTrapComponent::FindBody() const
{
    if (auto* Actor = Cast<ASkeletalMeshActor>(GetOwner())) return Actor->GetSkeletalMeshComponent();
    return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UQuasicomboLarvaeTrapComponent::BeginPlay()
{
    Super::BeginPlay();
    GetOwner()->Tags.AddUnique(TEXT("QC_LarvaeTrap"));
    if (auto* Body = FindBody())
    {
        Body->SetMobility(EComponentMobility::Movable);
        Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Body->SetVisibility(false, true);
    }
    SetState(EQuasicomboLarvaeTrapState::Armed);
}

void UQuasicomboLarvaeTrapComponent::SetState(EQuasicomboLarvaeTrapState Value) { State = Value; Age = 0; }

bool UQuasicomboLarvaeTrapComponent::TryTrigger(ASideScrollingCharacter* Player)
{
    if (State != EQuasicomboLarvaeTrapState::Armed || !IsValid(Player) || Player->GetCurrentHP() <= 0 ||
        FVector::DistSquared(Player->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(TriggerRadius)) return false;
    if (!Player->BeginFatalTrap(GetOwner())) return false;
    Target = Player;
    bWasInside = true;
    bDeliveredHit = bSpitReleased = false;
    SetState(EQuasicomboLarvaeTrapState::Emerging);
    FRotator Facing = GetOwner()->GetActorRotation();
    FRotator Tilt = Facing;
    Tilt.Yaw = 0;
    const FVector MeshForward = FRotator(0, -MeshForwardYawOffset, 0).Vector();
    Facing.Yaw = (Player->GetActorLocation() - GetOwner()->GetActorLocation()).Rotation().Yaw - Tilt.RotateVector(MeshForward).Rotation().Yaw;
    GetOwner()->SetActorRotation(Facing);
    if (auto* Body = FindBody())
    {
        Body->SetVisibility(true, true);
        if (EmergenceAnimation) { Body->PlayAnimation(EmergenceAnimation, false); Body->SetPlayRate(1); }
    }
    return true;
}

void UQuasicomboLarvaeTrapComponent::RemoveSpit()
{
    if (SpitVisual) SpitVisual->DestroyComponent();
    SpitVisual = nullptr;
}

void UQuasicomboLarvaeTrapComponent::ReleasePlayer()
{
    if (Target.IsValid()) Target->EndFatalTrap(GetOwner());
    Target.Reset();
}

void UQuasicomboLarvaeTrapComponent::CancelTrap()
{
    ReleasePlayer();
    RemoveSpit();
    if (auto* Body = FindBody()) Body->SetVisibility(false, true);
    bWasInside = true;
    SetState(EQuasicomboLarvaeTrapState::Cooldown);
}

void UQuasicomboLarvaeTrapComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleasePlayer();
    RemoveSpit();
    Super::EndPlay(Reason);
}

void UQuasicomboLarvaeTrapComponent::ReleaseSpit()
{
    if (bSpitReleased || !Target.IsValid()) return;
    bSpitReleased = true;
    const auto* Body = FindBody();
    SpitStart = Body ? (Body->GetSocketLocation(TEXT("MegaspikanLarvae_JawLeft")) +
        Body->GetSocketLocation(TEXT("MegaspikanLarvae_JawRight"))) * .5f : GetOwner()->GetActorLocation();
    SpitEnd = Target->GetActorLocation() + FVector(0, 0, 30);
    SpitVisual = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("FatalLarvaeSpit"));
    SpitVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    SpitVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpitVisual->SetCastShadow(false);
    SpitVisual->RegisterComponent();
    SpitVisual->SetWorldLocation(SpitStart);
    SpitVisual->SetWorldScale3D(FVector(.24f));
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Quasicombo/Mike/M_BusterEnergy.M_BusterEnergy")))
    {
        if (auto* Dynamic = SpitVisual->CreateDynamicMaterialInstance(0, Material))
        {
            Dynamic->SetVectorParameterValue(TEXT("LightColor"), FLinearColor(.15f, 1.f, .015f));
            Dynamic->SetScalarParameterValue(TEXT("Opacity"), 1);
        }
    }
}

void UQuasicomboLarvaeTrapComponent::BeginBurrowing()
{
    ReleasePlayer();
    RemoveSpit();
    SetState(EQuasicomboLarvaeTrapState::Burrowing);
    if (auto* Body = FindBody(); Body && EmergenceAnimation)
    {
        Body->PlayAnimation(EmergenceAnimation, false);
        Body->SetPosition(EmergenceAnimation->GetPlayLength(), false);
        Body->SetPlayRate(-1);
    }
}

void UQuasicomboLarvaeTrapComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, TickType, Function);
    if (!GetWorld() || !GetOwner() || !GetWorld()->IsGameWorld()) return;
    auto* Player = Cast<ASideScrollingCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    const auto* Run = UQuasicomboRunSubsystem::GetRun(this);
    const bool bCanActivate = Player && Player->GetCurrentHP() > 0 && (!Run ||
        (Run->HasRunStarted() && Run->GetOutcome() == EQuasicomboRunOutcome::Playing && !Run->IsBossCombatLocked()));
    const bool bInside = bCanActivate && FVector::DistSquared(Player->GetActorLocation(), GetOwner()->GetActorLocation()) <= FMath::Square(TriggerRadius);
    Age += Delta;
    if (State == EQuasicomboLarvaeTrapState::Armed)
    {
        if (bInside && !bWasInside) TryTrigger(Player);
        bWasInside = bInside;
        return;
    }
    if (State == EQuasicomboLarvaeTrapState::Cooldown)
    {
        bWasInside = bInside;
        if (Age >= RearmCooldown) SetState(EQuasicomboLarvaeTrapState::Armed);
        return;
    }
    if (State == EQuasicomboLarvaeTrapState::Burrowing)
    {
        if (Age >= (EmergenceAnimation ? EmergenceAnimation->GetPlayLength() : 2))
        {
            if (auto* Body = FindBody()) Body->SetVisibility(false, true);
            bWasInside = bInside;
            SetState(EQuasicomboLarvaeTrapState::Cooldown);
        }
        return;
    }
    if (!bDeliveredHit && (!Target.IsValid() || Target->GetCurrentHP() <= 0 ||
        (Run && (Run->GetOutcome() != EQuasicomboRunOutcome::Playing || Run->IsBossCombatLocked()))))
    { CancelTrap(); return; }
    if (State == EQuasicomboLarvaeTrapState::Emerging)
    {
        if (Age >= (EmergenceAnimation ? EmergenceAnimation->GetPlayLength() : 2))
        {
            SetState(EQuasicomboLarvaeTrapState::Spitting);
            if (auto* Body = FindBody(); Body && SpitAnimation) { Body->PlayAnimation(SpitAnimation, false); Body->SetPlayRate(1); }
        }
    }
    else if (State == EQuasicomboLarvaeTrapState::Spitting)
    {
        if (!bDeliveredHit && Age >= SpitReleaseTime) ReleaseSpit();
        if (bSpitReleased)
        {
            const float Alpha = FMath::Clamp((Age - SpitReleaseTime) / FMath::Max(.01f, SpitFlightDuration), 0.f, 1.f);
            if (SpitVisual) SpitVisual->SetWorldLocation(FMath::Lerp(SpitStart, SpitEnd, Alpha) + FVector(0, 0, FMath::Sin(Alpha * PI) * 30));
            if (Alpha >= 1 && !bDeliveredHit)
            {
                bDeliveredHit = true;
                ASideScrollingCharacter* Victim = Target.Get();
                // Use normal fatal damage so HP, scoring, death animation and reload stay consistent.
                Victim->ApplyDamage(Victim->GetCurrentHP(), GetOwner(), SpitEnd, FVector::ZeroVector);
                RemoveSpit();
            }
        }
        if (bDeliveredHit && Age >= (SpitAnimation ? SpitAnimation->GetPlayLength() : .833333f)) BeginBurrowing();
    }
}

AQuasicomboLarvaeTrap::AQuasicomboLarvaeTrap()
{
    Trap = CreateDefaultSubobject<UQuasicomboLarvaeTrapComponent>(TEXT("LarvaeTrap"));
}
