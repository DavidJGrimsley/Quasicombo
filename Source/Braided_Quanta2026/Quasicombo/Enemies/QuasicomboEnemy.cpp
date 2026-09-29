#include "QuasicomboEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AQuasicomboEnemy::AQuasicomboEnemy()
{
    RequiredHits = 3;
    AggroRange = 800.0f;
    AttackRange = 150.0f;
    PatrolHalfWidth = 700.0f;
    AttackCooldown = 1.0f;
    AttackWindup = 0.2f;
    RecoveryDuration = 0.25f;
    // Match the old light/heavy Blueprints: melee traces hit the capsule,
    // while the mesh uses physics-body collision for hit reactions and death.
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AQuasicomboEnemy::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyVariant();
}

void AQuasicomboEnemy::BeginPlay()
{
    // The parent initializes CurrentHP from RequiredHits.
    ApplyVariant();
    Super::BeginPlay();
}

void AQuasicomboEnemy::ApplyDamage(float Damage, AActor* DamageCauser,
    const FVector& DamageLocation, const FVector& DamageImpulse)
{
    const int32 StrikesBefore = GetAcceptedStrikes();
    FVector ReactionImpulse = DamageImpulse;
    if (GetRemainingStrikes() > 1 && GetCharacterMovement()->IsFalling())
    {
        // Preserve the original single-hit hop without stacking upward launches
        // throughout the longer four-to-six-hit combos.
        ReactionImpulse.Z = FMath::Min(0.0, ReactionImpulse.Z);
    }
    Super::ApplyDamage(Damage, DamageCauser, DamageLocation, ReactionImpulse);
    if (GetAcceptedStrikes() > StrikesBefore && DamageEffect)
    {
        // This is the ReceivedDamage graph shared by the old enemy Blueprints.
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DamageEffect,
            DamageLocation, ReactionImpulse.GetSafeNormal().Rotation());
    }
}

void AQuasicomboEnemy::ApplyVariant()
{
    const bool bGolem = Species == EQuasicomboEnemySpecies::StoneGolem;
    const int32 TierIndex = FMath::Clamp(static_cast<int32>(Tier), 0, 2);
    RequiredHits = 3 + TierIndex + (bGolem ? 1 : 0);
    const float BaseDamage = bGolem ? 1.5f : 1.0f;
    static constexpr float WoodResistance[] = { 0.0f, 0.1f, 0.2f };
    static constexpr float GolemResistance[] = { 0.3f, 0.4f, 0.6f };
    SetResistance(bGolem ? GolemResistance[TierIndex] : WoodResistance[TierIndex]);
    const float TierMultiplier = 1.0f + static_cast<float>(TierIndex);
    MeleeDamage = BaseDamage * TierMultiplier;
    SetActorScale3D(FVector(1.5f * (1.0f + 0.25f * TierIndex)));

    UCapsuleComponent* Capsule = GetCapsuleComponent();
    Capsule->SetCapsuleSize(bGolem ? 40.0f : 35.0f, 90.0f, true);
    Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    USkeletalMeshComponent* Body = GetMesh();
    Body->SetSkeletalMesh(bGolem ? GolemMesh : WoodMesh);
    Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));
    Body->SetRelativeScale3D(FVector(bGolem ? 0.55f : 1.0f));
    Body->SetAnimInstanceClass(bGolem ? GolemAnimClass : WoodAnimClass);
    Body->SetCollisionProfileName(TEXT("Ragdoll"));
    Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    PelvisBoneName = Body->GetSkeletalMeshAsset() &&
        Body->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(TEXT("pelvis")) != INDEX_NONE
        ? FName(TEXT("pelvis")) : NAME_None;
    Body->EmptyOverrideMaterials();

    if (UWidgetComponent* Bar = FindComponentByClass<UWidgetComponent>())
    {
        Bar->SetWidgetClass(EnemyLifeBarClass);
        Bar->SetWidgetSpace(EWidgetSpace::Screen);
        Bar->SetDrawSize(FVector2D(120.0f, 30.0f));
        Bar->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
    }

    if (bGolem)
    {
        UMaterialInterface* Material = TierIndex == 0 ? GolemLightMaterial :
            TierIndex == 1 ? GolemMediumMaterial : GolemHeavyMaterial;
        if (Material) Body->SetMaterial(0, Material);
        ComboAttackMontage = GolemComboMontage;
        ChargedAttackMontage = nullptr;
    }
    else
    {
        const TArray<TObjectPtr<UMaterialInterface>>& Materials = TierIndex == 0 ? WoodLightMaterials :
            TierIndex == 1 ? WoodMediumMaterials : WoodHeavyMaterials;
        for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
        {
            if (Materials[Slot]) Body->SetMaterial(Slot, Materials[Slot]);
        }
        ComboAttackMontage = WoodComboMontage;
        ChargedAttackMontage = WoodChargedMontage;
    }
}
