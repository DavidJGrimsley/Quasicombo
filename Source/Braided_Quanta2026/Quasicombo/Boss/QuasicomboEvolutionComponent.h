#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SlateWrapperTypes.h"
#include "Camera/CameraTypes.h"
#include "QuasicomboEvolutionComponent.generated.h"

class AQuasicomboBoss;
class ACameraActor;
class UNiagaraSystem;
class UNiagaraComponent;
class UAnimSequence;
class USoundBase;
class USkeletalMeshComponent;
class UPoseableMeshComponent;
class UUserWidget;

UENUM(BlueprintType)
enum class EQuasicomboEvolutionStage : uint8
{
	Inactive,
	Rise,
	Hover,
	FaceReveal,
	ArmorAssembly,
	Landing,
	Roar,
	Return,
	Smirk UMETA(Hidden), // Legacy value retained for serialized stage references; no longer played.
	Stumble,
	IdleRecovery
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEvolutionStageEvent, EQuasicomboEvolutionStage, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEvolutionAttachmentEvent, int32, PieceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEvolutionImpactEvent);

USTRUCT()
struct FQuasicomboArmorDebris
{
	GENERATED_BODY()
	UPROPERTY()
	TObjectPtr<UPoseableMeshComponent> Mesh;
	FVector LocalCenter = FVector::ZeroVector;
	FVector LocalExtent = FVector(10.0f);
	FVector Velocity = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
	int32 Bounces = 0;
	float Age = 0.0f;
	bool bSettled = false;
};

UCLASS(ClassGroup = (Quasicombo), meta = (BlueprintSpawnableComponent))
class UQuasicomboEvolutionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UQuasicomboEvolutionComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float StumbleSeconds = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float IdleRecoverySeconds = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float LiftHeight = 165.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float RiseSeconds = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float HoverSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float FaceRevealSeconds = 3.0f;
	/** Hold the closest framing after the eye flickers finish, before armor pullback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution", meta = (ClampMin = "0.0"))
	float FaceHoldSeconds = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float RoarSeconds = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float ReturnSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution")
	float DurationScale = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Evolution|VFX")
	TArray<TSoftObjectPtr<UNiagaraSystem>> Auras;
	/** Opposing tilts around the boss's forward axis; both rings share the body center. */
	UPROPERTY(EditAnywhere, Category = "Evolution|VFX", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float FireRingTiltDegrees = 45.0f;
	/** Optional override; otherwise the opening of the boss's existing defeat clip is used. */
	UPROPERTY(EditAnywhere, Category = "Evolution|Animation")
	TSoftObjectPtr<UAnimSequence> StumbleAnimation;
	UPROPERTY(EditAnywhere, Category = "Evolution|Animation")
	TSoftObjectPtr<UAnimSequence> RiseAnimation;
	UPROPERTY(EditAnywhere, Category = "Evolution|Animation")
	TSoftObjectPtr<UAnimSequence> HoverAnimation;
	UPROPERTY(EditAnywhere, Category = "Evolution|Animation")
	TSoftObjectPtr<UAnimSequence> AttachmentAnimation;
	UPROPERTY(EditAnywhere, Category = "Evolution|Animation")
	TSoftObjectPtr<UAnimSequence> RoarAnimation;
	UPROPERTY(EditAnywhere, Category = "Evolution|Sound")
	TObjectPtr<USoundBase> PowerUpSound;
	UPROPERTY(EditAnywhere, Category = "Evolution|Sound")
	TObjectPtr<USoundBase> AttachmentSound;
	UPROPERTY(EditAnywhere, Category = "Evolution|Sound")
	TObjectPtr<USoundBase> LandingSound;
	UPROPERTY(EditAnywhere, Category = "Evolution|Sound")
	TObjectPtr<USoundBase> RoarSound;
	UPROPERTY(BlueprintAssignable)
	FEvolutionStageEvent OnStageChanged;
	UPROPERTY(BlueprintAssignable)
	FEvolutionAttachmentEvent OnArmorAttached;
	UPROPERTY(BlueprintAssignable)
	FEvolutionImpactEvent OnLanded;
	UPROPERTY(BlueprintAssignable)
	FEvolutionImpactEvent OnRoar;
	void Start();
	UFUNCTION(BlueprintCallable, Category = "Evolution")
	void Cancel();
	void BreakArmorGroup(int32 Group, const FVector& Impulse);
	void ClearDebris();
	UFUNCTION(BlueprintPure)
	EQuasicomboEvolutionStage GetStage() const { return Stage; }
	UFUNCTION(BlueprintPure)
	int32 GetDebrisCount() const { return Debris.Num(); }
	UFUNCTION(BlueprintPure)
	UNiagaraComponent* GetActiveAura() const { return Aura; }
	UFUNCTION(BlueprintPure)
	UNiagaraComponent* GetStackedAura() const { return UpperAura; }
	bool HasAttached(int32 Piece) const { return Attached.Contains(Piece); }
	float GetAttachmentAge() const { return AttachmentAge; }
	float GetStageAge() const { return Age; }
	int32 GetAttachedCount() const { return Attached.Num(); }
	int32 GetAssemblyPieceCount() const { return FlightPieces.Num(); }

protected:
	virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Aura;
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> UpperAura;
	TArray<TWeakObjectPtr<UUserWidget>> CombatHUDs;
	TArray<ESlateVisibility> CombatHUDVisibility;
	UPROPERTY()
	TArray<FQuasicomboArmorDebris> Debris;
	EQuasicomboEvolutionStage Stage = EQuasicomboEvolutionStage::Inactive;
	TArray<int32> FlightPieces;
	TSet<int32> Attached;
	FTransform GroundTransform;
	float Age = 0.0f;
	float AssemblySeconds = 2.0f;
	float AttachmentAge = 1.0f;
	bool bResolved = false;
	bool bLanded = false;
	bool bReturningCamera = false;
	FMinimalViewInfo ReturnStartView;
	FMinimalViewInfo ReturnEndView;
	AQuasicomboBoss* Boss() const;
	void SetStage(EQuasicomboEvolutionStage Next);
	void Play(TSoftObjectPtr<UAnimSequence> Animation, float Seconds, bool bLoop = false);
	void UpdateCamera();
	void TickDebris(float Delta);
	void Sound(USoundBase* Asset);
};
