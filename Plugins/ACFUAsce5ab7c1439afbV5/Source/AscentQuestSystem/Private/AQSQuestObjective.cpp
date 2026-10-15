// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AQSQuestObjective.h"
#include "AQSQuestFunctionLibrary.h"
#include "AQSQuestManagerComponent.h"
#include "AQSQuestTargetComponent.h"
#include "Graph/AQSQuest.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ObjectSaveContext.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include <Kismet/KismetSystemLibrary.h>
#include <GameFramework/Actor.h>
#include <Kismet/GameplayStatics.h>

UAQSQuestObjective::UAQSQuestObjective()
{
	TargetRefType = ETargetReferenceType::ESoftRef;
	ObjectiveName = FText::FromString("Default Name");
	/*    ObjectiveDescription = "";*/
}

bool UAQSQuestObjective::GetStreamingSources(TArray<FWorldPartitionStreamingSource>& OutStreamingSources) const
{
	if (!bIsStreamingSourceRegistered || ForcedStreamingTargetIndices.IsEmpty()) {
		return false;
	}

	const int32 NumSourcesBefore = OutStreamingSources.Num();
	OutStreamingSources.Reserve(NumSourcesBefore + ForcedStreamingTargetIndices.Num());
	for (const int32 Index : ForcedStreamingTargetIndices) {
		if (!ReferencedActorStreamingLocations.IsValidIndex(Index) || !IsStreamingLocationResolved(Index)) {
			continue;
		}

		FWorldPartitionStreamingSource Source;
		Source.Name = FName(*FString::Printf(TEXT("AQS_%u_%d"), GetUniqueID(), Index));
		Source.Location = ReferencedActorStreamingLocations[Index];
		Source.Rotation = FRotator::ZeroRotator;
		Source.TargetState = EStreamingSourceTargetState::Activated;
		/*Never stall the game thread on a distant quest target: the objective simply
		waits in the LoadingTargets state until streaming is done.*/
		Source.bBlockOnSlowLoading = false;
		Source.Priority = EStreamingSourcePriority::High;

		/*Without an explicit shape World Partition falls back to the grid loading range,
		which would stream in everything around the target instead of just its own cell.
		The clamped radius keeps it to the target and its immediate surroundings.*/
		FStreamingSourceShape Shape;
		Shape.bUseGridLoadingRange = false;
		Shape.Radius = FMath::Clamp(ForcedTargetStreamingRadius, 1.0f, 5000.0f);
		Source.Shapes.Add(Shape);

		OutStreamingSources.Add(MoveTemp(Source));
	}
	return OutStreamingSources.Num() > NumSourcesBefore;
}

bool UAQSQuestObjective::IsStreamingLocationResolved(int32 Index) const
{
	/*Assets saved before the resolved flags existed have no array at all: keep trusting their
	cached locations so they behave as they did before, until they get saved again.*/
	if (ReferencedActorStreamingLocationsResolved.Num() != ReferencedActorStreamingLocations.Num()) {
		return true;
	}
	return ReferencedActorStreamingLocationsResolved.IsValidIndex(Index) && ReferencedActorStreamingLocationsResolved[Index];
}

bool UAQSQuestObjective::HasUsableStreamingCache() const
{
	return ReferencedActorStreamingLocations.Num() == ReferencedActors.Num();
}

bool UAQSQuestObjective::IsTargetLoadable(int32 Index) const
{
	return ForcedStreamingTargetIndices.Contains(Index);
}

TArray<class AActor*> UAQSQuestObjective::GetObjectiveTargetsActors() const
{
	TArray<class AActor*> outActors;
	for (const UAQSQuestTargetComponent* targetComp : questTargets) {
		if (IsValid(targetComp) && IsValid(targetComp->GetOwner())) {
			outActors.Add(targetComp->GetOwner());
		}
	}
	return outActors;
}

TArray<FVector> UAQSQuestObjective::GetKnownTargetLocations() const
{
	TArray<FVector> Locations;
	const TArray<AActor*> LiveActors = GetObjectiveTargetsActors();
	if (!LiveActors.IsEmpty()) {
		Locations.Reserve(LiveActors.Num());
		for (const AActor* Actor : LiveActors) {
			if (IsValid(Actor)) {
				Locations.Add(Actor->GetActorLocation());
			}
		}
		return Locations;
	}

	for (int32 Index = 0; Index < ReferencedActorStreamingLocations.Num(); ++Index) {
		if (IsStreamingLocationResolved(Index)) {
			Locations.Add(ReferencedActorStreamingLocations[Index]);
		}
	}
	return Locations;
}

