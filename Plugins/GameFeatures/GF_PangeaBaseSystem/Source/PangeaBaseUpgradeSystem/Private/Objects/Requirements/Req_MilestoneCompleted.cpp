// Fill out your copyright notice in the Description page of Project Settings.

#include "Objects/Requirements/Req_MilestoneCompleted.h"

#include "Actors/VillageBase.h"
#include "Components/UpgradeSystemComponent.h"
#include "GameFramework/Actor.h"

bool UReq_MilestoneCompleted::IsRequirementMetForContext(const FBaseUpgradeContext& Context) const
{
	if (!RequiredMilestoneTag.IsValid() || Context.UpgradeSystem == nullptr)
	{
		return false;
	}

	return Context.UpgradeSystem->IsMilestoneCompleted(RequiredMilestoneTag);
}

bool UReq_MilestoneCompleted::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	if (ContextObject == nullptr)
	{
		return false;
	}

	AActor* const OwnerActor = Cast<AActor>(ContextObject);
	if (OwnerActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Req_MilestoneCompleted: ContextObject is not an Actor"));
		return false;
	}

	UPangeaUpgradeSystemComponent* const UpgradeComp = OwnerActor->FindComponentByClass<UPangeaUpgradeSystemComponent>();
	if (UpgradeComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Req_MilestoneCompleted: OwnerActor '%s' has no UpgradeSystemComponent"), *OwnerActor->GetName());
		return false;
	}

	if (!RequiredMilestoneTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Req_MilestoneCompleted: RequiredMilestoneTag is INVALID"));
		return false;
	}

	const bool bCompleted = UpgradeComp->IsMilestoneCompleted(RequiredMilestoneTag);

	if (!bCompleted)
	{
		UE_LOG(LogTemp, Warning, TEXT("Milestone requirement FAILED: %s is not completed on %s"), *RequiredMilestoneTag.ToString(), *OwnerActor->GetName());
	}

	return bCompleted;
}
