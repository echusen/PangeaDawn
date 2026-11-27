// Fill out your copyright notice in the Description page of Project Settings.

#include "Objects/Requirements/Req_FacilityUnlocked.h"

#include "Actors/VillageBase.h"
#include "Components/FacilityManagerComponent.h"
#include "GameFramework/Actor.h"

struct FFacilityEntry;

bool UReq_FacilityUnlocked::IsRequirementMetForContext(const FBaseUpgradeContext& Context) const
{
	UPangeaFacilityManagerComponent* FacilityComp = Context.FacilityManager;
	if (FacilityComp == nullptr && Context.Village != nullptr)
	{
		FacilityComp = Context.Village->FindComponentByClass<UPangeaFacilityManagerComponent>();
	}

	if (FacilityComp == nullptr || !RequiredFacilityTag.IsValid())
	{
		return false;
	}

	for (const FFacilityEntry& Entry : FacilityComp->GetAllFacilities())
	{
		if (Entry.FacilityTag == RequiredFacilityTag)
		{
			return Entry.bUnlocked;
		}
	}

	return false;
}

bool UReq_FacilityUnlocked::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	if (ContextObject == nullptr)
	{
		return false;
	}

	AActor* const OwnerActor = Cast<AActor>(ContextObject);
	if (OwnerActor == nullptr)
	{
		return false;
	}

	UPangeaFacilityManagerComponent* const FacilityComp = OwnerActor->FindComponentByClass<UPangeaFacilityManagerComponent>();
	if (FacilityComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Req_FacilityUnlocked: %s has no FacilityManagerComponent"), *OwnerActor->GetName());
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
			{
				return true;
			}

			UE_LOG(LogTemp, Warning, TEXT("Facility requirement FAILED: Facility %s is NOT unlocked"), *RequiredFacilityTag.ToString());
			return false;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Facility requirement FAILED: Facility %s not found in FacilityManager"), *RequiredFacilityTag.ToString());

	return false;
}