TArray<TSoftObjectPtr<AActor>> UAQSQuestObjective::GetObjectiveTargetsSoftRef() const
{
	return ReferencedActors;
}

FText UAQSQuestObjective::GetObjectiveName_Implementation() const
{
	if (nodeOwner && GetRepetitions() > 1) {
		return FText::Format(NSLOCTEXT("AQS", "ObjectiveNameProgressFormat", "{0} {1}/{2}"),
			ObjectiveName, FText::AsNumber(GetCurrentRepetitions()), FText::AsNumber(GetRepetitions()));
	}
	return ObjectiveName;
}

FText UAQSQuestObjective::GetObjectiveDescription_Implementation() const
{
	return ObjectiveDescription;
}

APlayerController* UAQSQuestObjective::GetLocalPlayerController() const
{
	if (nodeOwner) {
		return nodeOwner->GetPlayerController();
	}
	return nullptr;
}

void UAQSQuestObjective::CompleteObjective()
{
	UAQSQuestManagerComponent* questMan = GetLocalQuestManager();
	if (questMan) {
		questMan->ServerCompleteObjective(GetObjectiveId());
	}
}

void UAQSQuestObjective::Internal_OnObjectiveStarted(UAQSObjectiveNode* inNodeOwner)
{
	nodeOwner = inNodeOwner;
	ActivationState = EObjectiveActivationState::EInactive;
	FillTargetsRef();

	if (bIsStreamingSourceRegistered) {
		ActivationState = EObjectiveActivationState::ELoadingTargets;
		TryDispatchObjectiveStarted();
		return;
	}

	DispatchObjectiveStarted();
}

bool UAQSQuestObjective::IsForcedStreamingCompleted() const
{
	if (!bIsStreamingSourceRegistered) {
		return true;
	}

	const UWorld* World = StreamingSourceWorld.Get();
	const UWorldPartitionSubsystem* WorldPartitionSubsystem = World ? World->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
	if (!WorldPartitionSubsystem) {
		return true;
	}

	return WorldPartitionSubsystem->IsStreamingCompleted(this);
}

void UAQSQuestObjective::DispatchObjectiveStarted()
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!IsValid(nodeOwner)) {
		UE_LOG(LogTemp, Error,
			TEXT("Quest objective '%s' has no node owner. The objective will still start."),
			*ObjectiveTag.ToString());
	}
	if (!IsValid(PlayerController)) {
		UE_LOG(LogTemp, Error,
			TEXT("Quest objective '%s' has no player controller. The objective will still start."),
			*ObjectiveTag.ToString());
	}

	ActivationState = EObjectiveActivationState::EActive;

	if (IsValid(PlayerController)) {
		if (PlayerController->HasAuthority()) {
			OnObjectiveStarted();
			if (PlayerController->IsLocalController()) {
				ClientOnObjectiveStarted();
			}
		}
		else {
			ClientOnObjectiveStarted();
		}
	}

	if (IsValid(nodeOwner)) {
		nodeOwner->NotifyObjectiveStarted();
	}
}

bool UAQSQuestObjective::TryDispatchObjectiveStarted()
{
	if (ActivationState == EObjectiveActivationState::EActive) {
		return true;
	}

	if (ActivationState == EObjectiveActivationState::ELoadingTargets && !IsForcedStreamingCompleted()) {
		return false;
	}

	if (ActivationState == EObjectiveActivationState::ELoadingTargets) {
		for (const int32 Index : ForcedStreamingTargetIndices) {
			if (!ReferencedActors.IsValidIndex(Index) || IsValid(ReferencedActors[Index].Get())) {
				continue;
			}
			UE_LOG(LogTemp, Warning,
				TEXT("Quest objective '%s' finished streaming but target '%s' is still missing. The objective will still start."),
				*ObjectiveTag.ToString(), *ReferencedActors[Index].ToSoftObjectPath().ToString());
		}
	}

	DispatchObjectiveStarted();
	return ActivationState == EObjectiveActivationState::EActive;
}

