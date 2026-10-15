// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AQSQuestManagerComponent.h"
#include "AQSQuestTargetComponent.h"
#include "AQSTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Graph/AQSQuest.h"
#include "Net/UnrealNetwork.h"
#include <Containers/Map.h>
#include <Kismet/GameplayStatics.h>
#include "AGSGraph.h"
#include "UObject/UObjectGlobals.h"

/*FAQSQuestRecord::operator== only compares the quest tag, since it's the key used by the
FastArraySerializer, so we need a real content comparison to tell whether a record actually
needs to be rewritten (and replicated) or not.*/
static bool AreQuestRecordsEquivalent(const FAQSQuestRecord& lhs, const FAQSQuestRecord& rhs)
{
	if (lhs.Quest != rhs.Quest || lhs.CompletedObjectives != rhs.CompletedObjectives
		|| lhs.Objectives.Num() != rhs.Objectives.Num()) {
		return false;
	}

	for (int32 index = 0; index < lhs.Objectives.Num(); index++) {
		if (lhs.Objectives[index].ObjectiveId != rhs.Objectives[index].ObjectiveId
			|| lhs.Objectives[index].CurrentRepetitions != rhs.Objectives[index].CurrentRepetitions) {
			return false;
		}
	}

	return true;
}

// Sets default values for this component's properties
UAQSQuestManagerComponent::UAQSQuestManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// ...
}

void UAQSQuestManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAQSQuestManagerComponent, CompletedQuestsTags);
	DOREPLIFETIME(UAQSQuestManagerComponent, FailedQuestsTags);
	DOREPLIFETIME(UAQSQuestManagerComponent, InProgressQuestsRecords);
	DOREPLIFETIME(UAQSQuestManagerComponent, TrackedQuestTag);
}
// Called when the game starts
void UAQSQuestManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!OnQuestEnded.IsAlreadyBound(this, &UAQSQuestManagerComponent::HandleAutoStartQuestCheck)) {
		OnQuestEnded.AddDynamic(this, &UAQSQuestManagerComponent::HandleAutoStartQuestCheck);
	}
}

void UAQSQuestManagerComponent::OnComponentSaved_Implementation()
{
	// The records are the only thing that gets serialized, while the real state lives in the
	// quest graphs: they're only written when an objective is completed, so resnapshot them
	// here to catch everything else (a quest just started, branched objectives, repetitions).
	// CompletedQuestsTags / FailedQuestsTags are kept up to date incrementally and their quest
	// object arrays can be empty in a loaded session, so they must NOT be rebuilt from here.
	// Records are server authoritative: on a client they are the replicated truth and the local
	// graphs are just a mirror of them, so there is nothing to resnapshot there.
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}

	if (TrackedQuest && TrackedQuestTag != TrackedQuest->GetQuestTag()) {
		TrackedQuestTag = TrackedQuest->GetQuestTag();
	}

	for (const UAQSQuest* quest : InProgressQuests) {
		if (!quest) {
			continue;
		}

		const FAQSQuestRecord updatedRecord(quest);
		const FAQSQuestRecord* currentRecord = InProgressQuestsRecords.GetQuest(quest->GetQuestTag());

		/*Rewriting a record marks it dirty and replicates it, which makes every client resynch
		its graphs: only pay that (and only touch the saved data) when the record really drifted
		from the graph, so that saving in an already consistent session stays a no-op.*/
		if (!currentRecord || !AreQuestRecordsEquivalent(*currentRecord, updatedRecord)) {
			InProgressQuestsRecords.AddQuest(updatedRecord);
		}
	}
}

void UAQSQuestManagerComponent::OnComponentLoaded_Implementation()
{
	SyncGraphs();
}

void UAQSQuestManagerComponent::ServerCompleteObjective_Implementation(const FGuid& objectiveToComplete)
{
	CompleteObjectiveByNodeID(objectiveToComplete);
}

void UAQSQuestManagerComponent::ServerCompleteObjectiveByTag_Implementation(const FGameplayTag& objectiveToComplete)
{
	CompleteObjective(objectiveToComplete);

}

