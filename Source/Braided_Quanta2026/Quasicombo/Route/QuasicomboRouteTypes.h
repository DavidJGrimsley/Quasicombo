#pragma once

#include "CoreMinimal.h"
#include "QuasicomboRouteTypes.generated.h"

UENUM(BlueprintType)
enum class EQuasicomboLane : uint8
{
	A,
	B,
	C
};

UENUM(BlueprintType)
enum class EQuasicomboRunOutcome : uint8
{
	Playing,
	Victory,
	Defeat
};

USTRUCT(BlueprintType)
struct FQuasicomboBraidOperation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Braid")
	int32 Generator = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Braid")
	int32 Power = 1;
};