void UAQSQuestObjective::Internal_OnObjectiveEnded(bool bInterrupted)
{
	if (ActivationState == EObjectiveActivationState::EActive && bInterrupted) {
		if (GetLocalPlayerController()->HasAuthority()) {
			// In standalone / listen server, this pawn may also be locally controlled
			OnObjectiveInterrupted();
			if (GetLocalPlayerController()->IsLocalController()) {
				// Execute local feedback logic too
				ClientOnObjectiveInterrupted();
			}
		}
		else {
			ClientOnObjectiveInterrupted();
		}

	}
	else if (ActivationState == EObjectiveActivationState::EActive) {
		if (GetLocalPlayerController()->HasAuthority()) {
			// In standalone / listen server, this pawn may also be locally controlled
			OnObjectiveCompleted();
			if (GetLocalPlayerController()->IsLocalController()) {
				// Execute local feedback logic too
				ClientOnObjectiveCompleted();
			}
		}
		else {
			ClientOnObjectiveCompleted();
		}
	}

	ActivationState = EObjectiveActivationState::EInactive;
	ReleaseForcedTargets();
}

void UAQSQuestObjective::FillTargetsRef()
{
	questTargets.Empty();
	const UAQSQuestManagerComponent* questMan = UAQSQuestFunctionLibrary::GetQuestManager(this);

	switch (TargetRefType) {
	case ETargetReferenceType::ETag:
		if (questMan) {
			for (UAQSQuestTargetComponent* targetComp : questMan->GetAllTargetsWithTags(ReferencedTargets)) {
				questTargets.Add(targetComp);
			}
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Add Quest Manager Component to your player controller! - AQSQuestObjective::Internal_OnObjectiveStarted"));
		}
		break;
	case ETargetReferenceType::ESoftRef: {
		ReleaseForcedTargets();

		UWorld* World = GetWorld();
		const bool bWorldPartition = World && World->GetWorldPartition() != nullptr;
		bool bHasUnresolvedSoftRef = false;

		for (int32 Index = 0; Index < ReferencedActors.Num(); ++Index) {
			AActor* Actor = ReferencedActors[Index].Get();
			/*Non-WP maps (and already-loaded WP cells) resolve the soft ref directly.
			LoadSynchronous is only a fallback when there is no World Partition at all.*/
			if (!IsValid(Actor) && bForceLoadReferencedActors && !bWorldPartition) {
				Actor = ReferencedActors[Index].LoadSynchronous();
			}

			if (IsValid(Actor)) {
				UAQSQuestTargetComponent* TargetComp = Actor->FindComponentByClass<UAQSQuestTargetComponent>();
				if (IsValid(TargetComp)) {
					questTargets.AddUnique(TargetComp);
				}
				else {
					UE_LOG(LogTemp, Warning, TEXT("Actor '%s' referenced by objective '%s' has no AQSQuestTargetComponent. The objective will still start."),
						*Actor->GetPathName(), *ObjectiveTag.ToString());
				}
			}
			else {
				bHasUnresolvedSoftRef = true;
				const bool bCanStreamInBackground = bForceLoadReferencedActors && bWorldPartition
					&& HasUsableStreamingCache() && IsStreamingLocationResolved(Index);
				if (!bCanStreamInBackground) {
					UE_LOG(LogTemp, Warning, TEXT("Quest objective '%s' could not resolve target '%s'. The objective will still start without it."),
						*ObjectiveTag.ToString(), *ReferencedActors[Index].ToSoftObjectPath().ToString());
				}
			}
		}

		/*Force-load is the exception: only unresolved soft refs on a World Partition map
		need a streaming source. Already-valid actors must not go through this path.*/
		if (bForceLoadReferencedActors && bHasUnresolvedSoftRef && bWorldPartition && World->IsGameWorld()) {
			UWorldPartitionSubsystem* WorldPartitionSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>();
			if (WorldPartitionSubsystem && HasUsableStreamingCache()) {
				for (int32 Index = 0; Index < ReferencedActors.Num(); ++Index) {
					if (IsValid(ReferencedActors[Index].Get())) {
						continue;
					}

					if (IsStreamingLocationResolved(Index)) {
						ForcedStreamingTargetIndices.Add(Index);
					}
					else {
						UE_LOG(LogTemp, Warning,
							TEXT("Quest objective '%s' has no valid cached location for target '%s'. The objective will still start. Open and save the quest asset in the editor."),
							*ObjectiveTag.ToString(), *ReferencedActors[Index].ToSoftObjectPath().ToString());
					}
				}

				if (!ForcedStreamingTargetIndices.IsEmpty()) {
					bIsStreamingSourceRegistered = true;
					StreamingSourceWorld = World;
					WorldPartitionSubsystem->RegisterStreamingSourceProvider(this);

					ForcedTargetsWaitTicks = 0;
					World->GetTimerManager().SetTimer(
						ForcedTargetsRefreshTimer,
						this,
						&UAQSQuestObjective::RefreshForcedTargets,
						0.1f,
						true);
				}
			}
			else if (WorldPartitionSubsystem) {
				UE_LOG(LogTemp, Warning,
					TEXT("Quest objective '%s' cannot force-load referenced actors: cached streaming locations are missing. The objective will still start. Open and save the quest asset in the editor."),
					*ObjectiveTag.ToString());
			}
		}
		break;
	}
	}
}