void UAQSQuestManagerComponent::ServerCompleteBranchedObjective_Implementation(const FGuid& objectiveToComplete, const TArray<FName>& optionalTransitionFilters)
{
	CompleteBranchedObjective(objectiveToComplete, optionalTransitionFilters);
}

void UAQSQuestManagerComponent::ServerStartQuest_Implementation(const FGameplayTag& questTag)
{
	StartQuest(questTag);
}

void UAQSQuestManagerComponent::ServerRemoveInProgressQuest_Implementation(const FGameplayTag& questTag)
{
	UAQSQuest* quest = GetQuestFromDB(questTag);
	RemoveInProgressQuest(quest);
}

bool UAQSQuestManagerComponent::StartQuest(const FGameplayTag& questTag)
{
	UAQSQuest* questToStart = GetQuestFromDB(questTag);
	const bool bStartChildNodes = true;
	const bool bStarted = Internal_StartQuest(questToStart, bStartChildNodes, bAutoTrackQuest);
	if (bStarted) {
		OnQuestStarted.Broadcast(questTag);
		if (UKismetSystemLibrary::IsDedicatedServer(this) && !UKismetSystemLibrary::IsStandalone(this)) {
			ClientDispatchQuestStarted(questTag);
		}
	}
	return bStarted;
}

bool UAQSQuestManagerComponent::StopQuest(const FGameplayTag& questTag)
{
	if (!IsQuestInProgressByTag(questTag)) {
		return false;
	}

	const FAQSQuestRecord* questRec = InProgressQuestsRecords.GetQuest(questTag);

	if (!questRec) {
		return false;
	}
	UAQSQuest* questToStop = GetQuestFromDB(questTag);
	if (InProgressQuests.Contains(questToStop)) {

		questToStop->ResetQuest();

		InProgressQuests.Remove(questToStop);

		if (TrackedQuestTag == questTag) {
			UntrackCurrentQuest();
		}
		InProgressQuestsRecords.Remove(FAQSQuestRecord(questToStop));

		questToStop->OnQuestEnded.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleQuestCompleted);

		questToStop->OnObjectiveStarted.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveStarted);
		questToStop->OnObjectiveCompleted.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveCompleted);
		questToStop->OnObjectiveUpdated.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveUpdated);

		OnQuestEnded.Broadcast(questTag, false);
		if (UKismetSystemLibrary::IsDedicatedServer(this) && !UKismetSystemLibrary::IsStandalone(this)) {
			ClientDispatchQuestCompleted(questTag, false);
		}
		return true;
	}

	return false;
}

bool UAQSQuestManagerComponent::Internal_StartQuest(UAQSQuest* questToStart, const bool bStartChildNodes, bool autoTrack)
{
	if (!questToStart) {
		return false;
	}

	if (IsQuestInProgress(questToStart) || CompletedQuestsTags.Contains(questToStart->GetQuestTag()) || FailedQuestsTags.Contains(questToStart->GetQuestTag())) {
		UE_LOG(LogTemp, Warning, TEXT("Quest is already Started!- UAQSQuestManagerComponent::StartQuest"));
		return false;
	}
	APlayerController* playerController = Cast<APlayerController>(GetOwner());

	if (!playerController) {
		UE_LOG(LogTemp, Error, TEXT("Quest Manager owner is not a PlayerController! - UAQSQuestManagerComponent::StartQuest"));
	}
	if (questToStart && questToStart->StartQuest(playerController, this, bStartChildNodes)) {
		InProgressQuests.Add(questToStart);
		InProgressQuestsRecords.AddQuest(FAQSQuestRecord(questToStart));

		BindQuestEvents(questToStart);


		if (!TrackedQuest && autoTrack) {
			TrackInProgressQuest(questToStart);
		}

		return true;
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Invalid quest - UAQSQuestManagerComponent::StartQuest"));
	}
	return false;
}

