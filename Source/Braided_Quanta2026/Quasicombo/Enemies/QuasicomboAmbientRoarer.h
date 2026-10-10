#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "QuasicomboAmbientRoarer.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

/** Stationary scenery with occasional roars and alternating stepping turns. */
UCLASS(ClassGroup=(Quasicombo), meta=(BlueprintSpawnableComponent))
class UQuasicomboAmbientRoarComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UQuasicomboAmbientRoarComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar")
    TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar")
    TObjectPtr<UAnimSequence> RoarAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar", meta=(ClampMin="1", Units="s"))
    float MinimumRoarInterval = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar", meta=(ClampMin="1", Units="s"))
    float MaximumRoarInterval = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar")
    TObjectPtr<UAnimSequence> TurnLeftAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar")
    TObjectPtr<UAnimSequence> TurnRightAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar", meta=(ClampMin="1", Units="s"))
    float ReadjustInterval = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background Roar", meta=(ClampMin="1", ClampMax="180", Units="deg"))
    float ReadjustAngle = 45;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Quasicombo|Background Roar")
    float TimeUntilNextRoar = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Quasicombo|Background Roar")
    float TimeUntilReadjust = 0;
    UFUNCTION(BlueprintPure, Category="Quasicombo|Background Roar") bool IsRoaring() const { return bRoaring; }
    UFUNCTION(BlueprintPure, Category="Quasicombo|Background Roar") bool IsReadjusting() const { return bReadjusting; }
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
protected:
    virtual void OnRegister() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    USkeletalMeshComponent* FindBody() const;
    void PlayIdle();
    void ScheduleRoar();
    bool bRoaring = false;
    float RoarRemaining = 0;
    bool bReadjusting = false;
    bool bNextTurnLeft = true;
    float TurnAge = 0;
    float TurnDuration = 0;
    float TurnYaw = 0;
    FRotator TurnStartRotation = FRotator::ZeroRotator;
};

UCLASS(Blueprintable)
class AQuasicomboAmbientRoarer : public ASkeletalMeshActor
{
    GENERATED_BODY()
public:
    AQuasicomboAmbientRoarer();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Background Roar")
    TObjectPtr<UQuasicomboAmbientRoarComponent> AmbientRoar;
};
