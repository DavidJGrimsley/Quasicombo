#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "QuasicomboBossAnimInstance.generated.h"

/** Small native graph: lizard idle/walk -> DefaultSlot. No Manny Control Rig dependency. */
UCLASS(Transient, Blueprintable)
class UQuasicomboBossAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
