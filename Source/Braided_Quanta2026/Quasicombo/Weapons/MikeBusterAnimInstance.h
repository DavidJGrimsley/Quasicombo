#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MikeBusterAnimInstance.generated.h"

/** Parent of the existing Mike AnimBP; only supplies the right-arm layer weight. */
UCLASS(Transient, Blueprintable)
class UMikeBusterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Side Scrolling|Wall Slide") bool bWallSliding = false;
	UPROPERTY(BlueprintReadOnly, Category="Buster Cannon") float BusterPoseAlpha = 0;
	UPROPERTY(BlueprintReadOnly, Category="Buster Cannon") FVector BusterArmRecoilOffset = FVector::ZeroVector;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
