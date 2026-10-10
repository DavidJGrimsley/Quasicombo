#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuasicomboQTETypes.h"
#include "QuasicomboRouteTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Components/SlateWrapperTypes.h"
#include "QuasicomboQTEDirector.generated.h"
class AQuasicomboBoss;
class ASideScrollingCharacter;
class UQuasicomboQTEComponent;
class UQuasicomboQTEPresentation;
class ACameraActor;
class UParticleSystemComponent;
class UAudioComponent;
class UAnimSequence;
class UAnimMontage;
class ACharacter;
class APlayerController;
class UUserWidget;
struct FQuasicomboQTESegment;

UCLASS()
class AQuasicomboQTEDirector : public AActor
{
    GENERATED_BODY()
    friend class FQuasicomboQTEDirectorTest;
    friend class FQuasicomboQTEJumpUppercutTest;
    friend class FQuasicomboQTEPromptPoseHoldTest;
    friend class FQuasicomboQTEAirborneChainTest;
    friend class FQuasicomboQTEFailureFeedbackTest;
public:
    AQuasicomboQTEDirector();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UQuasicomboQTEComponent> QTE;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UQuasicomboQTEPresentation> Presentation;
    UPROPERTY(EditAnywhere,Category="QTE",meta=(ClampMin="0.05",ClampMax="2.0")) float FailureFeedbackSeconds=.4f;
    bool Start(AQuasicomboBoss* Boss,ASideScrollingCharacter* Mike,bool bTau,int32 Round,int32 Rounds,int32 Count);
    bool IsActive() const { return bActive; }
    bool IsShowingFailureFeedback() const { return bActive&&Stage==EStage::FailureFeedback; }
#if WITH_DEV_AUTOMATION_TESTS
    void AdvanceForTesting(float Seconds);
    void AdvanceToPromptForTesting();
#endif
    bool Submit(EQuasicomboQTEInput Input,EQuasicomboQTEInputEdge Edge=EQuasicomboQTEInputEdge::Press);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    enum class EStage : uint8 { Enter, Apex, FinisherLanding, CannonLanding, Anticipation, Prompt, Action, Charge, FailureFeedback, Shot, Counter, Fall, Blast, Exit };
    UPROPERTY() TObjectPtr<UQuasicomboQTEPresentation> Data;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TObjectPtr<UParticleSystemComponent> ChargeFX;
    UPROPERTY() TObjectPtr<UParticleSystemComponent> ShotFX;
    UPROPERTY() TObjectPtr<UAudioComponent> ChargeAudio;
    UPROPERTY() TArray<TObjectPtr<UParticleSystemComponent>> ImpactFX;
    TArray<TWeakObjectPtr<UUserWidget>> CombatHUDs;
    TArray<ESlateVisibility> CombatHUDVisibility;
    TWeakObjectPtr<AQuasicomboBoss> Boss;
    TWeakObjectPtr<ASideScrollingCharacter> Mike;
    TWeakObjectPtr<UQuasicomboQTEComponent> OriginalQTE;
    TWeakObjectPtr<AActor> OriginalView;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<UAnimMontage> MikeMontage;
    TWeakObjectPtr<UAnimMontage> BossMontage;
    FTransform MikeStart,BossStart;
    FVector CounterStart,MikeCounterStart;
    uint64 RumbleHandle=0;
    float MikeRate=1,BossRate=1,SlowImpactRemaining=0,MikeGroundZ=0;
    bool bCounterStarted=false;
    bool bMikePoseDriven=false;
    bool bBossPoseDriven=false;
    bool bLaunched=false;
    bool bFlightFrozen=false;
    FName SegmentName;
    FVector RecoveryCorrection=FVector::ZeroVector;
    FVector LaunchStart;
    FVector LaunchPelvisStart;
    FVector ShotStart,ShotTarget,ImpactPosition;
    EMovementMode OriginalMovement=MOVE_Walking;
    EStage Stage=EStage::Enter;
    bool bActive=false,bFinal=false,bSuccess=false,bContact=false,bObliterated=false,bResolved=false;
    bool bMikeCollision=true,bBossCollision=true;
    int32 Beat=0,Count=2,PromptBeat=1;
    float HeldPoseFraction=0;
    float FailureFeedbackRemaining=0;
    double LastClock=0,HitStopUntil=0;
    float Age=0,Duration=0,OriginalDilation=1,Direction=1,FlightAge=0;
    UFUNCTION() void RunEnded(EQuasicomboRunOutcome Outcome);
    UFUNCTION() void DeviceConnectionChanged(EInputDeviceConnectionState State,FPlatformUserId User,FInputDeviceId Device);
    UFUNCTION() void Accepted(int32 Number,bool bChargeStart);
    UFUNCTION() void Released();
    UFUNCTION() void Result(bool bSucceeded);
    void SetStage(EStage Next,float Seconds);
    void Play(ACharacter* Character,UAnimSequence* Clip,float Seconds,bool bLoop=false,bool bHoldEnd=false,float BlendIn=.06f);
    void BeginAnticipation();
    void SetMikePose(float Fraction);
    void SetBossPose(float Fraction);
    void UpdateMikeActionPosition(int32 Move,float Fraction);
    FName BeatSegment(int32 Move) const;
    const FQuasicomboQTESegment* CurrentSegment() const;
    void PlaySegment(FName Name);
    void ApplySegmentPosition(float Fraction,bool bRecovery=false);
    void BeginTransition();
    void BeginRecovery();
    void Contact();
    void UpdateCamera(float Alpha);
    void UpdateFlight(float Delta);
    void FreezeFlight();
    void BeginCounter();
    void Finish();
    void Cleanup();
    void Rumble(float Strength);
    void StopCharge();
    void SpawnBurst(FVector Position,bool bHuge);
};
