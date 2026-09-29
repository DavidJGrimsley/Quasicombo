#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QuasicomboCreditsData.generated.h"

/** Cooked, editable credit lines used by the victory roll. */
UCLASS(BlueprintType)
class UQuasicomboCreditsData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Credits") TArray<FText> Lines;
};
