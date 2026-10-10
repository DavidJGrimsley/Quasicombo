#include "QuasicomboEvolutionComponent.h"
#include "QuasicomboEvolutionRules.h"
#include "QuasicomboBoss.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"

namespace
{
float Ease(float X)
{
	X = FMath::Clamp(X, 0.0f, 1.0f);
	return X * X * (3.0f - 2.0f * X);
}
} // namespace
UQuasicomboEvolutionComponent::UQuasicomboEvolutionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	for (const TCHAR* Name : {TEXT("MagicalBarrier"), TEXT("RingOfFlames"), TEXT("TrailsOfDeath")})
		Auras.Add(TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/MegaMagicVFXBundle/VFX/MagicAuraVFX/VFX/%s/Systems/N_%s.N_%s"), Name, Name, Name))));
	auto Animation = [](const TCHAR* Name) {
		return TSoftObjectPtr<UAnimSequence>(
			FSoftObjectPath(FString::Printf(TEXT("/Game/Quasicombo/Boss/Lizard/Animations/AS_Lizard_%s.AS_Lizard_%s"), Name, Name)));
	};
	RiseAnimation = Animation(TEXT("EvolutionRise"));
	HoverAnimation = Animation(TEXT("EvolutionHover"));
	AttachmentAnimation = Animation(TEXT("ArmorAttach"));
	RoarAnimation = Animation(TEXT("EvolutionRoar"));
}
AQuasicomboBoss* UQuasicomboEvolutionComponent::Boss() const
{
	return Cast<AQuasicomboBoss>(GetOwner());
}
void UQuasicomboEvolutionComponent::Sound(USoundBase* Asset)
{
	if (Asset)
		UGameplayStatics::PlaySoundAtLocation(this, Asset, GetOwner()->GetActorLocation());
}
void UQuasicomboEvolutionComponent::Play(TSoftObjectPtr<UAnimSequence> Animation, float Seconds, bool bLoop)
{
	if (auto* B = Boss())
		if (auto* Anim = B->GetMesh()->GetAnimInstance())
			if (auto* Clip = Animation.LoadSynchronous())
			{
				if (auto* Montage = Anim->PlaySlotAnimationAsDynamicMontage(
					Clip, TEXT("DefaultSlot"), 0.12f, 0.12f,
					bLoop ? 1.0f : Clip->GetPlayLength() / FMath::Max(0.01f, Seconds * FMath::Max(0.001f, DurationScale)),
					bLoop ? 1000 : 1))
					if (auto* Instance = Anim->GetActiveInstanceForMontage(Montage))
						Instance->bEnableAutoBlendOut = false;
			}
}
void UQuasicomboEvolutionComponent::Start()
{
	Cancel();
	ClearDebris();
	auto* B = Boss();
	if (!B)
		return;
	GroundTransform = B->GetActorTransform();
	AttachmentAnimation.LoadSynchronous();
	B->GetMesh()->PrestreamTextures(10.0f, true);
	for (auto* Part : B->GetEvolutionArmor())
		Part->PrestreamTextures(10.0f, true);
	Attached.Reset();
	FlightPieces.Reset();
	bResolved = false;
	bLanded = false;
	bReturningCamera = false;
	AttachmentAge = 1.0f;
	if (UClass* HUDClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/UBP_GameHUD.UBP_GameHUD_C")))
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, HUDClass, true);
		for (UUserWidget* Widget : Widgets)
		{
			CombatHUDs.Add(Widget);
			CombatHUDVisibility.Add(Widget->GetVisibility());
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	Camera = GetWorld()->SpawnActor<ACameraActor>();
	if (Camera)
	{
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		SetStage(EQuasicomboEvolutionStage::Stumble);
		UpdateCamera();
	}
	else
		SetStage(EQuasicomboEvolutionStage::Stumble);
}
void UQuasicomboEvolutionComponent::SetStage(EQuasicomboEvolutionStage Next)
{
	Stage = Next;
	Age = 0.0f;
	OnStageChanged.Broadcast(Next);
	switch (Next)
	{
	case EQuasicomboEvolutionStage::Stumble:
		if (auto* B = Boss())
			if (auto* Anim = B->GetMesh()->GetAnimInstance())
				if (auto* Clip = StumbleAnimation.IsNull() ? B->DefeatAnimation.Get() : StumbleAnimation.LoadSynchronous())
					Anim->PlaySlotAnimationAsDynamicMontage(Clip, TEXT("DefaultSlot"), 0.06f, 0.18f,
						1.0f / FMath::Max(0.001f, DurationScale));
		break;
	case EQuasicomboEvolutionStage::IdleRecovery:
		if (auto* B = Boss())
		{
			if (auto* Anim = B->GetMesh()->GetAnimInstance())
			{
				Anim->Montage_Pause(); // Blend away from the stumble endpoint, before the death fall.
				Anim->StopAllMontages(0.18f * DurationScale);
			}
			Play(TSoftObjectPtr<UAnimSequence>(B->IdleAnimation.Get()), IdleRecoverySeconds, true);
		}
		break;
	case EQuasicomboEvolutionStage::Rise:
		Play(RiseAnimation, RiseSeconds);
		if (auto* B = Boss()) B->SetEvolutionPlayerVisible(false);
		UpdateCamera();
		if (Camera)
			if (auto* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetViewTargetWithBlend(Camera, 0.2f);
		Sound(PowerUpSound);
		break;
	case EQuasicomboEvolutionStage::Hover:
		Play(HoverAnimation, HoverSeconds, true);
		break;
	// Keep the first charge loop running through the close-up and every impact.
	case EQuasicomboEvolutionStage::Roar:
		Play(RoarAnimation, RoarSeconds);
		Sound(RoarSound);
		OnRoar.Broadcast();
		break;
	case EQuasicomboEvolutionStage::Return:
		if (auto* B = Boss())
		{
			bReturningCamera = Camera && B->GetEvolutionGameplayView(ReturnEndView);
			if (bReturningCamera) Camera->GetCameraComponent()->GetCameraView(0.0f, ReturnStartView);
			if (auto* Anim = B->GetMesh()->GetAnimInstance())
				Anim->StopAllMontages(ReturnSeconds * DurationScale);
		}
		break;
	default:
		break;
	}
}
void UQuasicomboEvolutionComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Delta, TickType, TickFunction);
	TickDebris(Delta);
	auto* B = Boss();
	if (!B || Stage == EQuasicomboEvolutionStage::Inactive)
		return;
	if (B->GetBossPhase() != EQuasicomboBossPhase::Evolution)
	{
		Cancel();
		return;
	}
	Age += Delta / FMath::Max(0.001f, DurationScale);
	AttachmentAge += Delta / FMath::Max(0.001f, DurationScale);
	if (!bResolved && B->Quantum->HasEvolutionResolved())
	{
		bResolved = true;
		B->ResolveEvolutionPresentation();
		const int32 Level = B->GetEvolutionLevel();
		AssemblySeconds = Level == 2 ? 2.3f : Level == 1 ? 1.6f : 1.0f;
		const auto Parts = B->GetEvolutionArmor();
		for (int32 I = 0; I < Parts.Num(); ++I)
			if (B->ArmorInGroup(I, 0) || (Level == 2 && B->ArmorInGroup(I, 1)))
				if (Level > 0)
					FlightPieces.Add(I);
	}
	// Let the opening stumble and recovery read clearly before the charge VFX.
	if (bResolved && !Aura && Stage != EQuasicomboEvolutionStage::Stumble && Stage != EQuasicomboEvolutionStage::IdleRecovery)
	{
		const int32 Level = B->GetEvolutionLevel();
		if (Auras.IsValidIndex(Level))
			if (auto* System = Auras[Level].LoadSynchronous())
			{
				const FVector Center = B->GetMesh()->Bounds.Origin;
				const FQuat RingRotation = B->GetActorQuat() * FRotator(0, 0, FireRingTiltDegrees).Quaternion();
				Aura =
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Center, Level == 1 ? RingRotation.Rotator() : FRotator::ZeroRotator,
																   FVector(1.8f), true, true, ENCPoolMethod::None, false);
				if (Level == 1)
					UpperAura = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Center,
						(B->GetActorQuat() * FRotator(0, 0, -FireRingTiltDegrees).Quaternion()).Rotator(), FVector(1.8f), true, true, ENCPoolMethod::None, false);
			}
	}
	float Height = LiftHeight;
	switch (Stage)
	{
	case EQuasicomboEvolutionStage::Stumble:
		Height = 0.0f;
		if (Age >= StumbleSeconds) SetStage(EQuasicomboEvolutionStage::IdleRecovery);
		break;
	case EQuasicomboEvolutionStage::IdleRecovery:
		Height = 0.0f;
		if (Age >= IdleRecoverySeconds) SetStage(EQuasicomboEvolutionStage::Rise);
		break;
	case EQuasicomboEvolutionStage::Rise:
		Height = LiftHeight * Ease(Age / FMath::Max(0.01f, RiseSeconds));
		if (Age >= RiseSeconds)
			SetStage(EQuasicomboEvolutionStage::Hover);
		break;
	case EQuasicomboEvolutionStage::Hover:
		Height += 2.0f * FMath::Sin(Age * 5.0f);
		if (Age >= HoverSeconds && bResolved)
			SetStage(EQuasicomboEvolutionStage::FaceReveal);
		break;
	case EQuasicomboEvolutionStage::FaceReveal:
	{
		const auto Charge = QuasicomboEvolutionRules::EyeCharge(Age, FaceRevealSeconds);
		if (Charge.bPermanent)
			B->RevealEvolutionEyes(Charge.Alpha);
		else
			B->PreviewEvolutionEyes(Charge.bNewEyes, Charge.Alpha);
		// Charge clamps at full brightness and the closest framing during this hold.
		if (Age >= FaceRevealSeconds + FMath::Max(0.0f, FaceHoldSeconds))
			SetStage(EQuasicomboEvolutionStage::ArmorAssembly);
		break;
	}
	case EQuasicomboEvolutionStage::ArmorAssembly: {
		const auto Parts = B->GetEvolutionArmor();
		const float Stagger = FlightPieces.Num() > 1 ? FMath::Max(0.0f, (AssemblySeconds - 1.0f) / (FlightPieces.Num() - 1)) : 0.0f;
		for (int32 N = 0; N < FlightPieces.Num(); ++N)
		{
			const int32 I = FlightPieces[N];
			auto* Part = Parts[I];
			if (Attached.Contains(I))
				continue;
			const float T = (Age - 0.4f - N * Stagger) / 0.5f;
			if (T < 0.0f)
				continue;
			Part->SetVisibility(true);
			const float A = Ease(T);
			const float Sign = (N % 2) == 0 ? 1.0f : -1.0f;
			// The mesh remains leader-posed; only its flight offset changes.
			Part->SetRelativeLocation(
				FVector(Sign * 430.0f * (1.0f - A), -260.0f * (1.0f - A), 120.0f * (1.0f - A) + 75.0f * FMath::Sin(A * PI)));
			Part->SetRelativeRotation(FRotator(0.0f, Sign * 45.0f * (1.0f - A), Sign * 30.0f * (1.0f - A)));
			if (T >= 1.0f)
			{
				Part->SetRelativeTransform(FTransform::Identity);
				Attached.Add(I);
				// Preserve the impact reaction over the first charge pose.
				AttachmentAge = 0.0f;
				Sound(AttachmentSound);
				OnArmorAttached.Broadcast(I);
			}
		}
		Height += 2.5f * FMath::Sin(FMath::Clamp(AttachmentAge / 0.28f, 0.0f, 1.0f) * PI);
		if (Age >= AssemblySeconds && Attached.Num() == FlightPieces.Num() &&
			(FlightPieces.IsEmpty() || AttachmentAge >= 0.28f))
			SetStage(EQuasicomboEvolutionStage::Roar);
		break;
	}
	case EQuasicomboEvolutionStage::Roar:
		// Open the chest and roar during a controlled descent; finish grounded.
		Height = LiftHeight * (1.0f - Ease(Age / FMath::Max(0.01f, RoarSeconds * 0.85f)));
		if (Age >= RoarSeconds * 0.85f && !bLanded)
		{
			bLanded = true;
			Sound(LandingSound);
			OnLanded.Broadcast();
		}
		if (Age >= RoarSeconds * 0.65f && Aura)
			Aura->Deactivate();
		if (Age >= RoarSeconds * 0.65f && UpperAura)
			UpperAura->Deactivate();
		if (Age >= RoarSeconds)
			SetStage(EQuasicomboEvolutionStage::Return);
		break;
	case EQuasicomboEvolutionStage::Return:
		Height = 0.0f;
		if (Age >= ReturnSeconds)
		{
			UpdateCamera();
			B->CompleteEvolutionPresentation();
			Cancel();
			return;
		}
		break;
	default:
		return;
	}
	B->SetActorLocation(GroundTransform.GetLocation() + FVector(0, 0, Height), false, nullptr, ETeleportType::TeleportPhysics);
	if (Aura)
	{
		const FVector Center = B->GetMesh()->Bounds.Origin;
		Aura->SetWorldLocation(Center);
		Aura->SetWorldScale3D(FVector(1.8f));
		if (UpperAura)
		{
			// Cross the two rings at the body's midpoint, including during rise and descent.
			Aura->SetWorldRotation(B->GetActorQuat() * FRotator(0, 0, FireRingTiltDegrees).Quaternion());
			UpperAura->SetWorldLocationAndRotation(Center, B->GetActorQuat() * FRotator(0, 0, -FireRingTiltDegrees).Quaternion());
			UpperAura->SetWorldScale3D(FVector(1.8f));
		}
	}
	UpdateCamera();
}
void UQuasicomboEvolutionComponent::UpdateCamera()
{
	auto* B = Boss();
	if (!B || !Camera)
		return;
	if (Stage == EQuasicomboEvolutionStage::Return && bReturningCamera)
	{
		// One straight 3D path with simultaneous rotation and lens interpolation.
		// Snapshot the endpoint so gameplay camera tracking cannot bend the path.
		const float T = Ease(Age / FMath::Max(0.01f, ReturnSeconds));
		Camera->SetActorLocation(FMath::Lerp(ReturnStartView.Location, ReturnEndView.Location, T));
		Camera->SetActorRotation(FQuat::Slerp(ReturnStartView.Rotation.Quaternion(), ReturnEndView.Rotation.Quaternion(), T));
		Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(ReturnStartView.FOV, ReturnEndView.FOV, T));
		return;
	}
	const bool bEyes = Stage == EQuasicomboEvolutionStage::FaceReveal;
	const float Pull = Stage == EQuasicomboEvolutionStage::ArmorAssembly ? Ease(Age / 0.4f) : 1.0f;
	const float FaceArrivalSeconds = QuasicomboEvolutionRules::FaceArrivalSeconds(FaceRevealSeconds);
	// Share the accelerating camera curve with eye brightness; flickers stay short.
	const float FacePush = bEyes ? QuasicomboEvolutionRules::EyeCharge(Age, FaceRevealSeconds).Alpha : 1.0f;
	const FName Anchor = TEXT("EvolutionEyes");
	const FVector Head = B->GetMesh()->GetSocketLocation(B->GetMesh()->DoesSocketExist(Anchor) ? Anchor : FName(TEXT("head")));
	// Leave room below the feet for the persistent boss HUD.
	const FVector Body = B->GetMesh()->Bounds.Origin - FVector(0, 0, B->GetMesh()->Bounds.BoxExtent.Z * 0.08f);
	const float FaceWeight = bEyes ? Ease(Age / FaceArrivalSeconds) : Stage == EQuasicomboEvolutionStage::ArmorAssembly ? 1.0f - Pull : 0.0f;
	const FVector Target = FMath::Lerp(Body, Head, FaceWeight);
	int32 W = 1920, H = 1080;
	if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
		PC->GetViewportSize(W, H);
	const float Aspect = H > 0 && W > 0 ? float(W) / H : 1.777f;
	Camera->GetCameraComponent()->SetAspectRatio(Aspect);
	// Armor pullback starts at the same tight framing where the face reveal finishes.
	const float CloseDistance = FMath::Lerp(135.0f, 100.0f, FacePush);
	const float Distance = FMath::Lerp(FMath::Max(520.0f, B->GetMesh()->Bounds.BoxExtent.Z * Aspect / 0.466f * 1.35f), CloseDistance, FaceWeight);
	const FVector Front = B->GetActorForwardVector();
	FVector Eye = Target;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EvolutionCamera), false, B);
	float BestDistance = 0.0f;
	// Small frontal offsets avoid nearby scenery without changing to a side view.
	for (float Yaw : {0.0f, -20.0f, 20.0f})
	{
		for (float Up : {FMath::Lerp(25.0f, 0.0f, FaceWeight), 80.0f})
		{
			const FVector Direction = Front.RotateAngleAxis(Yaw, FVector::UpVector);
			FVector Candidate = Target + Direction * Distance + FVector(0, 0, Up);
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, Target, Candidate, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
				Candidate = Hit.Location - Direction * 20.0f;
			const float Available = FVector::Distance(Target, Candidate);
			if (Available > BestDistance)
			{
				Eye = Candidate;
				BestDistance = Available;
			}
			if (Available >= Distance)
				break;
		}
		if (BestDistance >= Distance)
			break;
	}
	Camera->SetActorLocation(Eye);
	Camera->SetActorRotation((Target - Eye).Rotation());
	// UE uses horizontal FOV. Widen it when collision shortens the camera arm,
	// including portrait viewports, so the whole body remains in the frame.
	const FVector Extent = B->GetMesh()->Bounds.BoxExtent;
	const float HalfWidth = FMath::Max(Extent.Y, Extent.Z * Aspect) * 1.35f;
	const float BodyFOV = FMath::Max(50.0f, FMath::RadiansToDegrees(2.0f * FMath::Atan2(HalfWidth, FMath::Max(80.0f, BestDistance))));
	Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp(FMath::Min(120.0f, BodyFOV), 28.0f, FaceWeight));
}
void UQuasicomboEvolutionComponent::BreakArmorGroup(int32 Group, const FVector& Impulse)
{
	auto* B = Boss();
	if (!B)
		return;
	const auto Parts = B->GetEvolutionArmor();
	for (int32 I = 0; I < Parts.Num(); ++I)
		if (B->ArmorInGroup(I, Group) && Parts[I]->IsVisible())
		{
			auto* Copy = NewObject<UPoseableMeshComponent>(B);
			Copy->SetSkinnedAssetAndUpdate(Parts[I]->GetSkeletalMeshAsset());
			Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Copy->RegisterComponent();
			Copy->SetWorldTransform(Parts[I]->GetComponentTransform());
			Copy->CopyPoseFromSkeletalComponent(B->GetMesh());
			for (int32 M = 0; M < Parts[I]->GetNumMaterials(); ++M)
				Copy->SetMaterial(M, Parts[I]->GetMaterial(M));
			FQuasicomboArmorDebris D;
			D.Mesh = Copy;
			TArray<FFinalSkinVertex> Vertices;
			// Dedicated servers and NullRHI automation have no render mesh object.
			if (Parts[I]->GetMeshObject())
				Parts[I]->GetCPUSkinnedVertices(Vertices, 0);
			FBox Box(ForceInit);
			for (const auto& Vertex : Vertices)
				Box += FVector(Vertex.Position);
			if (Box.IsValid)
			{
				D.LocalCenter = Box.GetCenter();
				D.LocalExtent = Box.GetExtent();
			}
			D.Velocity = FVector((I % 2 ? 1.0f : -1.0f) * 90.0f, 0, 150.0f) + Impulse.GetClampedToMaxSize(150.0f);
			D.AngularVelocity = FVector(35.0f + I * 4.0f, 50.0f - I * 3.0f, 60.0f);
			Debris.Add(D);
			Parts[I]->SetVisibility(false);
		}
}
void UQuasicomboEvolutionComponent::TickDebris(float Delta)
{
	for (auto& D : Debris)
		if (D.Mesh && !D.bSettled)
		{
			D.Age += Delta;
			D.Velocity.Z -= 980.0f * Delta;
			const FTransform Transform = D.Mesh->GetComponentTransform();
			const FVector Old = Transform.TransformPosition(D.LocalCenter);
			FVector Next = Old + D.Velocity * Delta;
			const FQuat Rotation =
				FRotator(D.AngularVelocity.X * Delta, D.AngularVelocity.Y * Delta, D.AngularVelocity.Z * Delta).Quaternion() *
				Transform.GetRotation();
			const float Bottom = FBox(-D.LocalExtent, D.LocalExtent)
									 .TransformBy(FTransform(Rotation, FVector::ZeroVector, Transform.GetScale3D()))
									 .GetExtent()
									 .Z;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ArmorDebris), false, GetOwner());
			if (GetWorld()->LineTraceSingleByObjectType(Hit, Old - FVector(0, 0, Bottom - 4.0f), Next - FVector(0, 0, Bottom),
														FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				Next.Z = Hit.ImpactPoint.Z + Bottom + 2.0f;
				D.Velocity.Z = FMath::Abs(D.Velocity.Z) * 0.22f;
				D.Velocity.X *= 0.45f;
				if (++D.Bounces >= 2)
					D.bSettled = true;
			}
			const FVector Origin = Next - Rotation.RotateVector(Transform.GetScale3D() * D.LocalCenter);
			D.Mesh->SetWorldLocationAndRotation(Origin, Rotation);
			if (D.Age > 5.0f)
				D.bSettled = true;
		}
}
void UQuasicomboEvolutionComponent::Cancel()
{
	AttachmentAge = 1.0f;
	bReturningCamera = false;
	if (auto* B = Boss())
	{
		B->GetMesh()->SetMorphTarget(TEXT("EvolutionSmirk"), 0.0f);
		B->GetMesh()->SetMorphTarget(TEXT("EvolutionEyeNarrow"), 0.0f);
	}
	for (int32 I = 0; I < CombatHUDs.Num(); ++I)
		if (UUserWidget* Widget = CombatHUDs[I].Get())
			Widget->SetVisibility(CombatHUDVisibility[I]);
	CombatHUDs.Reset();
	CombatHUDVisibility.Reset();
	if (Stage != EQuasicomboEvolutionStage::Inactive)
		if (auto* B = Boss())
		{
			B->SetActorTransform(GroundTransform, false, nullptr, ETeleportType::TeleportPhysics);
			for (auto* Part : B->GetEvolutionArmor())
				Part->SetRelativeTransform(FTransform::Identity);
			if (B->GetBossPhase() == EQuasicomboBossPhase::Evolution)
				if (auto* Anim = B->GetMesh()->GetAnimInstance())
					Anim->StopAllMontages(0.12f);
			B->AbortEvolutionPresentation();
			B->RestoreCameraAndTime();
		}
	Stage = EQuasicomboEvolutionStage::Inactive;
	if (Aura)
	{
		Aura->DeactivateImmediate();
		Aura->DestroyComponent();
		Aura = nullptr;
	}
	if (UpperAura)
	{
		UpperAura->DeactivateImmediate();
		UpperAura->DestroyComponent();
		UpperAura = nullptr;
	}
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
}
void UQuasicomboEvolutionComponent::ClearDebris()
{
	for (auto& D : Debris)
		if (D.Mesh)
			D.Mesh->DestroyComponent();
	Debris.Reset();
}
void UQuasicomboEvolutionComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Cancel();
	ClearDebris();
	Super::EndPlay(Reason);
}
