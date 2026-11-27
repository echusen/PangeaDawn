// Fill out your copyright notice in the Description page of Project Settings.

#include "Objects/Requirements/Req_QuestCompleted.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#include "AQSQuestManagerComponent.h"

bool UReq_QuestCompleted::IsRequirementMetForContext(const FBaseUpgradeContext& Context) const
{
	return IsRequirementMet(Context.InteractingPawn != nullptr ? static_cast<UObject*>(Context.InteractingPawn) : Context.GetPreferredRequirementObject());
}

bool UReq_QuestCompleted::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	APawn* const Pawn = Cast<APawn>(ContextObject);
	if (Pawn == nullptr)
	{
		return false;
	}

	// ACF Ultimate keeps personal quests on the owning controller, not the pawn.
	// Never resolve player index 0 here: requirements must use the interacting player.
	UAQSQuestManagerComponent* QuestManager =
		Pawn->GetController() != nullptr ? Pawn->GetController()->FindComponentByClass<UAQSQuestManagerComponent>() : nullptr;
	if (QuestManager == nullptr)
	{
		QuestManager = Pawn->FindComponentByClass<UAQSQuestManagerComponent>();
	}

	if (QuestManager == nullptr)
	{
		return false;
	}

	for (const FGameplayTag& QuestTag : RequiredQuestTags)
	{
		if (!QuestManager->IsQuestCompletedByTag(QuestTag))
		{
			return false;
		}
	}

	return true;
}

FText UReq_QuestCompleted::GetFailureMessage() const
{
	return FText::FromString(TEXT("Required quest(s) not completed."));
}
