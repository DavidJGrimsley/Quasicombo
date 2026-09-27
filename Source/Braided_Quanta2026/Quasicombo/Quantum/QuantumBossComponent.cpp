#include "QuantumBossComponent.h"
#include "QuasicomboBraidFixtures.h"
#include "QuasicomboQuantumRules.h"
#include "QuasicomboRunSubsystem.h"
#include "QuantumApiClient.h"
#include "QuantumApiSettings.h"
#include "Engine/World.h"

void UQuantumBossComponent::BeginPlay()
{
	Super::BeginPlay();
	Client = MakeShared<FQuantumApiClient>(GetDefault<UQuantumApiSettings>());
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		Run->OnRouteCommitted.AddDynamic(this, &UQuantumBossComponent::HandleRouteCommitted);
		if (Run->GetCurrentSection() == 3) EvaluateBraid();
	}
}

void UQuantumBossComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++BraidGeneration;
	++EvolutionGeneration;
	bFrozen = true;
	if (UWorld* World = GetWorld())
	{
		if (UQuasicomboRunSubsystem* Run = World->GetSubsystem<UQuasicomboRunSubsystem>())
		{
			Run->OnRouteCommitted.RemoveDynamic(this, &UQuantumBossComponent::HandleRouteCommitted);
		}
	}
	// In-flight callbacks retain their own client reference until completion.
	Client.Reset();
	Super::EndPlay(EndPlayReason);
}

void UQuantumBossComponent::HandleRouteCommitted(int32 Section, EQuasicomboLane)
{
	if (Section == 3) EvaluateBraid();
}

