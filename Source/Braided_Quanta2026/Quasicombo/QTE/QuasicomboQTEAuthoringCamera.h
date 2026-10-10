#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "QuasicomboQTEAuthoringCamera.generated.h"

/** Sequence camera: location is authored in the arena; focus follows the two subjects. */
UCLASS()
class AQuasicomboQTEAuthoringCamera : public ACameraActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Interp,Category="QTE Camera",meta=(ClampMin="0",ClampMax="1")) float TargetBlend=.5f;
    UPROPERTY(EditAnywhere,Interp,Category="QTE Camera") FVector FocusOffset=FVector(0,0,40);
};
