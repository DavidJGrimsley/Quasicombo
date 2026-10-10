#pragma once

#include "CoreMinimal.h"
#include "QuasicomboQTETypes.generated.h"

UENUM(BlueprintType)
enum class EQuasicomboQTEInput : uint8 { Attack, Dash, Jump, Invalid, Fire, Chomp, Claw };

UENUM(BlueprintType)
enum class EQuasicomboQTEInputEdge : uint8 { Press, Release, Cancel };
