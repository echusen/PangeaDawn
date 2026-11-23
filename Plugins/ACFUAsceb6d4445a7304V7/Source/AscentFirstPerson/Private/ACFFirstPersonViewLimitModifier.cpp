// Copyright (C) Dark Tower Interactive SRL. All Rights Reserved.
#include "ACFFirstPersonViewLimitModifier.h"
#include "Components/ACFFirstPersonComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

UACFFirstPersonViewLimitModifier::UACFFirstPersonViewLimitModifier()
{
	Priority = 255;
	AlphaInTime = 0.f;
	AlphaOutTime = 0.f;
}

bool UACFFirstPersonViewLimitModifier::ProcessViewRotation(AActor* ViewTarget, float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
	const APlayerController* Controller = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const UACFFirstPersonComponent* Component = Pawn && ViewTarget == Pawn ? Pawn->FindComponentByClass<UACFFirstPersonComponent>() : nullptr;
	if (!Component || !Component->IsInFirstPerson())
	{
		return false;
	}
	// Consume input before the controller commits control rotation / calls FaceRotation.
	// Excess input is discarded, not stored behind a visually clamped camera.
	OutViewRotation = Component->ClampFirstPersonViewRotation(OutViewRotation + OutDeltaRot);
	OutDeltaRot = FRotator::ZeroRotator;
	return true;
}
