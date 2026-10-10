#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "QuasicomboBackgroundCreature.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

/** Cosmetic local wandering; also attaches to existing scenery without replacing actors. */
UCLASS(ClassGroup=(Quasicombo), meta=(BlueprintSpawnableComponent))
class UQuasicomboBackgroundRoamingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UQuasicomboBackgroundRoamingComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(DisplayName="Idle Only"))
    bool bIdleOnly = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background")
    TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly"))
    TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly"))
    TObjectPtr<UAnimSequence> TurnLeftAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly"))
    TObjectPtr<UAnimSequence> TurnRightAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="1", Units="deg/s"))
    float TurnRate = 90;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="1", ClampMax="180", Units="deg"))
    float TurnAnimationAngle = 45;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="0", Units="cm"))
    float RoamingRadius = 600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="0", Units="cm/s"))
    float WalkSpeed = 90;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="0", Units="s"))
    float MinimumPause = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="0", Units="s"))
    float MaximumPause = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="1", Units="cm/s"))
    float AnimationWalkSpeed = 150;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="1", Units="cm"))
    float MaximumGroundStep = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(EditCondition="!bIdleOnly", ClampMin="1", Units="cm"))
    float ObstacleRadius = 40;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background")
    float MeshForwardYawOffset = 0;
    UFUNCTION(BlueprintCallable, Category="Quasicombo|Background") void SetIdleOnly(bool bValue);
    UFUNCTION(BlueprintPure, Category="Quasicombo|Background") FVector GetRoamingAnchor() const { return Anchor; }
    UFUNCTION(BlueprintPure, Category="Quasicombo|Background") FVector GetRoamingDestination() const { return Destination; }
    UFUNCTION(BlueprintPure, Category="Quasicombo|Background") bool IsRoaming() const { return bMoving && !bIdleOnly; }
    UFUNCTION(BlueprintPure, Category="Quasicombo|Background") bool IsTurning() const { return bTurning && !bIdleOnly; }
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
    virtual void BeginPlay() override;
    virtual void OnRegister() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
    USkeletalMeshComponent* FindBody() const;
    void PlayIdle();
    void Pause();
    bool FindGround(const FVector& Position, FHitResult& Hit) const;
    bool PickDestination();
    bool bLastIdleOnly = false;
    bool bMoving = false;
    bool bTurning = false;
    bool bGroundInitialized = false;
    float PauseRemaining = 0;
    float GroundOffset = 0;
    float TurnAge = 0;
    float TurnDuration = 0;
    float TurnStartYaw = 0;
    float TurnYaw = 0;
    FVector Anchor = FVector::ZeroVector;
    FVector Destination = FVector::ZeroVector;
    FRotator AuthoredRotation = FRotator::ZeroRotator;
};

/** Prefab base exposes the placement checkbox directly on the actor. */
UCLASS(Blueprintable)
class AQuasicomboBackgroundCreature : public ASkeletalMeshActor
{
    GENERATED_BODY()
public:
    AQuasicomboBackgroundCreature();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quasicombo|Background")
    TObjectPtr<UQuasicomboBackgroundRoamingComponent> Roaming;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Background", meta=(DisplayName="Idle Only"))
    bool bIdleOnly = false;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
    bool bLastIdleOnly = false;
};
