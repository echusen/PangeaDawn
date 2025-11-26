// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/Requirements/Req_QuestCompleted.h"

#include "AQSQuestManagerComponent.h"

bool UReq_QuestCompleted::IsRequirementMet_Implementation(UObject* ContextObject) const
{
	APawn* Pawn = Cast<APawn>(ContextObject);
	if (!Pawn)
		return false;

	UAQSQuestManagerComponent* QuestManager = Pawn->FindComponentByClass<UAQSQuestManagerComponent>();
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
