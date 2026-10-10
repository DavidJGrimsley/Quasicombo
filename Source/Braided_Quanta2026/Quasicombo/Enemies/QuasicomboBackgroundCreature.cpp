#include "QuasicomboBackgroundCreature.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UQuasicomboBackgroundRoamingComponent::UQuasicomboBackgroundRoamingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.f / 30.f;
}

USkeletalMeshComponent* UQuasicomboBackgroundRoamingComponent::FindBody() const
{
    if (!GetOwner()) return nullptr;
    if (auto* Skeletal = Cast<ASkeletalMeshActor>(GetOwner())) return Skeletal->GetSkeletalMeshComponent();
    return GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
}

void UQuasicomboBackgroundRoamingComponent::PlayIdle()
{
    if (auto* Body = FindBody(); Body && IdleAnimation)
    {
        Body->PlayAnimation(IdleAnimation, true);
        Body->SetPlayRate(1);
    }
}

void UQuasicomboBackgroundRoamingComponent::OnRegister()
{
    Super::OnRegister();
    if (GetOwner()) GetOwner()->Tags.AddUnique(TEXT("QC_Background"));
    if (!GetWorld() || !GetWorld()->IsGameWorld()) PlayIdle();
}

void UQuasicomboBackgroundRoamingComponent::BeginPlay()
{
    Super::BeginPlay();
    Anchor = GetOwner()->GetActorLocation();
    AuthoredRotation = GetOwner()->GetActorRotation();
    bLastIdleOnly = bIdleOnly;
    if (auto* Body = FindBody()) Body->SetMobility(EComponentMobility::Movable);
    Pause();
}

void UQuasicomboBackgroundRoamingComponent::SetIdleOnly(bool bValue)
{
    bIdleOnly = bLastIdleOnly = bValue;
    bMoving = bTurning = false;
    PauseRemaining = bValue ? 0 : FMath::FRandRange(MinimumPause, FMath::Max(MinimumPause, MaximumPause));
    PlayIdle();
}

#if WITH_EDITOR
void UQuasicomboBackgroundRoamingComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UQuasicomboBackgroundRoamingComponent, bIdleOnly))
        if (auto* Creature = Cast<AQuasicomboBackgroundCreature>(GetOwner())) Creature->bIdleOnly = bIdleOnly;
    Super::PostEditChangeProperty(Event);
    SetIdleOnly(bIdleOnly);
}
#endif

void UQuasicomboBackgroundRoamingComponent::Pause()
{
    bMoving = bTurning = false;
    PauseRemaining = bIdleOnly ? 0 : FMath::FRandRange(MinimumPause, FMath::Max(MinimumPause, MaximumPause));
    PlayIdle();
}

bool UQuasicomboBackgroundRoamingComponent::FindGround(const FVector& Position, FHitResult& Hit) const
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(QuasicomboSceneryGround), false, GetOwner());
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    TArray<FHitResult> Hits;
    const float Height = FMath::Max(1000.f, float(GetOwner()->GetActorScale3D().GetMax() * 300));
    GetWorld()->LineTraceMultiByObjectType(Hits, Position + FVector(0, 0, Height), Position - FVector(0, 0, Height * 2), Objects, Params);
    for (const FHitResult& Candidate : Hits)
    {
        const AActor* Actor = Candidate.GetActor();
        if (!Actor || Actor->IsA<APawn>() || Actor->ActorHasTag(TEXT("QC_Background")) || Actor->ActorHasTag(TEXT("QC_LarvaeTrap"))) continue;
        if (Candidate.ImpactNormal.Z >= .7f)
        {
            Hit = Candidate;
            return true;
        }
    }
    return false;
}

bool UQuasicomboBackgroundRoamingComponent::PickDestination()
{
    const FVector Current = GetOwner()->GetActorLocation();
    FHitResult Here;
    if (!FindGround(Current, Here)) return false;
    if (!bGroundInitialized)
    {
        GroundOffset = Current.Z - Here.ImpactPoint.Z;
        bGroundInitialized = true;
    }
    for (int32 Attempt = 0; Attempt < 8; ++Attempt)
    {
        const float Angle = FMath::FRand() * 2 * PI;
        // Leave room around destinations for a walking arc, including a reversal.
        const float TurnClearance = 2 * WalkSpeed / FMath::DegreesToRadians(FMath::Max(1.f, TurnRate)) + 40;
        const float Radius = FMath::Max(0.f, RoamingRadius - TurnClearance) * FMath::Sqrt(FMath::FRand());
        FVector Candidate = Anchor + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0);
        Candidate.Z = Current.Z;
        FHitResult Ground;
        if (!FindGround(Candidate, Ground) || FMath::Abs(Ground.ImpactPoint.Z - Here.ImpactPoint.Z) > MaximumGroundStep) continue;
        Candidate.Z = Ground.ImpactPoint.Z + GroundOffset;
        if (FVector::Dist2D(Current, Candidate) < 80) continue;
        Destination = Candidate;
        bMoving = true;
        return true;
    }
    return false;
}

void UQuasicomboBackgroundRoamingComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, TickType, Function);
    if (!GetOwner() || !GetWorld() || !GetWorld()->IsGameWorld()) return;
    if (bLastIdleOnly != bIdleOnly) SetIdleOnly(bIdleOnly);
    if (bIdleOnly) return; // No ground probes, transforms, destinations, or pause clock in idle mode.
    if (bTurning)
    {
        TurnAge += Delta;
        FRotator Rotation = AuthoredRotation;
        Rotation.Yaw = TurnStartYaw + TurnYaw * FMath::Clamp(TurnAge / TurnDuration, 0.f, 1.f);
        GetOwner()->SetActorRotation(Rotation);
        if (TurnAge >= TurnDuration) bTurning = false;
        return;
    }
    if (!bMoving)
    {
        PauseRemaining -= Delta;
        if (PauseRemaining <= 0 && !PickDestination()) Pause();
        return;
    }
    const FVector Current = GetOwner()->GetActorLocation();
    FVector Direction = Destination - Current;
    Direction.Z = 0;
    if (Direction.SizeSquared() < 20 * 20) { Pause(); return; }
    FRotator Tilt = AuthoredRotation;
    Tilt.Yaw = 0;
    const FVector MeshForward = FRotator(0, -MeshForwardYawOffset, 0).Vector();
    const float DesiredYaw = Direction.Rotation().Yaw - Tilt.RotateVector(MeshForward).Rotation().Yaw;
    const float YawDifference = FMath::FindDeltaAngleDegrees(GetOwner()->GetActorRotation().Yaw, DesiredYaw);
    UAnimSequence* TurnAnimation = YawDifference > 0 ? TurnRightAnimation.Get() : TurnLeftAnimation.Get();
    if (FMath::Abs(YawDifference) >= TurnAnimationAngle && TurnAnimation && TurnAnimation->GetPlayLength() > 0)
    {
        if (auto* Body = FindBody())
        {
            bTurning = true;
            TurnAge = 0;
            TurnDuration = TurnAnimation->GetPlayLength();
            TurnStartYaw = GetOwner()->GetActorRotation().Yaw;
            TurnYaw = FMath::Sign(YawDifference) * TurnAnimationAngle;
            Body->PlayAnimation(TurnAnimation, false);
            Body->SetPlayRate(1);
            return;
        }
    }
    FRotator Rotation = AuthoredRotation;
    Rotation.Yaw = FMath::FixedTurn(GetOwner()->GetActorRotation().Yaw, DesiredYaw, Delta * TurnRate);
    const FVector WalkingDirection = Rotation.RotateVector(MeshForward).GetSafeNormal2D();
    const float ApproachSpeed = FMath::Min(WalkSpeed, float(Direction.Size()) * FMath::DegreesToRadians(TurnRate) * .5f);
    const float Speed = ApproachSpeed * FMath::Lerp(.5f, 1.f, 1.f - FMath::Abs(YawDifference) / 180.f);
    FVector Next = Current + WalkingDirection * FMath::Min(float(Direction.Size()), Speed * Delta);
    FHitResult Ground, Here;
    if (FVector::DistSquared2D(Next, Anchor) > FMath::Square(RoamingRadius + 1) || !FindGround(Current, Here) ||
        !FindGround(Next, Ground) || FMath::Abs(Ground.ImpactPoint.Z - Here.ImpactPoint.Z) > MaximumGroundStep)
    { Pause(); return; }
    Next.Z = Ground.ImpactPoint.Z + GroundOffset;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(QuasicomboSceneryObstacle), false, GetOwner());
    // A large probe can overlap its supporting slope. Support was checked above;
    // only other geometry should stop this step as an obstacle.
    Params.AddIgnoredComponent(Here.GetComponent());
    Params.AddIgnoredComponent(Ground.GetComponent());
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FHitResult Obstacle;
    const FVector ProbeStart(Current.X, Current.Y, Here.ImpactPoint.Z + ObstacleRadius + 10);
    const FVector ProbeEnd(Next.X, Next.Y, Ground.ImpactPoint.Z + ObstacleRadius + 10);
    if (GetWorld()->SweepSingleByObjectType(Obstacle, ProbeStart, ProbeEnd, FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(ObstacleRadius), Params)) { Pause(); return; }
    // Apply heading only with a valid step, so blocked creatures cannot swivel while idle.
    GetOwner()->SetActorRotation(Rotation);
    GetOwner()->SetActorLocation(Next);
    if (auto* Body = FindBody(); Body && WalkAnimation)
    {
        auto* Instance = Body->GetSingleNodeInstance();
        if (!Instance || Instance->GetCurrentAsset() != WalkAnimation) Body->PlayAnimation(WalkAnimation, true);
        Body->SetPlayRate(FMath::Clamp(Speed / FMath::Max(1.f, AnimationWalkSpeed), .01f, 3.f));
    }
}

AQuasicomboBackgroundCreature::AQuasicomboBackgroundCreature()
{
    PrimaryActorTick.bCanEverTick = true;
    Roaming = CreateDefaultSubobject<UQuasicomboBackgroundRoamingComponent>(TEXT("BackgroundRoaming"));
}

void AQuasicomboBackgroundCreature::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // A component default/override is also a supported source for the checkbox.
    if (bIdleOnly == bLastIdleOnly) bIdleOnly = Roaming->bIdleOnly;
    bLastIdleOnly = bIdleOnly;
    Roaming->SetIdleOnly(bIdleOnly);
}

#if WITH_EDITOR
void AQuasicomboBackgroundCreature::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AQuasicomboBackgroundCreature, bIdleOnly))
        Roaming->SetIdleOnly(bIdleOnly);
    Super::PostEditChangeProperty(Event);
}
#endif

void AQuasicomboBackgroundCreature::Tick(float Delta)
{
    Super::Tick(Delta);
    if (bLastIdleOnly != bIdleOnly) { bLastIdleOnly = bIdleOnly; Roaming->SetIdleOnly(bIdleOnly); }
    else bIdleOnly = bLastIdleOnly = Roaming->bIdleOnly;
}
