#include "BusterProjectile.h"
#include "BusterTravelAppearance.h"
#include "QuasicomboLoadoutSubsystem.h"
#include "QuasicomboRunSubsystem.h"
#include "SideScrollingCharacter.h"
#include "SideScrollingCombatEnemy.h"
#include "CombatDamageable.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
UQuasicomboLoadoutSubsystem* Loadout(AActor* Actor)
{
	return Actor->GetGameInstance() ? Actor->GetGameInstance()->GetSubsystem<UQuasicomboLoadoutSubsystem>() : nullptr;
}
}

ABusterEnergyPulse::ABusterEnergyPulse()
{
	PrimaryActorTick.bCanEverTick = true;
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Energy"));
	SetRootComponent(Visual);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Visual->SetStaticMesh(Sphere.Object);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	EnergyMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Quasicombo/Mike/M_BusterEnergy.M_BusterEnergy")));
}
void ABusterEnergyPulse::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Base = EnergyMaterial.LoadSynchronous()) Material = Visual->CreateDynamicMaterialInstance(0, Base);
	if (auto* State = Loadout(this))
	{
		State->OnMikeLightColorChanged.AddDynamic(this, &ABusterEnergyPulse::ApplyColor);
		ApplyColor(State->GetMikeLightColor());
	}
	SetLifeSpan(0.3f);
}
void ABusterEnergyPulse::Initialize(FLinearColor Color, float Size) { Radius = Size; ApplyColor(Color); }
void ABusterEnergyPulse::ApplyColor(FLinearColor Color) { if (Material) Material->SetVectorParameterValue(TEXT("LightColor"), Color); }
void ABusterEnergyPulse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Visual->SetWorldScale3D(FVector(FMath::Lerp(0.08f, Radius / 50.0f, FMath::Clamp(Age / 0.3f, 0.f, 1.f))));
	if (Material) Material->SetScalarParameterValue(TEXT("Opacity"), 1.0f - Age / 0.3f);
}
void ABusterEnergyPulse::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* State = Loadout(this)) State->OnMikeLightColorChanged.RemoveDynamic(this, &ABusterEnergyPulse::ApplyColor);
	Super::EndPlay(Reason);
}

ABusterProjectile::ABusterProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileRoot"));
	SetRootComponent(SceneRoot);
	Visual = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ThunderBallTravel"));
	Visual->SetupAttachment(SceneRoot);
	// BP_ky_thunderBall's particle component faces along its local rotated axis.
	Visual->SetRelativeRotation(FRotator(90, 0, 0));
	Visual->bAutoActivate = false;
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TravelEffect = TSoftObjectPtr<UParticleSystem>(FSoftObjectPath(TEXT("/Game/Quasicombo/Buster/VFX/P_BusterThunderTravel.P_BusterThunderTravel")));
	ImpactEffect = TSoftObjectPtr<UParticleSystem>(FSoftObjectPath(TEXT("/Game/Quasicombo/Buster/VFX/P_BusterThunderImpact.P_BusterThunderImpact")));
}
void ABusterProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (auto* State = Loadout(this))
	{
		State->OnMikeLightColorChanged.AddDynamic(this, &ABusterProjectile::ApplyColor);
		State->OnMikeBodyPaletteChanged.AddDynamic(this, &ABusterProjectile::ApplyBodyPalette);
		ApplyColor(State->GetMikeLightColor());
		ApplyBodyPalette();
	}
}
void ABusterProjectile::Initialize(ASideScrollingCharacter* Player, FVector Direction, float Damage, float Speed, float Lifetime,
	FVector InVisualOffset, float InVisualScale, float InImpactScale,
	bool bInOriginalTravel, float InCoreIntensity, float InSparkIntensity)
{
	SetOwner(Player);
	SetInstigator(Player);
	Velocity = Direction.GetSafeNormal() * Speed;
	ShotDamage = Damage;
	VisualOffset = InVisualOffset;
	ImpactScale = InImpactScale;
	bOriginalTravel = bInOriginalTravel;
	TravelCoreIntensity = InCoreIntensity;
	TravelSparkIntensity = InSparkIntensity;
	SetActorRotation(Direction.Rotation());
	Visual->SetRelativeLocation(GetActorTransform().InverseTransformVectorNoScale(VisualOffset));
	Visual->SetRelativeScale3D(FVector(FMath::Max(0.01f, InVisualScale)));
	UParticleSystem* Effect = bOriginalTravel
		? LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FXVarietyPack/Particles/P_ky_thunderBall.P_ky_thunderBall"))
		: TravelEffect.LoadSynchronous();
	if (Effect)
	{
		Visual->SetTemplate(Effect);
		Visual->InstanceParameters.Reset();
		UpdateTravelColors();
		Visual->Activate(true);
	}
	SetLifeSpan(FMath::Max(0.01f, Lifetime));
}
void ABusterProjectile::ApplyColor(FLinearColor Color)
{
	LightColor = Color;
	UpdateTravelColors();
}

