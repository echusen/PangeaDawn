// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/Requirements/Req_FacilityUnlocked.h"

#include "Components/FacilityManagerComponent.h"

struct FFacilityEntry;

bool UReq_FacilityUnlocked::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	if (!ContextObject)
		return false;

	AActor* OwnerActor = Cast<AActor>(ContextObject);
	if (!OwnerActor)
		return false;

	UFacilityManagerComponent* FacilityComp = OwnerActor->FindComponentByClass<UFacilityManagerComponent>();
	if (!FacilityComp)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("Req_FacilityUnlocked: %s has no FacilityManagerComponent"),
			   *OwnerActor->GetName());
		return false;
	}

	if (!RequiredFacilityTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Req_FacilityUnlocked: RequiredFacilityTag is INVALID"));
		return false;
	}

	// Search the facilities
	for (const FFacilityEntry& Entry : FacilityComp->GetAllFacilities())
	{
		if (Entry.FacilityTag == RequiredFacilityTag)
		{
			if (Entry.bUnlocked)
				return true;

			UE_LOG(LogTemp, Warning,
				   TEXT("Facility requirement FAILED: Facility %s is NOT unlocked"),
				   *RequiredFacilityTag.ToString());
			return false;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("Facility requirement FAILED: Facility %s not found in FacilityManager"),
		*RequiredFacilityTag.ToString());

	return false;
}
