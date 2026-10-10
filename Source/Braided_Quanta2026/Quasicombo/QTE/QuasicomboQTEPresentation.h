#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QuasicomboQTEPresentation.generated.h"
class UAnimSequence;
class UParticleSystem;
class USoundBase;
class ULevelSequence;

/** Published from Sequencer. Offsets use +X toward the boss; Y is scene depth. */
USTRUCT(BlueprintType)
struct FQuasicomboQTESample
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) float Fraction=0;
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) FVector MikeOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) FVector LizardOffset=FVector(125,0,0);
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) float LizardYaw=180;
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) FVector CameraOffset=FVector(-80,450,110);
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) FVector FocusOffset=FVector(0,0,40);
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) float TargetBlend=.5f;
    UPROPERTY(EditAnywhere,meta=(EditCondition="false")) float FOV=48;
};

USTRUCT(BlueprintType)
struct FQuasicomboQTESegment
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,Category="Authoring") TSoftObjectPtr<ULevelSequence> Source;
    UPROPERTY(EditAnywhere,Category="Published",meta=(EditCondition="false")) TObjectPtr<UAnimSequence> Mike;
    UPROPERTY(EditAnywhere,Category="Published",meta=(EditCondition="false")) TObjectPtr<UAnimSequence> Lizard;
    UPROPERTY(EditAnywhere,Category="Published",meta=(EditCondition="false",DisplayName="Duration (generated)")) float Duration=1;
    UPROPERTY(EditAnywhere,Category="Published",meta=(EditCondition="false",DisplayName="Prompt Fraction (generated)")) float PromptFraction=-1;
    UPROPERTY(EditAnywhere,Category="Published",meta=(EditCondition="false",DisplayName="Contact Fraction (generated)")) float ContactFraction=-1;
    UPROPERTY(EditAnywhere,Category="Published",meta=(EditCondition="false",DisplayName="Path and Camera Samples (generated)")) TArray<FQuasicomboQTESample> Samples;
    FQuasicomboQTESample Evaluate(float Fraction) const;
};

USTRUCT(BlueprintType)
struct FQuasicomboQTEBeat
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> Mike;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> Lizard;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.2")) float Duration=0.9f;
    // Stop within anticipation; the accepted input continues this same clip.
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.01",ClampMax="0.4")) float PromptFraction=0.15f;
    UPROPERTY(EditAnywhere) bool bProducesContact=true;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0",ClampMax="1")) float ContactFraction=0.45f;
    UPROPERTY(EditAnywhere) FVector CameraOffset=FVector(-80,450,110);
    UPROPERTY(EditAnywhere) float FOV=48.f;
    UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> ContactSound;
    UPROPERTY(EditAnywhere) FName MikeContactBone=TEXT("hand_r");
    UPROPERTY(EditAnywhere) FName LizardContactBone=TEXT("spine_02");
};
UCLASS(BlueprintType)
class UQuasicomboQTEPresentation : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Sequencer Choreography") TMap<FName,FQuasicomboQTESegment> Choreography;
    const FQuasicomboQTESegment* FindSegment(FName Name) const { return Choreography.Find(Name); }
    UFUNCTION(CallInEditor,Category="Sequencer Choreography") void PublishChoreography();
    UFUNCTION(BlueprintCallable,Category="Editor Scripting|QTE",meta=(DevelopmentOnly)) static UAnimSequence* CreateStagedClip(UAnimSequence* Template);
    UPROPERTY(EditAnywhere) TArray<FQuasicomboQTEBeat> Beats;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> MikeJumpReady;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> MikeApproach;
    UPROPERTY(EditAnywhere,meta=(ClampMin="50",Units="cm/s")) float ApproachSpeed=220.f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.2",Units="s")) float JumpReadyDuration=0.6f;
    UPROPERTY(EditAnywhere,meta=(ClampMin="0.1",ClampMax="0.98")) float JumpReadyPromptFraction=0.95f;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> MikeCounterReaction;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> LizardCounter;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> MikeCharge;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> MikeFire;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> MikeRecover;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> LizardRecover;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> LizardFlight;
    UPROPERTY(EditAnywhere) TObjectPtr<UParticleSystem> ChargeEffect;
    UPROPERTY(EditAnywhere) TObjectPtr<UParticleSystem> ImpactEffect;
    UPROPERTY(EditAnywhere) TObjectPtr<UParticleSystem> ElectricityEffect;
    UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> ChargeSound;
    UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> FireSound;
    UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> ImpactSound;
    UPROPERTY(EditAnywhere,meta=(ClampMin="2")) float CinematicDiameterMultiplier=3.f;
    UPROPERTY(EditAnywhere) float ExplosionScale=4.f;
};
