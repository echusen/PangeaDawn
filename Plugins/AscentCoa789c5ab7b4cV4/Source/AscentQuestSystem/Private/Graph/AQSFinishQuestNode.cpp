// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 


#include "Graph/AQSFinishQuestNode.h"
#include "Graph/AQSQuest.h"
#include "AQSQuestFunctionLibrary.h"
#include "AQSQuestManagerComponent.h"

UAQSFinishQuestNode::UAQSFinishQuestNode()
{
}

void UAQSFinishQuestNode::ActivateNode()
{
	Super::ActivateNode();

	UAQSQuest* quest = Cast< UAQSQuest>(GetGraph());
	quest->CompleteQuest(bIsSuccessful);

	if (NextQuestTag != FGameplayTag()) {
		UAQSQuestManagerComponent* questManager = UAQSQuestFunctionLibrary::GetQuestManager(this);
		if (questManager) {
			questManager->StartQuest(NextQuestTag);
		}
	}
}

