// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_EndDash.h"
#include "PlatformingCharacter.h"
#include "SideScrollingCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_EndDash::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp)
	{
		return;
	}

	// The original notify only recognized APlatformingCharacter. The same dash
	// montage is now intentionally reused by ASideScrollingCharacter.
	if (APlatformingCharacter* PlatformingCharacter = Cast<APlatformingCharacter>(MeshComp->GetOwner()))
	{
		PlatformingCharacter->EndDash();
	}
	else if (ASideScrollingCharacter* SideScrollingCharacter = Cast<ASideScrollingCharacter>(MeshComp->GetOwner()))
	{
		SideScrollingCharacter->EndDash();
	}
}

FString UAnimNotify_EndDash::GetNotifyName_Implementation() const
{
	return FString("End Dash");
}
