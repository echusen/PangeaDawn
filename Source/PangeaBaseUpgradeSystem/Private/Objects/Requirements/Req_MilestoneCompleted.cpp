// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/Requirements/Req_MilestoneCompleted.h"

#include "Components/UpgradeSystemComponent.h"

bool UReq_MilestoneCompleted::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	if (!ContextObject)
	{
		return false;
	}

	AActor* OwnerActor = Cast<AActor>(ContextObject);
	if (!OwnerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Req_MilestoneCompleted: ContextObject is not an Actor"));
		return false;
	}

	UUpgradeSystemComponent* UpgradeComp = OwnerActor->FindComponentByClass<UUpgradeSystemComponent>();
	if (!UpgradeComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Req_MilestoneCompleted: OwnerActor '%s' has no UpgradeSystemComponent"),
			   *OwnerActor->GetName());
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
		UE_LOG(LogTemp, Warning,
			TEXT("Milestone requirement FAILED: %s is not completed on %s"),
			*RequiredMilestoneTag.ToString(),
			*OwnerActor->GetName());
	}

	return bCompleted;
}