void ABusterProjectile::ApplyBodyPalette()
{
	if (auto* State = Loadout(this)) SparkColor = State->GetMikeSparkColor();
	UpdateTravelColors();
}
void ABusterProjectile::UpdateTravelColors()
{
	if (!Visual || bOriginalTravel) return;
	// Color parameters pack into 8-bit FColor; vector parameters retain HDR values.
	Visual->SetVectorParameter(TEXT("BusterCoreColor"), BusterTravel::ResolveTint(LightColor, true, TravelCoreIntensity));
	Visual->SetVectorParameter(TEXT("BusterSparkColor"), BusterTravel::ResolveTint(SparkColor, false, TravelSparkIntensity));
}
bool ABusterProjectile::SweepForImpact(UWorld* World, const FVector& Start, const FVector& End,
	float Radius, const AActor* Owner, const AActor* Projectile, FHitResult& OutHit)
{
	if (!World) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BusterBolt), false, Projectile);
	Params.AddIgnoredActor(Owner);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	FHitResult WorldHit, PawnHit;
	const bool bWorldHit = World->SweepSingleByChannel(WorldHit, Start, End, FQuat::Identity,
		ECC_Visibility, Shape, Params);
	FCollisionObjectQueryParams Pawns;
	Pawns.AddObjectTypesToQuery(ECC_Pawn);
	const bool bPawnHit = World->SweepSingleByObjectType(PawnHit, Start, End, FQuat::Identity,
		Pawns, Shape, Params);
	if (!bWorldHit && !bPawnHit) return false;
	OutHit = bPawnHit && (!bWorldHit || PawnHit.Time < WorldHit.Time) ? PawnHit : WorldHit;
	return true;
}
void ABusterProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bResolved) return;
	if (const auto* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>(); Run && Run->GetOutcome() != EQuasicomboRunOutcome::Playing) { Destroy(); return; }
	FHitResult Hit;
	const FVector End = GetActorLocation() + Velocity * DeltaSeconds;
	if (SweepForImpact(GetWorld(), GetActorLocation(), End, CollisionRadius, GetOwner(), this, Hit)) ResolveImpact(Hit);
	else SetActorLocation(End);
}
void ABusterProjectile::ResolveImpact(const FHitResult& Hit)
{
	if (bResolved) return;
	bResolved = true;
	AActor* Target = Hit.GetActor();
	if (Target && Target != GetOwner())
	{
		if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(Target))
		{
			const auto* Enemy = Cast<ACombatEnemy>(Target);
			const auto* Side = Cast<ASideScrollingCombatEnemy>(Target);
			const float BeforeHP = Enemy ? Enemy->CurrentHP : 0;
			const int32 BeforeHits = Side ? Side->GetAcceptedStrikes() : 0;
			Damageable->ApplyDamage(ShotDamage, this, Hit.ImpactPoint, Velocity.GetSafeNormal() * 180.0f);
			if (Side ? Side->GetAcceptedStrikes() > BeforeHits : Enemy ? Enemy->CurrentHP < BeforeHP : true)
                if (auto* Player = Cast<ASideScrollingCharacter>(GetInstigator()))
                {
                    Player->RecordCombatActivity();
                    Player->RegisterHitCombo();
                }
		}
	}
	if (UParticleSystem* Effect = ImpactEffect.LoadSynchronous())
		if (UParticleSystemComponent* Impact = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Effect,
			FTransform(GetActorRotation(), Hit.ImpactPoint + VisualOffset, FVector(FMath::Max(0.01f, ImpactScale))),
			true, EPSCPoolMethod::None, false))
		{
			Impact->SetColorParameter(TEXT("BusterCoreColor"), LightColor);
			Impact->SetColorParameter(TEXT("BusterSparkColor"), SparkColor);
			Impact->Activate(true);
		}
	Destroy();
}
void ABusterProjectile::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* State = Loadout(this))
	{
		State->OnMikeLightColorChanged.RemoveDynamic(this, &ABusterProjectile::ApplyColor);
		State->OnMikeBodyPaletteChanged.RemoveDynamic(this, &ABusterProjectile::ApplyBodyPalette);
	}
	Super::EndPlay(Reason);
}