void UAQSQuestManagerComponent::BindQuestEvents(UAQSQuest* questToStart)
{
	if (!questToStart->OnQuestEnded.IsAlreadyBound(this, &UAQSQuestManagerComponent::HandleQuestCompleted)) {
		questToStart->OnQuestEnded.AddDynamic(this, &UAQSQuestManagerComponent::HandleQuestCompleted);
	}
	if (!questToStart->OnObjectiveStarted.IsAlreadyBound(this, &UAQSQuestManagerComponent::HandleObjectiveStarted)) {
		questToStart->OnObjectiveStarted.AddDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveStarted);
	}
	if (!questToStart->OnObjectiveCompleted.IsAlreadyBound(this, &UAQSQuestManagerComponent::HandleObjectiveCompleted)) {
		questToStart->OnObjectiveCompleted.AddDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveCompleted);
	}
	if (!questToStart->OnObjectiveUpdated.IsAlreadyBound(this, &UAQSQuestManagerComponent::HandleObjectiveUpdated)) {
		questToStart->OnObjectiveUpdated.AddDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveUpdated);
	}
}

bool UAQSQuestManagerComponent::CompleteObjective(FGameplayTag objectiveToComplete)
{
	for (UAQSQuest* quest : InProgressQuests) {
		FGuid objectiveId;
		if (quest->FromTagToID(objectiveToComplete, objectiveId)) {
			CompleteObjectiveByNodeID(objectiveId);
		}
	}
	return false;
}

bool UAQSQuestManagerComponent::CompleteObjectiveByNodeID(const FGuid& objectiveID)
{
	if (GetOwner()->HasAuthority())
	{
		for (UAQSQuest* quest : InProgressQuests) {
			if (quest->HasActiveObjective(objectiveID)) {
				const bool bCompleted = quest->CompleteObjective(objectiveID);
				if (bCompleted) {
					UpdateInProgressQuestRecords(quest);
				}
				return bCompleted;
			}
		}
	}
	return false;
}

void UAQSQuestManagerComponent::UpdateInProgressQuestRecords(UAQSQuest* quest)
{
	//We only update it if it's still in progress (may be completed or failed)
	if (InProgressQuestsRecords.Contains(quest->QuestTag)) {
		InProgressQuestsRecords.AddQuest(FAQSQuestRecord(quest));
	}
	OnInProgressQuestsUpdate.Broadcast();
}

TArray<UAQSQuestObjective*> UAQSQuestManagerComponent::GetCurrentlyTrackedQuestObjectives() const
{
	if (TrackedQuest) {
		return TrackedQuest->GetAllActiveObjectives();
	}
	return TArray<UAQSQuestObjective*>();
}

TArray<UAQSQuestTargetComponent*> UAQSQuestManagerComponent::GetCurrentlyTrackedQuestObjectivesTargets() const
{
	const TArray<UAQSQuestObjective*> objectives = GetCurrentlyTrackedQuestObjectives();
	TArray<UAQSQuestTargetComponent*> targets;

	for (const UAQSQuestObjective* obj : objectives) {
		targets.Append(obj->GetObjectiveTargets());
	}
	return targets;
}

TArray<AActor*> UAQSQuestManagerComponent::GetCurrentlyTrackedQuestObjectivesTargetActors() const
{
	const TArray<UAQSQuestObjective*> objectives = GetCurrentlyTrackedQuestObjectives();
	TArray<AActor*> targets;

	for (const UAQSQuestObjective* obj : objectives) {
		targets.Append(obj->GetObjectiveTargetsActors());
	}
	return targets;
}

bool UAQSQuestManagerComponent::IsObjectiveInProgress(const FGuid& objectiveId) const
{
	for (const auto& quest : InProgressQuestsRecords.Quests) {
		if (DoesQuestHaveInProgressObjective(quest, objectiveId)) {
			return true;
		}
	}
	return false;
}

bool UAQSQuestManagerComponent::IsObjectiveInProgressByTag(const FGameplayTag& objectiveTag) const
{
	for (const auto& quest : InProgressQuestsRecords.Quests) {
		if (DoesQuestHaveInProgressObjectiveByTag(quest, objectiveTag)) {
			return true;
		}
	}
	return false;
}


class UAQSQuest* UAQSQuestManagerComponent::GetQuestFromDB(const FGameplayTag& questTag)
{
	UAQSQuest* newQuest = GetQuest(questTag);
	if (newQuest) {
		return newQuest;
	}
	if (!QuestsDB) {
		UE_LOG(LogTemp, Error, TEXT("Missing Quests Database from Quest Manager! - UAQSQuestManagerComponent::GetQuestFromDB"));
		return nullptr;
	}

