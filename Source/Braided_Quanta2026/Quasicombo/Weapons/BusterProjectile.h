#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BusterProjectile.generated.h"

class ASideScrollingCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UParticleSystem;
class UParticleSystemComponent;

/** Short-lived emissive pulse, also used for cannon attachment and landing. */
UCLASS()
class ABusterEnergyPulse : public AActor
{
	GENERATED_BODY()
public:
	ABusterEnergyPulse();
	void Initialize(FLinearColor Color, float Size = 45.0f);
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UFUNCTION() void ApplyColor(FLinearColor Color);
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY() TSoftObjectPtr<UMaterialInterface> EnergyMaterial;
	float Age = 0;
	float Radius = 45;
};

/** Swept, single-impact bolt. The actor is the impact identity; its instigator is Mike. */
UCLASS()
class ABusterProjectile : public AActor
{
	GENERATED_BODY()
public:
	static bool SweepForImpact(UWorld* World, const FVector& Start, const FVector& End,
		float Radius, const AActor* Owner, const AActor* Projectile, FHitResult& OutHit);
	ABusterProjectile();
	void Initialize(ASideScrollingCharacter* Player, FVector Direction, float Damage, float Speed, float Lifetime,
		FVector InVisualOffset = FVector::ZeroVector, float InVisualScale = 1.0f, float InImpactScale = 1.0f,
		bool bInOriginalTravel = false, float InCoreIntensity = 1.0f, float InSparkIntensity = 1.0f);
	virtual void Tick(float DeltaSeconds) override;
	void ResolveImpact(const FHitResult& Hit);
	UPROPERTY(EditAnywhere, Category="Buster Cannon") float CollisionRadius = 8.0f;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UFUNCTION() void ApplyColor(FLinearColor Color);
	UFUNCTION() void ApplyBodyPalette();
private:
	UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category="Buster Cannon|VFX") TObjectPtr<UParticleSystemComponent> Visual;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX") TSoftObjectPtr<UParticleSystem> TravelEffect;
	UPROPERTY(EditAnywhere, Category="Buster Cannon|VFX") TSoftObjectPtr<UParticleSystem> ImpactEffect;
	FVector Velocity = FVector::ZeroVector;
	FVector VisualOffset = FVector::ZeroVector;
	FLinearColor LightColor = FLinearColor(0.175f, 0.367f, 1.0f);
	FLinearColor SparkColor = FLinearColor(0.175f, 0.367f, 1.0f);
	float ShotDamage = 2;
	float ImpactScale = 1.0f;
	float TravelCoreIntensity = 1.0f;
	float TravelSparkIntensity = 1.0f;
	bool bOriginalTravel = false;
	void UpdateTravelColors();
	bool bResolved = false;
};
