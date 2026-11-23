// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 


#include "AQSTypes.h"
#include "Graph/AQSQuest.h"

FAQSQuestRecord::FAQSQuestRecord(const  UAQSQuest* quest)
{
    if (quest) {
        Quest = quest->GetQuestTag();
        for (const auto& obj : quest->GetAllActiveObjectives()) {
            const FAQSObjectiveRecord objRecord(obj);

            //The load path finds the node back by id, a record without one could never be restored
            if (objRecord.ObjectiveId.IsValid()) {
                Objectives.Add(objRecord);
            }
            else {
                UE_LOG(LogTemp, Warning, TEXT("Active objective of quest %s has no owning node, it will not be saved - FAQSQuestRecord::FAQSQuestRecord"), *quest->GetQuestTag().ToString());
            }
        }
        CompletedObjectives = quest->GetCompletedObjectives();
    }

}