	for (const auto& it : QuestsDB->GetRowMap()) {
		FAQSQuestData* questStruct = (FAQSQuestData*)(it.Value);
		if (!questStruct) {
			break;
		}
		UAQSQuest* quest = questStruct->Quest;
		if (!quest) {
			continue;
		}
		if (quest->GetQuestTag() == questTag) {
			// This is a runtime instance, not a new authored asset. Preserve text identities:
			// normal editor duplication can regenerate FText keys when changing packages.
			FObjectDuplicationParameters duplicateParams(quest, GetOuter());
			duplicateParams.PortFlags |= PPF_DuplicateVerbatim;
			newQuest = Cast<UAQSQuest>(StaticDuplicateObjectEx(duplicateParams));
			loadedQuests.Add(questTag, newQuest);
			return newQuest;
		}
	}
	return nullptr;
}

class UAQSQuest* UAQSQuestManagerComponent::GetQuest(const FGameplayTag& questTag) const
{
	if (loadedQuests.Contains(questTag)) {
		return loadedQuests.FindChecked(questTag);
	}
	return nullptr;
}

class UAQSQuestObjective* UAQSQuestManagerComponent::GetQuestObjectiveFromDB(const FGuid& objectiveId, const FGameplayTag& questTag) const
{
	const UAQSQuest* quest = GetQuest(questTag);
	if (quest) {
		return quest->GetObjectiveById(objectiveId);
	}
	return nullptr;
}



bool UAQSQuestManagerComponent::TryGetInProgressObjectiveInfo(const FGameplayTag& questTag, const FGuid& objectiveId, FAQSObjectiveInfo& outObjectiveInfo) const
{
	if (!IsQuestInProgressByTag(questTag) || !IsObjectiveInProgress(objectiveId)) {
		return false;
	}

	const FAQSQuestRecord* questRec = InProgressQuestsRecords.GetQuest(questTag);

	if (!questRec) {
		return false;
	}

	const FAQSObjectiveRecord* objectiveRec = questRec->Objectives.FindByKey(objectiveId);
	if (!objectiveRec) {
		return false;
	}

	const UAQSQuest* quest = GetQuest(questTag);
	if (quest) {
		const UAQSObjectiveNode* node = quest->GetActiveObjectiveNode(objectiveId);
		if (node) {
			const UAQSQuestObjective* objective = node->GetQuestObjective();
			if (objective) {
				outObjectiveInfo = FAQSObjectiveInfo(objective, *objectiveRec);
				return true;
			}
		}
	}
	return false;
}

bool UAQSQuestManagerComponent::TryGetInProgressQuestInfo(const FGameplayTag& questTag, FAQSQuestInfo& outQuestInfo)
{
	if (!IsQuestInProgressByTag(questTag)) {
		return false;
	}

	const FAQSQuestRecord* questRec = InProgressQuestsRecords.GetQuest(questTag);

	if (!questRec) {
		return false;
	}

	const UAQSQuest* quest = GetQuestFromDB(questTag);
	if (quest) {
		outQuestInfo = FAQSQuestInfo(quest, *questRec);
		return true;
	}

	return false;
}

bool UAQSQuestManagerComponent::TryGetTargetActorsForInProgressObjective(const FGuid& objectiveId, const FGameplayTag& questTag, TArray<class AActor*>& outTargets)
{
	UAQSQuestObjective* objective = GetQuestObjectiveFromDB(objectiveId, questTag);

	if (!objective) {
		return false;
	}

	outTargets = objective->GetObjectiveTargetsActors();
	return IsObjectiveInProgress(objectiveId);
}

bool UAQSQuestManagerComponent::IsObjectiveCompletedByTag(const FGameplayTag& questTag, const FGameplayTag& objectiveTag) const
{
	if (IsQuestCompletedByTag(questTag)) {
		return true;
	}
	else if (IsQuestInProgressByTag(questTag)) {
		const UAQSQuest* quest = GetQuest(questTag);
		if (quest) {
			FGuid objectiveId;
			if (quest->FromTagToID(objectiveTag, objectiveId)) {
				return quest->IsObjectiveCompleted(objectiveId);
			}

		}
	}

	return false;
}