bool UAQSQuestObjective::TryAddLateLoadedTarget(UAQSQuestTargetComponent* targetComp)
{
	if (!IsValid(targetComp) || questTargets.Contains(targetComp)) {
		return false;
	}

	bool bIsReferenced = false;
	switch (TargetRefType) {
	case ETargetReferenceType::ETag:
		bIsReferenced = targetComp->GetTargetTag().IsValid() && ReferencedTargets.Contains(targetComp->GetTargetTag());
		break;
	case ETargetReferenceType::ESoftRef: {
		const AActor* owner = targetComp->GetOwner();
		for (const TSoftObjectPtr<AActor>& ActorRef : ReferencedActors) {
			if (owner && ActorRef.Get() == owner) {
				bIsReferenced = true;
				break;
			}
		}
		break;
	}
	}

	if (!bIsReferenced) {
		return false;
	}

	questTargets.Add(targetComp);
	return true;
}

void UAQSQuestObjective::RefreshForcedTargets()
{
	for (const TSoftObjectPtr<AActor>& ActorRef : ReferencedActors) {
		AActor* Actor = ActorRef.Get();
		if (!IsValid(Actor) || !Actor->HasActorBegunPlay()) {
			continue;
		}

		UAQSQuestTargetComponent* TargetComp = Actor->FindComponentByClass<UAQSQuestTargetComponent>();
		if (IsValid(TargetComp) && TargetComp->HasBegunPlay()) {
			questTargets.AddUnique(TargetComp);
		}
	}

	if (TryDispatchObjectiveStarted() || HasDispatchedObjectiveStarted()) {
		if (UWorld* World = GetWorld()) {
			World->GetTimerManager().ClearTimer(ForcedTargetsRefreshTimer);
		}
		return;
	}

	++ForcedTargetsWaitTicks;
	/*Hard cap so a hung World Partition load can never stall a quest line forever.*/
	constexpr int32 MaxForcedTargetWaitTicks = 300;
	if (ForcedTargetsWaitTicks >= MaxForcedTargetWaitTicks) {
		UE_LOG(LogTemp, Error,
			TEXT("Quest objective '%s' World Partition load did not finish in time. Starting the objective anyway."),
			*ObjectiveTag.ToString());
		DispatchObjectiveStarted();
		if (UWorld* World = GetWorld()) {
			World->GetTimerManager().ClearTimer(ForcedTargetsRefreshTimer);
		}
		return;
	}

	if (ForcedTargetsWaitTicks % 100 == 0) {
		for (int32 Index = 0; Index < ReferencedActors.Num(); ++Index) {
			const AActor* Actor = ReferencedActors[Index].Get();
			if (!IsValid(Actor) && !IsTargetLoadable(Index)) {
				continue;
			}
			const bool bActorReady = IsValid(Actor) && Actor->HasActorBegunPlay()
				&& IsValid(Actor->FindComponentByClass<UAQSQuestTargetComponent>());
			if (!bActorReady) {
				UE_LOG(LogTemp, Warning,
					TEXT("Quest objective '%s' is still waiting for target '%s' to finish streaming in."),
					*ObjectiveTag.ToString(), *ReferencedActors[Index].ToSoftObjectPath().ToString());
			}
		}
	}
}

