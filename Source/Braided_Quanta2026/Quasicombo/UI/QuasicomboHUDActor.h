#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "QuasicomboQTEComponent.h"
#include "QuasicomboHUDActor.generated.h"

class UTextBlock;
class UBorder;
class USizeBox;
class UInputAction;
class UProgressBar;
class UQuasicomboVictoryWidget;
class UQuasicomboCreditsData;
struct FKey;

UCLASS()
class UQuasicomboHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

  private:
	UPROPERTY()
	TObjectPtr<UTextBlock> RunText;
	UPROPERTY()
	TObjectPtr<UTextBlock> RoundsRemainingText;
	UPROPERTY()
	TObjectPtr<UTextBlock> BossText;
	UPROPERTY()
	TObjectPtr<UTextBlock> EvolutionBuffText;
	UPROPERTY()
	TObjectPtr<UBorder> BossInfoPanel;
	UPROPERTY()
	TObjectPtr<USizeBox> BossInfoWidth;
	UPROPERTY()
	TObjectPtr<UTextBlock> RetryText;
	UPROPERTY()
	TObjectPtr<UProgressBar> BossHealth;
	UPROPERTY()
	TArray<TObjectPtr<UProgressBar>> BossArmor;
	UPROPERTY()
	TObjectPtr<UTextBlock> PromptText;
	UPROPERTY() TObjectPtr<UBorder> PromptIcon;
	UPROPERTY() TObjectPtr<UTextBlock> PromptIconText;
	UPROPERTY() TObjectPtr<UInputAction> AttackAction;
	UPROPERTY() TObjectPtr<UInputAction> DashAction;
	UPROPERTY() TObjectPtr<UInputAction> JumpAction;
	FString DescribePromptAction(EQuasicomboQTEInput Input, FKey* OutGamepadKey = nullptr) const;
};

UCLASS()
class AQuasicomboHUDActor : public AActor
{
	GENERATED_BODY()
public:
	AQuasicomboHUDActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UFUNCTION() void HandleRunStarted();
	UFUNCTION() void HandleVictoryReady();
	UFUNCTION() void HandleRunEnded(EQuasicomboRunOutcome Outcome);
	UPROPERTY() TObjectPtr<UQuasicomboHUDWidget> Widget;
	UPROPERTY() TObjectPtr<UQuasicomboVictoryWidget> VictoryWidget;
	UPROPERTY(EditDefaultsOnly, Category="Quasicombo|UI") TObjectPtr<UQuasicomboCreditsData> CreditsData;
};