/*

void UAQSQuestManagerComponent::GetInProgressQuestsInfo(TArray<FAQSQuestInfo>& outInfo) const
{
	for (const auto& quest : InProgressQuestsRecords.Quests) {
		FAQSQuestInfo questInfo;
		if (TryGetInProgressQuestInfo(quest.Quest, questInfo)) {
			outInfo.Add(questInfo);
		}
	}
}*/

UAQSQuestSuccededNode* UAQSQuestManagerComponent::GetQuestSucceededNode(const FGameplayTag& questTag)
{
	UAQSQuest* quest = GetQuestFromDB(questTag);
	if (quest) {
		return quest->GetQuestSucceededNode();
	}
	return nullptr;
}

UAQSQuestFailedNode* UAQSQuestManagerComponent::GetQuestFailedNode(const FGameplayTag& questTag)
{
	UAQSQuest* quest = GetQuestFromDB(questTag);
	if (quest) {
		return quest->GetQuestFailedNode();
	}
	return nullptr;
}

bool UAQSQuestManagerComponent::CompleteBranchedObjective(const FGuid& objectiveToComplete, TArray<FName> optionalTransitionFilters)
{
	for (UAQSQuest* quest : InProgressQuests) {
		if (quest->HasActiveObjective(objectiveToComplete)) {
			const bool bCompleted = quest->CompleteBranchedObjective(objectiveToComplete, optionalTransitionFilters);
			return bCompleted;
		}
	}
	return false;
}

void UAQSQuestManagerComponent::TrackInProgressQuestByTag(const FGameplayTag& questTag)
{
	UAQSQuest* quest = GetQuestFromDB(questTag);
	if (quest) {
		TrackInProgressQuest(quest);
	}
}

bool UAQSQuestManagerComponent::TrackInProgressQuest(class UAQSQuest* questToTrack)
{
	if (questToTrack == TrackedQuest) {
		return true;
	}
	if (InProgressQuests.Contains(questToTrack)) {
		UntrackCurrentQuest();
		TrackedQuest = questToTrack;
		TrackedQuestTag = questToTrack->GetQuestTag();
		TrackedQuest->SetQuestTracked(true);
		OnTrackedQuestChanged.Broadcast();
		return true;
	}

	return false;
}

void UAQSQuestManagerComponent::UntrackCurrentQuest()
{
	if (TrackedQuest) {
		TrackedQuestTag = FGameplayTag();
		TrackedQuest->SetQuestTracked(false);
		TrackedQuest = nullptr;
		OnTrackedQuestChanged.Broadcast();
	}
}

TArray<class UAQSQuestTargetComponent*> UAQSQuestManagerComponent::GetAllTargetsWithTag(const FGameplayTag& targetTag) const
{
	TArray<UAQSQuestTargetComponent*> RegisteredTargets;
	QuestTargets.MultiFind(targetTag, RegisteredTargets);

	TArray<UAQSQuestTargetComponent*> ValidTargets;
	ValidTargets.Reserve(RegisteredTargets.Num());
	for (UAQSQuestTargetComponent* Target : RegisteredTargets) {
		if (IsValid(Target)) {
			ValidTargets.AddUnique(Target);
		}
	}
	return ValidTargets;
}

TArray<class UAQSQuestTargetComponent*> UAQSQuestManagerComponent::GetAllTargetsWithTags(const TArray<FGameplayTag>& targetTags) const
{
	TArray<UAQSQuestTargetComponent*> outArray;
	for (const auto& tag : targetTags) {
		outArray.Append(GetAllTargetsWithTag(tag));
	}

	return outArray;
}

void UAQSQuestManagerComponent::RegisterTarget(UAQSQuestTargetComponent* targetComp)
{
	if (IsValid(targetComp) && targetComp->GetTargetTag().IsValid()) {
		QuestTargets.AddUnique(targetComp->GetTargetTag(), targetComp);
	}
}

void UAQSQuestManagerComponent::UnregisterTarget(class UAQSQuestTargetComponent* targetComp)
{
	if (targetComp) {
		QuestTargets.RemoveSingle(targetComp->GetTargetTag(), targetComp);
	}
}

