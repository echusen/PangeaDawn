// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Graph/AQSObjectiveNode.h"
#include "AGSCondition.h"
#include "AQSQuestManagerComponent.h"
#include "AQSQuestObjective.h"
#include "AQSQuestTargetComponent.h"
#include "Graph/AQSQuest.h"
#include <GameFramework/Actor.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>
#include "ADSDialogueFunctionLibrary.h"

void UAQSObjectiveNode::ActivateNode()
{
    Super::ActivateNode();
    CurrentRepetitions = 0;
    bInterrupted = false;
    bIsCompleted = false;
    if (const UAQSQuest* Quest = Cast<UAQSQuest>(GetGraph())) {
        bIsTracked = Quest->IsCurrentTrackedQuest();
    }
    if (!Objective) {
        Objective = NewObject<UAQSQuestObjective>();
    }

    if (Objective) {
        Objective->Internal_OnObjectiveStarted(this);
    } else {
        UE_LOG(LogTemp, Error, TEXT("Missing Objective! - UAQSObjectiveNode::ActivateNode"));
    }
}

void UAQSObjectiveNode::DeactivateNode()
{
    const bool bObjectiveStarted = Objective && Objective->HasDispatchedObjectiveStarted();
    if (bObjectiveStarted) {
        if (!bInterrupted) {
            bIsCompleted = true;
            UpdateReferences(EQuestUpdateType::ECompleted);
        } else {
            UpdateReferences(EQuestUpdateType::EInterrupted);
        }
    }
    if (Objective) {
        Objective->Internal_OnObjectiveEnded(bInterrupted);
    } else {
        UE_LOG(LogTemp, Error, TEXT("Missing Objective! - UAQSObjectiveNode::DeactivateNode"));
    }
    Super::DeactivateNode();
}

UAQSObjectiveNode::UAQSObjectiveNode()
{
#if WITH_EDITOR
    BackgroundColor = FLinearColor::Black;
#endif

    // Objective = CreateDefaultSubobject<UAQSQuestObjective>("Default Objective");
}

bool UAQSObjectiveNode::TryToComplete()
{
    if (!Objective) {
        UE_LOG(LogTemp, Error, TEXT("Missing Objective! - UAQSObjectiveNode::TryToComplete"));
        return false;
    }
    if (!Objective->HasDispatchedObjectiveStarted()) {
        if (Objective->IsWaitingForTargets()) {
            return false;
        }
        UE_LOG(LogTemp, Warning,
            TEXT("Objective '%s' had not started; starting it now so the quest cannot stall."),
            *GetObjectiveTag().ToString());
        Objective->DispatchObjectiveStarted();
    }

    CurrentRepetitions++;

    UpdateReferences(EQuestUpdateType::EUpdated);
    if (CurrentRepetitions >= Repetitions) {

        for (UAGSCondition* condition : CompleteConditions) {
            if (!condition->VerifyForNode(GetPlayerController(), this)) {

                return false;
            }
        }
        CompleteNode();
        return true;
    }
    return false;
}

TArray<class UAQSQuestTargetComponent*> UAQSObjectiveNode::GetObjectiveTargets() const
{
    if (!Objective) {
        UE_LOG(LogTemp, Error, TEXT("Missing Objective! - UAQSObjectiveNode::GetObjectiveTargets"));
        return TArray<UAQSQuestTargetComponent*>();
    }

    return Objective->GetObjectiveTargets();
}

FGameplayTag UAQSObjectiveNode::GetObjectiveTag() const
{
    if (!Objective) {
        UE_LOG(LogTemp, Error, TEXT("Missing Objective! - UAQSObjectiveNode::GetObjectiveTag"));
        return FGameplayTag();
    }

    return Objective->GetObjectiveTag();
}

void UAQSObjectiveNode::InterruptObjective()
{
    bInterrupted = true;
    SetIsTracked(false);
}

void UAQSObjectiveNode::SetIsTracked(bool inTracked)
{
    bIsTracked = inTracked;
    if (Objective && Objective->HasDispatchedObjectiveStarted()) {
        UpdateReferences(EQuestUpdateType::EUpdated);
    }
}

void UAQSObjectiveNode::NotifyObjectiveStarted()
{
    UpdateReferences(EQuestUpdateType::EStarted);
}

void UAQSObjectiveNode::UpdateReferences(EQuestUpdateType updateType)
{
    if (!Objective) {
        UE_LOG(LogTemp, Error, TEXT("Missing Objective! - UAQSObjectiveNode::UpdateReferences"));
        return;
    }

    UAQSQuest* quest = Cast<UAQSQuest>(GetGraph());
    if (!quest) {
        UE_LOG(LogTemp, Error, TEXT("Missing Quest! - UAQSObjectiveNode::UpdateReferences"));
        return;
    }

    const FGameplayTag& questTag = quest->GetQuestTag();
    TObjectPtr<class UAQSQuestManagerComponent> questMan = quest->GetQuestManager();

    if (questMan) {
        questMan->DispatchObjectiveUpdate(Objective->GetObjectiveId(), questTag, updateType);
    }
}

#if WITH_EDITOR

FText UAQSObjectiveNode::GetNodeTitle() const
{
    if (!IsValid(Objective)) {
        return FText::FromString("Set Objective");
    }

    const FText ObjectiveTitle = Objective->GetRawName();
    if (ObjectiveTitle.IsEmpty()) {
        return FText::FromString("Set Objective Name");
    }
    return ObjectiveTitle;
}

FText UAQSObjectiveNode::GetParagraphTitle() const
{
    if (!Objective) {
        return FText::FromString("Set Objective");
    }
    // ETargetReferenceType refType = Objective->GetTa
    TArray<TSoftObjectPtr<AActor>> refActors = Objective->GetObjectiveTargetsSoftRef();
    if (refActors.IsEmpty()) {
        return FText::FromString("Missing Targets");
    } else {
       return FText::FromString(UADSDialogueFunctionLibrary::ExtractActorNameFromSoftRef(refActors[0])) ;
    }

  //  return FText::FromString("No Target");
}

#endif
