#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuasicomboVictoryWidget.generated.h"

class UBorder;
class UButton;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UQuasicomboCreditsData;
class UQuasicomboRunSubsystem;

/** One focused victory flow: results, technical explanation, then credits. */
UCLASS()
class UQuasicomboVictoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetCreditsData(UQuasicomboCreditsData* InCreditsData);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	enum class EPage : uint8 { Menu, Technical, Credits };
	UPROPERTY() TObjectPtr<UQuasicomboCreditsData> CreditsData;
	UPROPERTY() TObjectPtr<UBorder> MenuPage;
	UPROPERTY() TObjectPtr<UBorder> TechnicalPage;
	UPROPERTY() TObjectPtr<UBorder> CreditsPage;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreText;
	UPROPERTY() TObjectPtr<UTextBlock> BreakdownText;
	UPROPERTY() TObjectPtr<UTextBlock> TechnicalText;
	UPROPERTY() TObjectPtr<UScrollBox> TechnicalScroll;
	UPROPERTY() TObjectPtr<UScrollBox> CreditsScroll;
	UPROPERTY() TObjectPtr<UVerticalBox> CreditsContent;
	UPROPERTY() TObjectPtr<UButton> InfoButton;
	UPROPERTY() TObjectPtr<UButton> ReplayButton;
	UPROPERTY() TObjectPtr<UButton> QuitButton;
	UPROPERTY() TObjectPtr<UButton> ContinueButton;
	UPROPERTY() TObjectPtr<UButton> TechnicalBackButton;
	UPROPERTY() TObjectPtr<UButton> CreditsBackButton;
	EPage Page = EPage::Menu;
	int32 MenuSelection = 0;
	float CreditsEndWait = 0.0f;
	bool bPendingFocus = false;
	void ShowPage(EPage NewPage);
	void FillResults(const UQuasicomboRunSubsystem* Run);
	void FillCredits();
	UFUNCTION() void ShowTechnical();
	UFUNCTION() void ReplayNoOp();
	UFUNCTION() void QuitGame();
	UFUNCTION() void ShowCredits();
	UFUNCTION() void BackToMenu();
};
