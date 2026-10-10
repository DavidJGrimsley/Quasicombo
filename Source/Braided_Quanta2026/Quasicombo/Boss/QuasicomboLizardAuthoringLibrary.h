#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QuasicomboLizardAuthoringLibrary.generated.h"
class USkeletalMesh;
class UAnimSequence;

/** Editor authoring bridge. The vendor mesh is read-only; facial deltas go on a duplicate. */
UCLASS()
class UQuasicomboLizardAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Lizard Authoring")
	static void FinishAnimationCompression(UAnimSequence* Animation);
	UFUNCTION(BlueprintCallable, Category="Lizard Authoring")
	static bool ExportReferencePose(USkeletalMesh* Mesh, const FString& Filename);
	UFUNCTION(BlueprintCallable, Category="Lizard Authoring")
	static bool AddFaceShapes(USkeletalMesh* Original, USkeletalMesh* Duplicate, const FString& Filename);
};
