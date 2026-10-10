#include "QuasicomboEncounterLightingComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/LightComponent.h"

UQuasicomboEncounterLightingComponent::UQuasicomboEncounterLightingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UQuasicomboEncounterLightingComponent::CaptureLight()
{
	if (CapturedLight.IsValid()) return true;
	ULightComponent* Sun = IsValid(DirectionalLight) ? DirectionalLight->GetLightComponent() : nullptr;
	if (!Sun && GetWorld())
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			ULightComponent* Candidate = It->GetLightComponent();
			if (Candidate && Candidate->bAffectsWorld && Candidate->IsVisible() &&
				Candidate->Mobility != EComponentMobility::Static && (!Sun || Candidate->Intensity > Sun->Intensity))
				Sun = Candidate;
		}
	}
	if (!Sun || Sun->Mobility == EComponentMobility::Static) return false;
	CapturedLight = Sun;
	OriginalIntensity = Sun->Intensity;
	return true;
}

void UQuasicomboEncounterLightingComponent::FadeTo(float Intensity, float Seconds)
{
	if (!CaptureLight()) return;
	FadeFrom = CapturedLight->Intensity;
	FadeTarget = FMath::Max(0.0f, Intensity);
	FadeElapsed = 0.0f;
	FadeDuration = FMath::Max(0.0f, Seconds);
	if (FadeDuration <= 0.0f || FMath::IsNearlyEqual(FadeFrom, FadeTarget))
	{
		CapturedLight->SetIntensity(FadeTarget);
		SetComponentTickEnabled(false);
	}
	else SetComponentTickEnabled(true);
}

void UQuasicomboEncounterLightingComponent::BeginCombat()
{
	bCombatStarted = true;
	// Retry keeps the post-transformation lighting, including after an interrupted cinematic.
	FadeTo(bEvolutionStarted ? PostEvolutionIntensity : CombatIntensity,
		bEvolutionStarted ? EvolutionFadeSeconds : CombatFadeSeconds);
}

void UQuasicomboEncounterLightingComponent::BeginEvolution()
{
	bCombatStarted = true;
	bEvolutionStarted = true;
	FadeTo(EvolutionIntensity, EvolutionFadeSeconds);
}

void UQuasicomboEncounterLightingComponent::EndEvolution()
{
	if (bEvolutionStarted && bCombatStarted) FadeTo(PostEvolutionIntensity, EvolutionFadeSeconds);
}

void UQuasicomboEncounterLightingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!CapturedLight.IsValid()) { SetComponentTickEnabled(false); return; }
	FadeElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(FadeElapsed / FadeDuration, 0.0f, 1.0f);
	const float Eased = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	CapturedLight->SetIntensity(FMath::Lerp(FadeFrom, FadeTarget, Eased));
	if (Alpha >= 1.0f) SetComponentTickEnabled(false);
}

void UQuasicomboEncounterLightingComponent::Restore()
{
	SetComponentTickEnabled(false);
	if (CapturedLight.IsValid()) CapturedLight->SetIntensity(OriginalIntensity);
	CapturedLight.Reset();
	bEvolutionStarted = false;
	bCombatStarted = false;
}

void UQuasicomboEncounterLightingComponent::TransferToRewardOwner(AActor* RewardOwner)
{
	if (!RewardOwner || !CapturedLight.IsValid()) return;
	auto* RewardLighting = NewObject<UQuasicomboEncounterLightingComponent>(RewardOwner);
	RewardOwner->AddInstanceComponent(RewardLighting);
	RewardLighting->RegisterComponent();
	RewardLighting->CapturedLight = CapturedLight;
	RewardLighting->OriginalIntensity = OriginalIntensity;
	RewardLighting->FadeFrom = FadeFrom;
	RewardLighting->FadeTarget = FadeTarget;
	RewardLighting->FadeElapsed = FadeElapsed;
	RewardLighting->FadeDuration = FadeDuration;
	RewardLighting->bEvolutionStarted = bEvolutionStarted;
	RewardLighting->bCombatStarted = bCombatStarted;
	RewardLighting->SetComponentTickEnabled(IsComponentTickEnabled());
	SetComponentTickEnabled(false);
	CapturedLight.Reset();
}

bool UQuasicomboEncounterLightingComponent::SetApproachProgress(float Progress, float StartIntensity, float EndIntensity)
{
	if (bCombatStarted || !FMath::IsFinite(Progress) || !FMath::IsFinite(StartIntensity) ||
		!FMath::IsFinite(EndIntensity) || !CaptureLight()) return false;
	SetComponentTickEnabled(false);
	CapturedLight->SetIntensity(FMath::Lerp(FMath::Max(0.0f, StartIntensity),
		FMath::Max(0.0f, EndIntensity), FMath::Clamp(Progress, 0.0f, 1.0f)));
	return true;
}

void UQuasicomboEncounterLightingComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Restore();
	Super::EndPlay(Reason);
}