void UQuantumBossComponent::ApplyFixture()
{
	UQuasicomboRunSubsystem* Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (!Run) return;
	const TArray<EQuasicomboLane> History = Run->GetRouteHistory();
	FQuasicomboBraidFixture Fixture;
	if (!TryGetQuasicomboBraidFixture(History, Fixture)) return;
	TArray<FQuantumApiComplexAmplitude> FallbackState;
	FQuantumApiComplexAmplitude Vacuum; Vacuum.Real = Fixture.VacuumReal; Vacuum.Imag = Fixture.VacuumImag;
	FQuantumApiComplexAmplitude Tau; Tau.Real = Fixture.TauReal; Tau.Imag = Fixture.TauImag;
	FallbackState.Add(Vacuum);
	FallbackState.Add(Tau);
	if (!SetState(FallbackState, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Quasicombo: validated braid fixture was rejected"));
	}
}

bool UQuantumBossComponent::IsRunActive() const
{
	UWorld* World = GetWorld();
	const UQuasicomboRunSubsystem* Run = World ? World->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	return Run && Run->GetOutcome() == EQuasicomboRunOutcome::Playing;
}

bool UQuantumBossComponent::SetState(const TArray<FQuantumApiComplexAmplitude>& NewState, bool bUsedFallback,
	const TArray<double>* ReportedProbabilities)
{
	if (NewState.Num() != 2 || bFrozen || !IsRunActive()) return false;
	FQuasicomboValidatedQuantumState Validated;
	if (!QuasicomboQuantumRules::TryValidateState(NewState, ReportedProbabilities, Validated)) return false;
	State = NewState;
	VacuumProbability = Validated.VacuumProbability;
	TauProbability = Validated.TauProbability;
	bFallback = bUsedFallback;
	OnQuantumStateReady.Broadcast(VacuumProbability, TauProbability, bFallback);
	return true;
}

void UQuantumBossComponent::EvaluateBraid()
{
	if (bFrozen || bBraidStarted || bEvolutionStarted || !IsRunActive()) return;
	UQuasicomboRunSubsystem* Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (!Run || Run->GetCurrentSection() != 3) return;
	bBraidStarted = true;
	const int32 RequestGeneration = ++BraidGeneration;
	if (!bUseLiveQuantumApi || !Client)
	{
		ApplyFixture();
		return;
	}
	FQuantumApiTopologicalBraidRequest Request;
	for (const FQuasicomboBraidOperation& Operation : Run->GetBraidOperations())
	{
		FQuantumApiBraidOperation ApiOperation;
		ApiOperation.Generator = Operation.Generator;
		ApiOperation.Power = Operation.Power;
		Request.BraidWord.Add(ApiOperation);
	}
	const TWeakObjectPtr<UQuantumBossComponent> WeakThis(this);
	const TSharedPtr<FQuantumApiClient> RequestClient = Client;
	RequestClient->EvaluateTopologicalBraidTyped(Request, FQuantumApiRequestOptions{},
		FQuantumApiTopologicalBraidDelegate::CreateLambda([WeakThis, RequestGeneration, RequestClient](const FQuantumApiTopologicalBraidResponse& Response)
		{
			if (!RequestClient) return;
			if (UQuantumBossComponent* Self = WeakThis.Get(); Self &&
				QuasicomboQuantumRules::CanAcceptBraid(RequestGeneration, Self->BraidGeneration,
					Self->bFrozen, Self->bEvolutionStarted, Self->IsRunActive()))
			{
				const TArray<double> ReportedProbabilities = { Response.VacuumProbability, Response.TauProbability };
				if (!Self->SetState(Response.LogicalState, false, &ReportedProbabilities))
				{
					UE_LOG(LogTemp, Warning, TEXT("Quasicombo: invalid braid response; using route fixture"));
					Self->ApplyFixture();
				}
			}
		}),
		FQuantumApiErrorDelegate::CreateLambda([WeakThis, RequestGeneration, RequestClient](const FQuantumApiError& Error)
		{
			if (!RequestClient) return;
			if (UQuantumBossComponent* Self = WeakThis.Get(); Self &&
				QuasicomboQuantumRules::CanAcceptBraid(RequestGeneration, Self->BraidGeneration,
					Self->bFrozen, Self->bEvolutionStarted, Self->IsRunActive()))
			{
				UE_LOG(LogTemp, Warning, TEXT("Quasicombo: braid request failed (%s); using route fixture"), *Error.Error);
				Self->ApplyFixture();
			}
		}));
}

void UQuantumBossComponent::EvolveState()
{
	if (bFrozen || bEvolutionStarted || !IsRunActive()) return;
	if (State.Num() != 2) ApplyFixture();
	if (State.Num() != 2) return;
	bEvolutionStarted = true;
	++BraidGeneration;
	if (!bUseLiveQuantumApi || !Client)
	{
		RetainStateAfterEvolutionFailure(TEXT("offline mode"));
		return;
	}
	UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	FQuantumApiTimeEvolutionRequest Request;
	Request.InitialStatevector = State;
	FQuantumApiPauliTerm X; X.Pauli = TEXT("X"); X.Coefficient = FMath::Clamp(BaseX + (Run ? Run->GetHighestCombo() : 0) * ComboXPerHit, -1.0, 1.0);
	FQuantumApiPauliTerm Z; Z.Pauli = TEXT("Z"); Z.Coefficient = FMath::Clamp(BaseZ + (Run ? Run->GetDamageTaken() : 0) * DamageZPerHP, -1.0, 1.0);
	Request.Hamiltonian = { X, Z };
	Request.Time = FMath::Max(0.001, EvolutionTime);
	Request.NumTimesteps = 2;
	Request.Shots = 1;
	const int32 RequestGeneration = ++EvolutionGeneration;
	const TWeakObjectPtr<UQuantumBossComponent> WeakThis(this);
	const TSharedPtr<FQuantumApiClient> RequestClient = Client;
	RequestClient->RunTimeEvolution(Request, FQuantumApiRequestOptions{},
		FQuantumApiTimeEvolutionDelegate::CreateLambda([WeakThis, RequestGeneration, RequestClient](const FQuantumApiTimeEvolutionResponse& Response)
		{
			if (!RequestClient) return;
			if (UQuantumBossComponent* Self = WeakThis.Get(); Self &&
				QuasicomboQuantumRules::CanAcceptEvolution(RequestGeneration, Self->EvolutionGeneration,
					Self->bFrozen, Self->IsRunActive()) &&
				!Self->SetState(Response.FinalStatevector, false, &Response.FinalProbabilities))
			{
				Self->RetainStateAfterEvolutionFailure(TEXT("invalid response"));
			}
		}),
		FQuantumApiErrorDelegate::CreateLambda([WeakThis, RequestGeneration, RequestClient](const FQuantumApiError& Error)
		{
			if (!RequestClient) return;
			if (UQuantumBossComponent* Self = WeakThis.Get(); Self &&
				QuasicomboQuantumRules::CanAcceptEvolution(RequestGeneration, Self->EvolutionGeneration,
					Self->bFrozen, Self->IsRunActive()))
			{
				Self->RetainStateAfterEvolutionFailure(Error.Error);
			}
		}));
}

void UQuantumBossComponent::RetainStateAfterEvolutionFailure(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("Quasicombo: evolution failed (%s); retaining last valid complex state"), *Reason);
	bFallback = true;
	OnQuantumStateReady.Broadcast(VacuumProbability, TauProbability, true);
}

void UQuantumBossComponent::FreezeForFinisher()
{
	if (State.Num() != 2) ApplyFixture();
	bFrozen = true;
	++BraidGeneration;
	++EvolutionGeneration;
}
