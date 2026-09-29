// Copyright Epic Games, Inc. All Rights Reserved.

#include "SideScrollingPlayerController.h"
#include "QuasicomboRunSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "SideScrollingCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Braided_Quanta2026.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UObject/UObjectGlobals.h"

void ASideScrollingPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void ASideScrollingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					if (CurrentContext)
					{
						Subsystem->AddMappingContext(CurrentContext, 0);
					}
				}
			}

			// The SideScroller IMC does not know about the actions that live in
			// the Platforming and Combat content packs. Create a small runtime
			// context containing only those actions instead of adding the full
			// Platforming/Combat IMCs (which would duplicate movement mappings).
			if (!GameplayExtensionsMappingContext)
			{
				GameplayExtensionsMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SideScrollerGameplayExtensions"));

				UInputAction* DashAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Variant_Platforming/Input/Actions/IA_Dash.IA_Dash"));
				UInputAction* ComboAttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Variant_Combat/Input/Actions/IA_ComboAttack.IA_ComboAttack"));
				UInputAction* ChargedAttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Variant_Combat/Input/Actions/IA_ChargedAttack.IA_ChargedAttack"));

				if (DashAction)
				{
					GameplayExtensionsMappingContext->MapKey(DashAction, EKeys::LeftShift);
					GameplayExtensionsMappingContext->MapKey(DashAction, EKeys::Gamepad_RightShoulder);
				}

				if (ComboAttackAction)
				{
					GameplayExtensionsMappingContext->MapKey(ComboAttackAction, EKeys::LeftMouseButton);
					GameplayExtensionsMappingContext->MapKey(ComboAttackAction, EKeys::Gamepad_FaceButton_Right);
				}

				if (ChargedAttackAction)
				{
					GameplayExtensionsMappingContext->MapKey(ChargedAttackAction, EKeys::RightMouseButton);
					GameplayExtensionsMappingContext->MapKey(ChargedAttackAction, EKeys::Gamepad_RightTriggerAxis);
				}
			}

			if (GameplayExtensionsMappingContext)
			{
				Subsystem->AddMappingContext(GameplayExtensionsMappingContext, 1);
			}
		}
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogBraided_Quanta2026, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void ASideScrollingPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		InPawn->OnDestroyed.AddDynamic(this, &ASideScrollingPlayerController::OnPawnDestroyed);
	}
}

void ASideScrollingPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	if (UQuasicomboRunSubsystem* Run = GetWorld()->GetSubsystem<UQuasicomboRunSubsystem>())
	{
		Run->EndRun(EQuasicomboRunOutcome::Defeat);
	}
}

bool ASideScrollingPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
