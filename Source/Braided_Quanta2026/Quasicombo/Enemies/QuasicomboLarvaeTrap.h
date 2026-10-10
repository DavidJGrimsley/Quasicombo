#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "QuasicomboLarvaeTrap.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class ASideScrollingCharacter;

UENUM(BlueprintType)
enum class EQuasicomboLarvaeTrapState : uint8 { Buried, Armed, Emerging, Spitting, Burrowing, Cooldown };

UCLASS(ClassGroup=(Quasicombo), meta=(BlueprintSpawnableComponent))
class UQuasicomboLarvaeTrapComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UQuasicomboLarvaeTrapComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Trap", meta=(ClampMin="1", Units="cm")) float TriggerRadius = 600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Trap") TObjectPtr<UAnimSequence> EmergenceAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Trap") TObjectPtr<UAnimSequence> SpitAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Trap", meta=(ClampMin="0", Units="s")) float SpitReleaseTime = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Trap", meta=(ClampMin="0.01", Units="s")) float SpitFlightDuration = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Trap", meta=(ClampMin="0", Units="s")) float RearmCooldown = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Trap") float MeshForwardYawOffset = -90;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Trap") EQuasicomboLarvaeTrapState State = EQuasicomboLarvaeTrapState::Buried;
    UFUNCTION(BlueprintCallable, Category="Quasicombo|Trap") bool TryTrigger(ASideScrollingCharacter* Player);
    UFUNCTION(BlueprintCallable, Category="Quasicombo|Trap") void CancelTrap();
    UFUNCTION(BlueprintPure, Category="Quasicombo|Trap") bool HasDeliveredLethalHit() const { return bDeliveredHit; }
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    USkeletalMeshComponent* FindBody() const;
    void SetState(EQuasicomboLarvaeTrapState NewState);
    void BeginBurrowing();
    void ReleaseSpit();
    void RemoveSpit();
    void ReleasePlayer();
    float Age = 0;
    bool bWasInside = false;
    bool bSpitReleased = false;
    bool bDeliveredHit = false;
    TWeakObjectPtr<ASideScrollingCharacter> Target;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> SpitVisual;
    FVector SpitStart = FVector::ZeroVector;
    FVector SpitEnd = FVector::ZeroVector;
};

UCLASS(Blueprintable)
class AQuasicomboLarvaeTrap : public ASkeletalMeshActor
{
    GENERATED_BODY()
public:
    AQuasicomboLarvaeTrap();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Trap") TObjectPtr<UQuasicomboLarvaeTrapComponent> Trap;
};