void UAQSQuestObjective::ReleaseForcedTargets()
{
	/*Prefer the cached world: this also runs from BeginDestroy, where going through
	GetWorld() would dereference a nodeOwner that may already be on its way out.*/
	UWorld* World = StreamingSourceWorld.IsValid() ? StreamingSourceWorld.Get() : GetWorld();
	if (World) {
		World->GetTimerManager().ClearTimer(ForcedTargetsRefreshTimer);
		if (bIsStreamingSourceRegistered) {
			if (UWorldPartitionSubsystem* WorldPartitionSubsystem = World->GetSubsystem<UWorldPartitionSubsystem>()) {
				WorldPartitionSubsystem->UnregisterStreamingSourceProvider(this);
			}
		}
	}
	bIsStreamingSourceRegistered = false;
	StreamingSourceWorld.Reset();
	ForcedStreamingTargetIndices.Reset();
	questTargets.Reset();
}

void UAQSQuestObjective::BeginDestroy()
{
	ReleaseForcedTargets();
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UAQSQuestObjective::PreSave(FObjectPreSaveContext SaveContext)
{
	RefreshReferencedActorLocations();
	Super::PreSave(SaveContext);
}

void UAQSQuestObjective::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FProperty* ChangedMember = PropertyChangedEvent.MemberProperty
		? PropertyChangedEvent.MemberProperty
		: PropertyChangedEvent.Property;
	if (!ChangedMember
		|| ChangedMember->GetFName() == GET_MEMBER_NAME_CHECKED(UAQSQuestObjective, ReferencedActors)) {
		RefreshReferencedActorLocations();
	}
}

void UAQSQuestObjective::RefreshReferencedActorLocations()
{
	const TArray<FVector> PreviousLocations = ReferencedActorStreamingLocations;
	const TArray<bool> PreviousResolved = ReferencedActorStreamingLocationsResolved;
	const bool bHadUsablePreviousCache = PreviousLocations.Num() == ReferencedActors.Num()
		&& PreviousResolved.Num() == PreviousLocations.Num();

	ReferencedActorStreamingLocations.SetNum(ReferencedActors.Num());
	ReferencedActorStreamingLocationsResolved.SetNum(ReferencedActors.Num());
	for (int32 Index = 0; Index < ReferencedActors.Num(); ++Index) {
		AActor* Actor = ReferencedActors[Index].LoadSynchronous();
		if (Actor) {
			ReferencedActorStreamingLocations[Index] = Actor->GetActorLocation();
			ReferencedActorStreamingLocationsResolved[Index] = true;
			continue;
		}

		/*Saving the quest without its map (or region) loaded must not wipe a cache that
		was already resolved by a previous save with the map open.*/
		if (bHadUsablePreviousCache && PreviousResolved[Index]) {
			ReferencedActorStreamingLocations[Index] = PreviousLocations[Index];
			ReferencedActorStreamingLocationsResolved[Index] = true;
			continue;
		}

		/*Leaving a stale value would register a streaming source on the wrong cell.*/
		ReferencedActorStreamingLocations[Index] = FVector::ZeroVector;
		ReferencedActorStreamingLocationsResolved[Index] = false;
		UE_LOG(LogTemp, Warning,
			TEXT("Quest objective '%s' could not resolve referenced target '%s': its location will not be streamed in at runtime."),
			*ObjectiveTag.ToString(), *ReferencedActors[Index].ToSoftObjectPath().ToString());
	}
}
#endif

void UAQSQuestObjective::OnObjectiveStarted_Implementation()
{
}

void UAQSQuestObjective::ClientOnObjectiveStarted_Implementation()
{
}

void UAQSQuestObjective::OnObjectiveCompleted_Implementation()
{
}

void UAQSQuestObjective::ClientOnObjectiveCompleted_Implementation()
{
}

void UAQSQuestObjective::OnObjectiveUpdated_Implementation()
{
}

void UAQSQuestObjective::OnObjectiveInterrupted_Implementation()
{
}

void UAQSQuestObjective::ClientOnObjectiveInterrupted_Implementation()
{
}

void UAQSQuestObjective::SetCurrentRepetitions(int32 currentRep)
{
	if (currentRep < GetRepetitions()) {
		nodeOwner->SetCurrentRepetitions(currentRep);
	}
}

UAQSQuestManagerComponent* UAQSQuestObjective::GetLocalQuestManager() const
{
	return GetLocalPlayerController() ? GetLocalPlayerController()->FindComponentByClass<UAQSQuestManagerComponent>() : nullptr;
}
