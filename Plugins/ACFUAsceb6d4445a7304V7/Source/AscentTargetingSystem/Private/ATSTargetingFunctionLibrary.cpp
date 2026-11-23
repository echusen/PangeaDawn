// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ATSTargetingFunctionLibrary.h"
#include "ATSBaseTargetComponent.h"
#include "ATSTargetPointComponent.h"
#include "GameFramework/Pawn.h" 

AActor* UATSTargetingFunctionLibrary::GetTargetedActor(const APawn* pawn)
{
	if (const UATSBaseTargetComponent* target = UATSBaseTargetComponent::FindForActor(pawn)) {
		return target->GetCurrentTarget();
	}
	return nullptr;
}

UATSTargetPointComponent* UATSTargetingFunctionLibrary::GetCurrentTargetPoint(const APawn* pawn)
{
	if (UATSBaseTargetComponent* target = UATSBaseTargetComponent::FindForActor(pawn)) {
		if (target->GetCurrentTargetPoint()) {
			return target->GetCurrentTargetPoint();
		} else if (AActor* targetActor = target->GetCurrentTarget()) {
			return targetActor->FindComponentByClass<UATSTargetPointComponent>();
		}
	}
	return nullptr;
}

bool UATSTargetingFunctionLibrary::GetLookAtLocation(const APawn* pawn, FVector& OutLocation)
{
	if (const UATSBaseTargetComponent* target = UATSBaseTargetComponent::FindForActor(pawn)) {
		return target->GetLookAtLocation(OutLocation);
	}
	return false;
}
