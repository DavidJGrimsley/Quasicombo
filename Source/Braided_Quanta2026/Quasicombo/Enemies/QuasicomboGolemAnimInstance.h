#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "QuasicomboGolemAnimInstance.generated.h"

/** Plays the golem's native idle/walk clips under the shared attack montage slot. */
UCLASS(Transient, Blueprintable)
class UQuasicomboGolemAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
