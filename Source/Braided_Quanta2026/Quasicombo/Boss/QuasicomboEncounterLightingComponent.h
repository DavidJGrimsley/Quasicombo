#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuasicomboEncounterLightingComponent.generated.h"

class ADirectionalLight;
class ULightComponent;

UCLASS(ClassGroup = (Quasicombo), meta = (BlueprintSpawnableComponent))
class UQuasicomboEncounterLightingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UQuasicomboEncounterLightingComponent();
	/** Optional explicit sun. Otherwise use the brightest visible non-static directional light. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Encounter Lighting")
	TObjectPtr<ADirectionalLight> DirectionalLight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Lighting", meta = (ClampMin = "0.0"))
	float CombatIntensity = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Lighting", meta = (ClampMin = "0.0"))
	float EvolutionIntensity = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Lighting", meta = (ClampMin = "0.0"))
	float PostEvolutionIntensity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Lighting", meta = (ClampMin = "0.0"))
	float CombatFadeSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter Lighting", meta = (ClampMin = "0.0"))
	float EvolutionFadeSeconds = 1.0f;
	void BeginCombat();
	void BeginEvolution();
	void EndEvolution();
	void Restore();
	/** Keep reward lighting after the sixth-beat explosion removes the boss. */
	void TransferToRewardOwner(AActor* RewardOwner);
	/** Returns false once combat has taken ownership. Captures the baseline before the first approach write. */
	bool SetApproachProgress(float Progress, float StartIntensity, float EndIntensity);
	bool HasEncounterControl() const { return bCombatStarted; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	TWeakObjectPtr<ULightComponent> CapturedLight;
	float OriginalIntensity = 0.0f;
	float FadeFrom = 0.0f;
	float FadeTarget = 0.0f;
	float FadeElapsed = 0.0f;
	float FadeDuration = 1.0f;
	bool bEvolutionStarted = false;
	bool bCombatStarted = false;
	bool CaptureLight();
	void FadeTo(float Intensity, float Seconds);
};
