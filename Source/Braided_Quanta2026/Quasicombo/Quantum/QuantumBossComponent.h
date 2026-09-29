#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuantumApiTypes.h"
#include "QuasicomboRouteTypes.h"
#include "QuantumBossComponent.generated.h"

class FQuantumApiClient;
UENUM(BlueprintType)
enum class EQuasicomboQuantumUpdate : uint8 { Braid, Evolution };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FQuasicomboQuantumUpdated, EQuasicomboQuantumUpdate, Reason, double, PreviousTau, double, NewTau, bool, bFallback);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FQuasicomboQuantumStateReady, double, VacuumProbability, double, TauProbability, bool, bFallback);

UCLASS(ClassGroup=(Quasicombo), meta=(BlueprintSpawnableComponent))
class UQuantumBossComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable, Category="Quasicombo|Quantum") FQuasicomboQuantumUpdated OnQuantumUpdated;
	UFUNCTION(BlueprintPure, Category="Quasicombo|Quantum") bool HasEvolutionResolved() const { return bEvolutionResolved; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Quantum") double GetPreEvolutionTau() const { return PreEvolutionTau; }
	void TimeoutEvolution();
	void ResumeAfterRetry();
	UPROPERTY(BlueprintAssignable, Category="Quasicombo|Quantum") FQuasicomboQuantumStateReady OnQuantumStateReady;
	/** Useful for offline play and deterministic automation; route fixtures supply the braid state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Quantum") bool bUseLiveQuantumApi = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Quantum") double BaseX = 0.3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Quantum") double BaseZ = 0.2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Quantum") double ComboXPerHit = 0.025;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Quantum") double DamageZPerHP = 0.035;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quasicombo|Quantum") double EvolutionTime = 0.5;
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Quantum") void EvaluateBraid();
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Quantum") void EvolveState();
	UFUNCTION(BlueprintPure, Category="Quasicombo|Quantum") double GetTauProbability() const { return TauProbability; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Quantum") double GetVacuumProbability() const { return VacuumProbability; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Quantum") bool HasQuantumState() const { return State.Num() == 2; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Quantum") bool UsedFallback() const { return bFallback; }
	UFUNCTION(BlueprintPure, Category="Quasicombo|Quantum") FString GetEvolutionFailureReason() const { return EvolutionFailureReason; }
	/** Cancels logical ownership of late HTTP callbacks when the finisher begins. */
	UFUNCTION(BlueprintCallable, Category="Quasicombo|Quantum") void FreezeForFinisher();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION() void HandleRouteCommitted(int32 Section, EQuasicomboLane Lane);
private:
	TSharedPtr<FQuantumApiClient> Client;
	TArray<FQuantumApiComplexAmplitude> State;
	double VacuumProbability = 1.0;
	double TauProbability = 0.0;
	int32 BraidGeneration = 0;
	int32 EvolutionGeneration = 0;
	bool bBraidStarted = false;
	bool bFallback = false;
	bool bEvolutionStarted = false;
	bool bEvolutionResolved = false;
	FString EvolutionFailureReason;
	double PreEvolutionTau = 0.0;
	bool bFrozen = false;
	void ApplyFixture();
	bool SetState(const TArray<FQuantumApiComplexAmplitude>& NewState, bool bUsedFallback,
		const TArray<double>* ReportedProbabilities = nullptr);
	bool IsRunActive() const;
	void RetainStateAfterEvolutionFailure(const FString& Reason);
};