void UAQSQuestManagerComponent::DispatchObjectiveUpdate(const FGuid& objectiveId, const FGameplayTag& questTag, EQuestUpdateType updateType)
{
	const UAQSQuest* quest = GetQuestFromDB(questTag);
	if (!quest) {
		return;
	}
	const UAQSQuestObjective* questObjective = quest->GetActiveObjective(objectiveId);

	if (!questObjective) {
		return;
	}

	//OnInProgressQuestsUpdate.Broadcast();

	bool bIsTracked = false;
	if (GetCurrentlyTrackedQuestTag() == questTag) {
		bIsTracked = true;
		OnTrackedQuestUpdated.Broadcast();
	}

	switch (updateType) {
	case EQuestUpdateType::EStarted:

		OnObjectiveStarted.Broadcast(questObjective->GetObjectiveId(), questTag);
		break;
	case EQuestUpdateType::ECompleted:

		OnObjectiveCompleted.Broadcast(questObjective->GetObjectiveId(), questTag);
		break;
	case EQuestUpdateType::EUpdated:

		OnObjectiveUpdated.Broadcast(questObjective->GetObjectiveId(), questTag);
		break;
	case EQuestUpdateType::EInterrupted:
		OnObjectiveInterrupted.Broadcast(questObjective->GetObjectiveId(), questTag);
		break;
	}
	TArray<UAQSQuestTargetComponent*> targetComps = questObjective->GetObjectiveTargets();
	for (UAQSQuestTargetComponent* targetComp : targetComps) {

		if (IsValid(targetComp)) {
			targetComp->DispatchObjectiveUpdated(questObjective->GetObjectiveId(), questTag, updateType, bIsTracked);
		}
	}
}

void UAQSQuestManagerComponent::NotifyTargetLoaded(UAQSQuestTargetComponent* targetComp)
{
	if (!IsValid(targetComp)) {
		return;
	}

	for (UAQSQuest* quest : InProgressQuests) {
		if (!IsValid(quest)) {
			continue;
		}

		const FGameplayTag questTag = quest->GetQuestTag();
		for (UAQSQuestObjective* objective : quest->GetAllActiveObjectives()) {
			if (!IsValid(objective) || !objective->TryAddLateLoadedTarget(targetComp)) {
				continue;
			}

			if (objective->IsWaitingForTargets()) {
				objective->TryDispatchObjectiveStarted();
				continue;
			}

			targetComp->DispatchObjectiveUpdated(objective->GetObjectiveId(), questTag, EQuestUpdateType::EStarted, objective->IsTracked());
			DispatchObjectiveTargetsLoaded(objective->GetObjectiveId(), questTag);
		}
	}
}

void UAQSQuestManagerComponent::DispatchObjectiveTargetsLoaded(const FGuid& objectiveId, const FGameplayTag& questTag)
{
	OnObjectiveTargetsLoaded.Broadcast(objectiveId, questTag);

	/*The objective already broadcast its start while its targets were still streaming in, so any
	listener that queried the target list back then got an empty one. Replay the regular update
	events to give quest trackers, markers and compasses a chance to pick the targets up.*/
	OnObjectiveUpdated.Broadcast(objectiveId, questTag);

	if (GetCurrentlyTrackedQuestTag() == questTag) {
		OnTrackedQuestUpdated.Broadcast();
	}

	OnInProgressQuestsUpdate.Broadcast();
}




void UAQSQuestManagerComponent::ClientDispatchQuestStarted_Implementation(const FGameplayTag& questTag)
{
	OnQuestStarted.Broadcast(questTag);
}

void UAQSQuestManagerComponent::ClientDispatchQuestCompleted_Implementation(const FGameplayTag& questTag, bool bSucceded)
{
	OnQuestEnded.Broadcast(questTag, bSucceded);
}


void UAQSQuestManagerComponent::DEBUG_ProceedQuest(const FGameplayTag& inProgressQuest)
{
	UAQSQuest* quest = GetQuest(inProgressQuest);
	if (quest) {
		const TArray<UAQSQuestObjective*> objectives = quest->GetAllActiveObjectives();
		for (const auto& obj : objectives) {
			ServerCompleteObjective(obj->GetObjectiveId());
		}
	}
}

