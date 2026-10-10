#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "QuasicomboPlayerMovement.generated.h"

UENUM(BlueprintType)
enum class EQuasicomboWallState : uint8 { None, Gripping, Sliding };

/** Wall assistance stays in falling physics, including the engine's fall substeps. */
UCLASS()
class UQuasicomboPlayerMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling|Wall Slide", meta=(ClampMin="0", Units="s"))
    float WallGripDuration = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling|Wall Slide", meta=(ClampMin="1", Units="cm/s"))
    float WallSlideSpeed = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling|Wall Slide", meta=(ClampMin="0", Units="s"))
    float WallContactGrace = 0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Side Scrolling|Wall Slide", meta=(ClampMin="0", Units="cm"))
    float WallProbeDistance = 18.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Side Scrolling|Wall Slide")
    EQuasicomboWallState WallState = EQuasicomboWallState::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Side Scrolling|Wall Slide")
    FVector WallNormal = FVector::ZeroVector;

    void SetHorizontalInput(float Value) { HorizontalInput = Value; }
    void ClearWall(float ReattachDelay = 0.2f);
    bool ConsumeWallJump(FVector& Normal);
    bool IsOnWall() const { return WallState != EQuasicomboWallState::None; }
    virtual void PhysFalling(float DeltaTime, int32 Iterations) override;
    virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
    virtual float GetGravityZ() const override;

private:
    bool FindWall(FHitResult& Hit) const;
    void UpdateWall();
    float HorizontalInput = 0;
    float LastContactTime = -10000;
    float GripStartedTime = -10000;
    float ReattachAfter = -10000;
};
