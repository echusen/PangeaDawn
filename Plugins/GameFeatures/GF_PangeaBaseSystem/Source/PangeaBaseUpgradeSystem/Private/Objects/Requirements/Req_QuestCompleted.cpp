// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/Requirements/Req_QuestCompleted.h"

#include "AQSQuestManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

bool UReq_QuestCompleted::IsRequirementMetForContext(const FBaseUpgradeContext& Context) const
{
	return IsRequirementMet(Context.InteractingPawn ? static_cast<UObject*>(Context.InteractingPawn) : Context.GetPreferredRequirementObject());
}

bool UReq_QuestCompleted::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	APawn* Pawn = Cast<APawn>(ContextObject);
	if (!Pawn)
		return false;

	// ACF Ultimate keeps personal quests on the owning controller, not the pawn.
	// Never resolve player index 0 here: requirements must use the interacting player.
	UAQSQuestManagerComponent* QuestManager = Pawn->GetController()
		? Pawn->GetController()->FindComponentByClass<UAQSQuestManagerComponent>() : nullptr;
	if (!QuestManager)
		QuestManager = Pawn->FindComponentByClass<UAQSQuestManagerComponent>();
	if (!QuestManager)
		return false;

	for (const FGameplayTag& QuestTag : RequiredQuestTags)
	{
		if (!QuestManager->IsQuestCompletedByTag(QuestTag))
			return false;
	}

	return true;
}

FText UReq_QuestCompleted::GetFailureMessage() const
{
	return FText::FromString(TEXT("Required quest(s) not completed."));
}