void UAQSQuestManagerComponent::DEBUG_ProceedTrackedQuest()
{
	DEBUG_ProceedQuest(GetCurrentlyTrackedQuestTag());
}

void UAQSQuestManagerComponent::DEBUG_RemoveInProgressQuest(const FGameplayTag& inProgressQuest)
{
	UAQSQuest* quest = GetQuestFromDB(inProgressQuest);

	if (InProgressQuests.Contains(quest)) {
		InProgressQuests.Remove(quest);
		InProgressQuestsRecords.Remove(FAQSQuestRecord(quest));
	}
}

void UAQSQuestManagerComponent::OnRep_TrackedQuest()
{
	OnTrackedQuestChanged.Broadcast();
}

void UAQSQuestManagerComponent::OnRep_InProgressQuestsRecords()
{
	SynchInProgressQuest();
	OnInProgressQuestsUpdate.Broadcast();
}

void UAQSQuestManagerComponent::OnRep_FailedQuestsTags()
{
	SynchFailedQuests();
	OnFailedQuestsUpdate.Broadcast();
}

void UAQSQuestManagerComponent::OnRep_CompletedQuestsTags()
{
	SynchCompletedQuests();
	OnCompletedQuestsUpdate.Broadcast();
}

void UAQSQuestManagerComponent::HandleQuestCompleted(const FGameplayTag& questToComplete, bool bSuccesful)
{
	if (InProgressQuestsRecords.Contains(questToComplete)) {
		UAQSQuest* quest = GetQuest(questToComplete);

		if (quest) {
			if (TrackedQuestTag == questToComplete) {
				UntrackCurrentQuest();
			}
			InProgressQuests.Remove(quest);
			InProgressQuestsRecords.Remove(FAQSQuestRecord(quest));

			if (bSuccesful) {
				CompletedQuests.AddUnique(quest);
				CompletedQuestsTags.AddUnique(questToComplete);
				OnCompletedQuestsUpdate.Broadcast();
			}
			else {
				FailedQuests.AddUnique(quest);
				FailedQuestsTags.AddUnique(questToComplete);
				OnFailedQuestsUpdate.Broadcast();
			}
			UnbindQuestEvents(quest);

		}

		OnQuestEnded.Broadcast(questToComplete, bSuccesful);
		if (UKismetSystemLibrary::IsDedicatedServer(this) && !UKismetSystemLibrary::IsStandalone(this)) {
			ClientDispatchQuestCompleted(questToComplete, bSuccesful);
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Invalid quest!! - UAQSQuestManagerComponent::HandleQuestCompleted"));
	}
}

void UAQSQuestManagerComponent::HandleAutoStartQuestCheck(const FGameplayTag& completedQuest, bool bSuccessful)
{
	AActor* owner = GetOwner();
	if (!bSuccessful || !owner || !owner->HasAuthority()) {
		return;
	}

	for (const TPair<FGameplayTag, FAQSQuestAutoStartRequirements>& autoStartEntry : AutoStartQuestRequirements) {
		const FGameplayTag& questToStart = autoStartEntry.Key;
		const TArray<FGameplayTag>& requiredQuests = autoStartEntry.Value.RequiredCompletedQuests;

		if (!questToStart.IsValid() || requiredQuests.IsEmpty()
			|| IsQuestInProgressByTag(questToStart)
			|| IsQuestCompletedByTag(questToStart)
			|| IsQuestFailedByTag(questToStart)) {
			continue;
		}

		// Only entries depending on this event can have become newly eligible.
		if (!requiredQuests.Contains(completedQuest)) {
			continue;
		}

		bool bAllRequirementsCompleted = true;
		for (const FGameplayTag& requiredQuest : requiredQuests) {
			if (!requiredQuest.IsValid() || !IsQuestCompletedByTag(requiredQuest)) {
				bAllRequirementsCompleted = false;
				break;
			}
		}

		if (bAllRequirementsCompleted) {
			StartQuest(questToStart);
		}
	}
}

void UAQSQuestManagerComponent::UnbindQuestEvents(UAQSQuest* quest)
{
	quest->OnQuestEnded.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleQuestCompleted);

	quest->OnObjectiveStarted.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveStarted);
	quest->OnObjectiveCompleted.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveCompleted);
	quest->OnObjectiveUpdated.RemoveDynamic(this, &UAQSQuestManagerComponent::HandleObjectiveUpdated);
}

