// Fill out your copyright notice in the Description page of Project Settings.


#include "Actions/UA_EnableFacility.h"

#include "Components/FacilityManagerComponent.h"

void UUA_EnableFacility::Execute_Implementation(UObject* ContextObject)
{
	AActor* OwnerActor = Cast<AActor>(ContextObject);
	if (!OwnerActor)
		return;

	UFacilityManagerComponent* Manager = OwnerActor->FindComponentByClass<UFacilityManagerComponent>();
	if (!Manager)
		return;

	switch (Mode)
	{
	case EFacilityEnableMode::EnableOnly:
		Manager->EnableFacility(FacilityTag);
		break;

	case EFacilityEnableMode::DisableOnly:
		Manager->DisableFacility(FacilityTag);
		break;

	case EFacilityEnableMode::Toggle:
		if (Manager->IsFacilityEnabled(FacilityTag))
			Manager->DisableFacility(FacilityTag);
		else
			Manager->EnableFacility(FacilityTag);
		break;
	}
}
