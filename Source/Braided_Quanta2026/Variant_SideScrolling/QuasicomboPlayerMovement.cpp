#include "QuasicomboPlayerMovement.h"
#include "SideScrollingCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void UQuasicomboPlayerMovement::ClearWall(float Delay)
{
    WallState = EQuasicomboWallState::None;
    WallNormal = FVector::ZeroVector;
    LastContactTime = -10000;
    if (GetWorld()) ReattachAfter = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, Delay);
}

bool UQuasicomboPlayerMovement::FindWall(FHitResult& Hit) const
{
    if (!CharacterOwner || !GetWorld()) return false;
    const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    const float Reach = Capsule->GetScaledCapsuleRadius() + WallProbeDistance;
    const float Height = Capsule->GetScaledCapsuleHalfHeight() * 0.35f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(QuasicomboWall), false, CharacterOwner);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    const float Preferred = FMath::Abs(HorizontalInput) > .1f ? FMath::Sign(HorizontalInput)
        : FMath::Sign(CharacterOwner->GetActorForwardVector().X);
    for (float Direction : {Preferred, -Preferred})
    {
        for (float Offset : {-Height, Height})
        {
            const FVector Start = CharacterOwner->GetActorLocation() + FVector(0, 0, Offset);
            TArray<FHitResult> Hits;
            GetWorld()->LineTraceMultiByObjectType(Hits, Start, Start + FVector(Direction * Reach, 0, 0), Objects, Params);
            for (const FHitResult& Candidate : Hits)
            {
                const AActor* Actor = Candidate.GetActor();
                const UPrimitiveComponent* Component = Candidate.GetComponent();
                if (!Actor || !Component || Actor->IsA<APawn>() || Actor->ActorHasTag(TEXT("QC_Background")) ||
                    Actor->ActorHasTag(TEXT("QC_LarvaeTrap")) || Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
                if (FMath::Abs(Candidate.ImpactNormal.X) > .8f && FMath::Abs(Candidate.ImpactNormal.Z) < .2f)
                {
                    Hit = Candidate;
                    return true;
                }
            }
        }
    }
    return false;
}

void UQuasicomboPlayerMovement::UpdateWall()
{
    const auto* Player = Cast<ASideScrollingCharacter>(CharacterOwner);
    const float Now = GetWorld()->GetTimeSeconds();
    if (!Player || !Player->CanAttachToWall() || Velocity.Z > 0 || Now < ReattachAfter)
    {
        WallState = EQuasicomboWallState::None;
        return;
    }
    FHitResult Hit;
    if (!FindWall(Hit))
    {
        WallState = EQuasicomboWallState::None;
        return; // Keep the last normal/time for the short jump grace period.
    }
    if (HorizontalInput * Hit.ImpactNormal.X > .1f)
    {
        ClearWall();
        return;
    }
    const FVector NewNormal = FVector(Hit.ImpactNormal.X, 0, 0).GetSafeNormal();
    if (Now - LastContactTime > WallContactGrace || !WallNormal.Equals(NewNormal, .1f)) GripStartedTime = Now;
    LastContactTime = Now;
    WallNormal = NewNormal;
    WallState = Now - GripStartedTime < WallGripDuration ? EQuasicomboWallState::Gripping : EQuasicomboWallState::Sliding;
    CharacterOwner->SetActorRotation((-WallNormal).Rotation());
    Velocity.Z = WallState == EQuasicomboWallState::Gripping ? 0 : FMath::Max(Velocity.Z, -WallSlideSpeed);
}

bool UQuasicomboPlayerMovement::ConsumeWallJump(FVector& Normal)
{
    if (!GetWorld() || !IsFalling() || WallNormal.IsNearlyZero() ||
        GetWorld()->GetTimeSeconds() - LastContactTime > WallContactGrace) return false;
    Normal = WallNormal;
    ClearWall();
    return true;
}

void UQuasicomboPlayerMovement::PhysFalling(float DeltaTime, int32 Iterations)
{
    UpdateWall();
    Super::PhysFalling(DeltaTime, Iterations);
    if (!IsFalling()) ClearWall(0);
}

FVector UQuasicomboPlayerMovement::NewFallVelocity(const FVector& Initial, const FVector& Gravity, float DeltaTime) const
{
    FVector Result = Super::NewFallVelocity(Initial, Gravity, DeltaTime);
    if (WallState == EQuasicomboWallState::Gripping) Result.Z = 0;
    else if (WallState == EQuasicomboWallState::Sliding) Result.Z = FMath::Max(Result.Z, -WallSlideSpeed);
    return Result;
}

float UQuasicomboPlayerMovement::GetGravityZ() const
{
    return WallState == EQuasicomboWallState::Gripping ? 0 : Super::GetGravityZ();
}