void UAQSQuestManagerComponent::HandleObjectiveStarted(const FGuid& objective, const FGameplayTag& quest)
{
	OnObjectiveStarted.Broadcast(objective, quest);
}

void UAQSQuestManagerComponent::HandleObjectiveCompleted(const FGuid& objective, const FGameplayTag& quest)
{
	OnObjectiveCompleted.Broadcast(objective, quest);
}

void UAQSQuestManagerComponent::HandleObjectiveUpdated(const FGuid& objective, const FGameplayTag& quest)
{
	OnObjectiveUpdated.Broadcast(objective, quest);
}

void UAQSQuestManagerComponent::SyncGraphs()
{

	SynchCompletedQuests();
	SynchFailedQuests();
	SynchInProgressQuest();

}

void UAQSQuestManagerComponent::SynchInProgressQuest()
{
	// 	if (GetOwner()->HasAuthority()) {
	// 		return;
	// 	}

		//InProgressQuests.Empty();
		// this NEEDS to be a copy! Otherwise the quest update will override it
	TArray<FAQSQuestRecord> tempRecords = InProgressQuestsRecords.Quests;
	for (const FAQSQuestRecord& questData : tempRecords) {
		UAQSQuest* quest = GetQuestFromDB(questData.Quest);

		if (quest) {

			if (!quest->GetIsStarted()) {
				APlayerController* playerController = Cast<APlayerController>(GetOwner());
				//Internal_StartQuest(quest, false, false);
				quest->StartQuest(playerController, this, false);
				BindQuestEvents(quest);
				InProgressQuests.AddUnique(quest);
			}

			quest->SetCompletedObjectives(questData.CompletedObjectives);

			// 1 is already completed, the other one not -> save game -> load again,
			// then if the objective is completed, it won't start the next objective node.
			// Because completed state is not set, so we set all the objectiveNodes to completed that we saved.
			// So we loop all Saved CompletedObjectives here and set the state.
			for (const FGuid& Tag : questData.CompletedObjectives)
			{
				UAQSObjectiveNode* ObjectiveNode = quest->GetObjectiveNode(Tag);
				if (ObjectiveNode)
				{
					ObjectiveNode->SetObjectiveCompleted(true);
				}
			}

			for (const auto& obj : questData.Objectives) {
				UAQSObjectiveNode* objNode = Cast<UAQSObjectiveNode>(quest->GetNodeById(obj.ObjectiveId));
				if (objNode) {
					if (!objNode->IsNodeActivated()) {
						quest->ActivateNode(objNode);
					}

					objNode->SetCurrentRepetitions(obj.CurrentRepetitions);
				}
			}
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Invalid Quest - UAQSQuestManagerComponent::Synch Graph"));
		}

	}

	UAQSQuest* quest = GetQuestFromDB(TrackedQuestTag);
	if (quest) {
		TrackInProgressQuest(quest);
	}
}

void UAQSQuestManagerComponent::SynchFailedQuests()
{
	FailedQuests.Empty();
	for (const auto& questData : FailedQuestsTags) {
		UAQSQuest* quest = GetQuestFromDB(questData);
		if (quest) {
			FailedQuests.Add(quest);
		}
		if (InProgressQuests.Contains(quest)) {
			InProgressQuests.Remove(quest);
		}
	}
}

void UAQSQuestManagerComponent::SynchCompletedQuests()
{
	CompletedQuests.Empty();
	for (const auto& questData : CompletedQuestsTags) {
		UAQSQuest* quest = GetQuestFromDB(questData);
		if (quest) {
			CompletedQuests.Add(quest);
		}
		if (InProgressQuests.Contains(quest)) {
			InProgressQuests.Remove(quest);
		}
	}
}

bool UAQSQuestManagerComponent::RemoveInProgressQuest(UAQSQuest* quest)
{
	if (!GetOwner()->HasAuthority()) {
		return false;
	}
	if (IsQuestInProgress(quest)) {
		InProgressQuests.Remove(quest);
		InProgressQuestsRecords.Remove(FAQSQuestRecord(quest));
		return true;
	}
	return false;
}
