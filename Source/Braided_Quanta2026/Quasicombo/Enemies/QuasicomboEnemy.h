#pragma once

#include "CoreMinimal.h"
#include "SideScrollingCombatEnemy.h"
#include "QuasicomboEnemy.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class UUserWidget;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EQuasicomboEnemySpecies : uint8
{
    WoodMonster,
    StoneGolem
};

UENUM(BlueprintType)
enum class EQuasicomboEnemyTier : uint8
{
    Light,
    Medium,
    Heavy
};

/** One placeable enemy configured by species and tier. Art is assigned on BP_QC_Enemy. */
UCLASS(Blueprintable, meta=(PrioritizeCategories="Quasicombo Combat"))
class AQuasicomboEnemy : public ASideScrollingCombatEnemy
{
    GENERATED_BODY()

public:
    AQuasicomboEnemy();

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Variant")
    EQuasicomboEnemySpecies Species = EQuasicomboEnemySpecies::WoodMonster;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Quasicombo|Variant")
    EQuasicomboEnemyTier Tier = EQuasicomboEnemyTier::Light;

    UFUNCTION(BlueprintPure, Category="Quasicombo|Variant")
    float GetConfiguredMeleeDamage() const { return MeleeDamage; }

    /** Chance that a normal Quasicombo enemy chooses a charged, hyper-armored attack. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ChargedAttackChance = 0.25f;

    UAnimSequence* GetGolemIdleAnimation() const { return GolemIdleAnimation; }
    UAnimSequence* GetGolemWalkAnimation() const { return GolemWalkAnimation; }

    virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation,
        const FVector& DamageImpulse) override;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual bool ShouldUseChargedSideAttack() const override;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Wood Monster")
    TObjectPtr<USkeletalMesh> WoodMesh;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Wood Monster")
    TSubclassOf<UAnimInstance> WoodAnimClass;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Wood Monster")
    TObjectPtr<UAnimMontage> WoodComboMontage;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Wood Monster")
    TObjectPtr<UAnimMontage> WoodChargedMontage;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Wood Monster")
    TArray<TObjectPtr<UMaterialInterface>> WoodLightMaterials;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Wood Monster")
    TArray<TObjectPtr<UMaterialInterface>> WoodMediumMaterials;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Wood Monster")
    TArray<TObjectPtr<UMaterialInterface>> WoodHeavyMaterials;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<USkeletalMesh> GolemMesh;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TSubclassOf<UAnimInstance> GolemAnimClass;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<UAnimSequence> GolemIdleAnimation;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<UAnimSequence> GolemWalkAnimation;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<UAnimMontage> GolemComboMontage;

    /** Optional dedicated charged montage. If unset, charged attacks reuse the combo animation with charged timing. */
    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<UAnimMontage> GolemChargedMontage;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<UMaterialInterface> GolemLightMaterial;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<UMaterialInterface> GolemMediumMaterial;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Stone Golem")
    TObjectPtr<UMaterialInterface> GolemHeavyMaterial;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|UI")
    TSubclassOf<UUserWidget> EnemyLifeBarClass;

    UPROPERTY(EditDefaultsOnly, Category="Quasicombo|Assets|Effects")
    TObjectPtr<UNiagaraSystem> DamageEffect;

private:
    void ApplyVariant();
};
