#include "MikeBusterAnimInstance.h"
#include "SideScrollingCharacter.h"
#include "BusterCannonComponent.h"

void UMikeBusterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const auto* Player = Cast<ASideScrollingCharacter>(TryGetPawnOwner());
    bWallSliding = Player && Player->IsWallSliding();
	const bool bShow = Player && Player->GetCurrentHP() > 0 && Player->GetBusterCannon() &&
		Player->GetBusterCannon()->IsEquipped() && Player->GetBusterCannon()->IsHoldingFirePose();
	BusterPoseAlpha = FMath::FInterpTo(BusterPoseAlpha, bShow ? 1.f : 0.f, DeltaSeconds, 15.f);
	BusterArmRecoilOffset = FVector::ZeroVector;
}
