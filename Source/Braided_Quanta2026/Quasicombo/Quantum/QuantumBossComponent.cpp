#include "QuantumBossComponent.h"
#include "QuasicomboBraidFixtures.h"
#include "QuasicomboQuantumRules.h"
#include "QuasicomboRunSubsystem.h"
#include "QuantumApiClient.h"
#include "QuantumApiSettings.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

UQuantumBossComponent::UQuantumBossComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}
void UQuantumBossComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction *TickFunction)
{
	Super::TickComponent(Delta, TickType, TickFunction);
	if (bEvolutionStarted && !bEvolutionResolved && !bFrozen &&
		FPlatformTime::Seconds() - EvolutionRequestStartedAt >= BossRequestTimeoutSeconds)
		TimeoutEvolution();
}
void UQuantumBossComponent::EnsureState()
{
	if (State.Num() != 2)
		ApplyFixture();
	// Encounter presentation and evolution share this captured starting state.
	// A pending braid response must not replace it after combat has begun.
	++BraidGeneration;
	bBraidStarted = true;
}
void UQuantumBossComponent::InitializePreview(double Tau)
{
	bUseLiveQuantumApi = false;
	bBraidStarted = true;
	++BraidGeneration;
	TArray<FQuantumApiComplexAmplitude> Initial;
	Initial.SetNum(2);
	Initial[0].Real = FMath::Sqrt(1.0 - FMath::Clamp(Tau, 0.0, 1.0));
	Initial[1].Real = FMath::Sqrt(FMath::Clamp(Tau, 0.0, 1.0));
	SetState(Initial, false);
}
void UQuantumBossComponent::EvolvePreview(double Tau)
{
	if (bEvolutionStarted || bFrozen || !IsRunActive())
		return;
	EnsureState();
	PreEvolutionTau = TauProbability;
	bEvolutionStarted = true;
	++BraidGeneration;
	++EvolutionGeneration;
	EvolutionSource = EQuasicomboEvolutionSource::Preview;
	TArray<FQuantumApiComplexAmplitude> Result;
	Result.SetNum(2);
	Result[0].Real = FMath::Sqrt(1.0 - FMath::Clamp(Tau, 0.0, 1.0));
	Result[1].Real = FMath::Sqrt(FMath::Clamp(Tau, 0.0, 1.0));
	SetState(Result, false);
}

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
	// Ending a run freezes even bosses that have not entered combat yet.
	// There is no state to initialize after that run has ended.
	if (bFrozen || !IsRunActive())
		return;
	UQuasicomboRunSubsystem *Run = GetWorld() ? GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>() : nullptr;
	if (!Run)
		return;
	TArray<EQuasicomboLane> History = Run->GetRouteHistory();
	// A manually positioned PlayerStart may reach the arena before any route.
	// Use the existing fixture, filling unplayed legs with the default A lane.
	while (History.Num() < 3)
		History.Add(EQuasicomboLane::A);
	FQuasicomboBraidFixture Fixture;
	if (!TryGetQuasicomboBraidFixture(History, Fixture))
		return;
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
	if (bEvolutionStarted) bEvolutionResolved = true;
	OnQuantumStateReady.Broadcast(VacuumProbability, TauProbability, bFallback);
	OnQuantumUpdated.Broadcast(bEvolutionStarted ? EQuasicomboQuantumUpdate::Evolution : EQuasicomboQuantumUpdate::Braid,
		bEvolutionStarted ? PreEvolutionTau : TauProbability, TauProbability, bFallback);
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
	FQuantumApiRequestOptions Options;
	Options.TimeoutSecondsOverride = BossRequestTimeoutSeconds;
	RequestClient->EvaluateTopologicalBraidTyped(
		Request, Options,
		FQuantumApiTopologicalBraidDelegate::CreateLambda([WeakThis, RequestGeneration,
														   RequestClient](const FQuantumApiTopologicalBraidResponse &Response) {
			if (!RequestClient)
				return;
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
		FQuantumApiErrorDelegate::CreateLambda([WeakThis, RequestGeneration, RequestClient](const FQuantumApiError &Error) {
			if (!RequestClient)
				return;
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
	if (bFrozen || bEvolutionStarted || !IsRunActive())
		return;
	if (State.Num() != 2)
		EnsureState();
	if (State.Num() != 2)
		return;
	bEvolutionStarted = true;
	EvolutionFailureReason.Empty();
	PreEvolutionTau = TauProbability;
	++BraidGeneration;
	UQuasicomboRunSubsystem *Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>();
	FQuantumApiTimeEvolutionRequest Request;
	Request.InitialStatevector = State;
	FQuantumApiPauliTerm X; X.Pauli = TEXT("X"); X.Coefficient = FMath::Clamp(BaseX + (Run ? Run->GetHighestCombo() : 0) * ComboXPerHit, -1.0, 1.0);
	FQuantumApiPauliTerm Z; Z.Pauli = TEXT("Z"); Z.Coefficient = FMath::Clamp(BaseZ + (Run ? Run->GetDamageTaken() : 0) * DamageZPerHP, -1.0, 1.0);
	Request.Hamiltonian = { X, Z };
	Request.Time = FMath::Max(0.001, EvolutionTime);
	Request.NumTimesteps = 2;
	Request.Shots = 1;
	EvolutionSnapshot = Request;
	EvolutionRequestStartedAt = FPlatformTime::Seconds();
	if (!bUseLiveQuantumApi || !Client)
	{
		ResolveLocalEvolution(TEXT("offline mode"));
		return;
	}
	const int32 RequestGeneration = ++EvolutionGeneration;
	const double RequestStartedAt = FPlatformTime::Seconds();
	const TWeakObjectPtr<UQuantumBossComponent> WeakThis(this);
	const TSharedPtr<FQuantumApiClient> RequestClient = Client;
	FQuantumApiRequestOptions Options;
	Options.TimeoutSecondsOverride = BossRequestTimeoutSeconds;
	RequestClient->RunTimeEvolution(
		Request, Options,
		FQuantumApiTimeEvolutionDelegate::CreateLambda([WeakThis, RequestGeneration, RequestStartedAt,
														RequestClient](const FQuantumApiTimeEvolutionResponse &Response) {
			if (!RequestClient)
				return;
			const double Elapsed = FPlatformTime::Seconds() - RequestStartedAt;
			if (UQuantumBossComponent* Self = WeakThis.Get())
			{
				if (!QuasicomboQuantumRules::CanAcceptEvolution(RequestGeneration, Self->EvolutionGeneration,
					Self->bFrozen, Self->IsRunActive()))
				{
					UE_LOG(LogTemp, Warning, TEXT("Quasicombo: late evolution response ignored (HTTP %d, %.2fs)"), Response.Meta.StatusCode,
						   Elapsed);
					return;
				}
				if (Elapsed >= Self->BossRequestTimeoutSeconds)
				{
					Self->TimeoutEvolution();
					return;
				}
				Self->EvolutionSource = EQuasicomboEvolutionSource::Live;
				if (!Self->SetState(Response.FinalStatevector, false, &Response.FinalProbabilities))
					Self->ResolveLocalEvolution(TEXT("invalid response"));
				else
				{
					++Self->EvolutionGeneration;
				}
			}
		}),
		FQuantumApiErrorDelegate::CreateLambda([WeakThis, RequestGeneration, RequestStartedAt,
												RequestClient](const FQuantumApiError &Error) {
			if (!RequestClient)
				return;
			UE_LOG(LogTemp, Warning, TEXT("Quasicombo: evolution API error %s (HTTP %d, %.2fs)"),
				*Error.Error, Error.StatusCode, FPlatformTime::Seconds() - RequestStartedAt);
			if (UQuantumBossComponent* Self = WeakThis.Get(); Self &&
				QuasicomboQuantumRules::CanAcceptEvolution(RequestGeneration, Self->EvolutionGeneration,
					Self->bFrozen, Self->IsRunActive()))
			{
				Self->ResolveLocalEvolution(Error.Error);
			}
		}));
}

void UQuantumBossComponent::ResolveLocalEvolution(const FString &Reason)
{
	if (bEvolutionResolved || bFrozen || !IsRunActive())
		return;
	EvolutionFailureReason = Reason;
	++EvolutionGeneration;
	++BraidGeneration;
	TArray<FQuantumApiComplexAmplitude> Local;
	double X = 0.0, Z = 0.0;
	for (const auto &Term : EvolutionSnapshot.Hamiltonian)
	{
		if (Term.Pauli == TEXT("X"))
			X = Term.Coefficient;
		if (Term.Pauli == TEXT("Z"))
			Z = Term.Coefficient;
	}
	EvolutionSource = EQuasicomboEvolutionSource::Local;
	if (QuasicomboQuantumRules::EvolveLocally(EvolutionSnapshot.InitialStatevector, X, Z, EvolutionSnapshot.Time, Local) &&
		SetState(Local, true))
	{
		return;
	}
	// Invalid inputs never fabricate a quantum state.
	bFallback = true;
	bEvolutionResolved = true;
	OnQuantumStateReady.Broadcast(VacuumProbability, TauProbability, true);
	OnQuantumUpdated.Broadcast(EQuasicomboQuantumUpdate::Evolution, PreEvolutionTau, TauProbability, true);
}

void UQuantumBossComponent::TimeoutEvolution()
{
	if (bEvolutionResolved || bFrozen) return;
	++BraidGeneration;
	++EvolutionGeneration;
	if (!bEvolutionStarted) { PreEvolutionTau = TauProbability; bEvolutionStarted = true; }
	ResolveLocalEvolution(TEXT("encounter deadline"));
}

void UQuantumBossComponent::ResumeAfterRetry()
{
	++BraidGeneration;
	++EvolutionGeneration;
	bFrozen = false;
	if (!bEvolutionResolved) bEvolutionStarted = false;
	if (State.Num() != 2) ApplyFixture();
}

void UQuantumBossComponent::FreezeForFinisher()
{
	if (State.Num() != 2) ApplyFixture();
	bFrozen = true;
	++BraidGeneration;
	++EvolutionGeneration;
}
