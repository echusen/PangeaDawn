#include "Components/MiningSitePresentationCoordinatorComponent.h"

#include "Actors/MiningSettlementStockpileActor.h"
#include "Actors/MiningSiteActor.h"
#include "Tasks/MiningSmartObjectInteractionTask.h"
#include "Components/MiningSiteComponent.h"
#include "DataAssets/MiningSitePresentationConfig.h"
#include "SmartObjectComponent.h"

void UMiningSitePresentationCoordinatorComponent::UpdatePresentationActorRoute(
	AMiningSiteActor& SiteActor,
	TArray<TObjectPtr<AActor>>& ActorArray,
	EMiningPresentationRole Role,
	const TArray<TObjectPtr<USmartObjectComponent>>& SmartObjectComponents,
	float PrimaryRadius,
	float PrimaryAngleOffsetDegrees,
	float SecondaryRadius,
	float SecondaryAngleOffsetDegrees)
{
	const int32 DesiredCount = ActorArray.Num();
	if (DesiredCount <= 0)
	{
		return;
	}

	const UMiningSitePresentationConfig* Config = GetPresentationConfig(SiteActor.MiningSiteComponent);
	if (Config && Config->bUseACFRoutineJobs)
	{
		for (int32 Index = 0; Index < ActorArray.Num(); ++Index)
		{
			FVector Primary, Secondary;
			if (!IsValid(ActorArray[Index])) continue;
			if (!TryGetConfiguredStationTargets(SiteActor, SiteActor.MiningSiteComponent, Role, Index, Primary, Secondary))
			{
				ReleaseRoutineAssignment(ActorArray[Index]);
				EmitValidationWarning(SiteActor, TEXT("ACF duty station markers missing; assignment stopped."));
				continue;
			}
			ConfigureRoutineJob(SiteActor, ActorArray[Index], Role, SmartObjectComponents.IsValidIndex(Index) ? SmartObjectComponents[Index].Get() : nullptr, Primary, Secondary);
		}
		return; // Sole movement owner is now the per-controller ACF duty, including failures.
	}
	const bool bUseFullFidelity = IsFullFidelityPresentationRelevant(SiteActor);
	for (AActor* Actor : ActorArray) ReleaseRoutineAssignment(Actor);
	const float AngleStep = DesiredCount > 1 ? 40.0f : 0.0f;
	for (int32 Index = 0; Index < DesiredCount; ++Index)
	{
		AActor* Actor = ActorArray[Index];
		if (Actor && bUseFullFidelity && HasSmartObjectInteractionTimedOut(Actor, GetInteractionDurationForActor(SiteActor.MiningSiteComponent, Role, Actor)))
		{
			ReleaseSmartObjectForActor(Actor);
			PresentationRoutePhaseMap.Add(Actor, !PresentationRoutePhaseMap.FindRef(Actor));
		}
	}

	for (int32 Index = 0; Index < DesiredCount; ++Index)
	{
		AActor* Actor = ActorArray[Index];
		if (!Actor)
		{
			continue;
		}

		FVector PrimaryTarget = GetPresentationSlotLocation(SiteActor, PrimaryRadius, PrimaryAngleOffsetDegrees + (Index * AngleStep));
		FVector SecondaryTarget = GetPresentationSlotLocation(SiteActor, SecondaryRadius, SecondaryAngleOffsetDegrees + (Index * 6.0f));
		if (!TryGetConfiguredStationTargets(SiteActor, SiteActor.MiningSiteComponent, Role, Index, PrimaryTarget, SecondaryTarget))
		{
			EmitValidationWarning(SiteActor, FString::Printf(TEXT("%s station %d is not configured correctly."), Role == EMiningPresentationRole::Worker ? TEXT("Worker") : TEXT("Guard"), Index));
			continue;
		}

		USmartObjectComponent* SmartObjectComponent = SmartObjectComponents.IsValidIndex(Index) ? SmartObjectComponents[Index].Get() : nullptr;
		const bool bMovingToSecondary = PresentationRoutePhaseMap.FindRef(Actor);
		const FVector CurrentTarget = bMovingToSecondary ? SecondaryTarget : PrimaryTarget;
		const TObjectPtr<UMiningSmartObjectInteractionTask>* ExistingTask = PresentationSmartObjectTaskMap.Find(Actor);
		const bool bHasActiveInteraction = ExistingTask && ExistingTask->Get() && !ExistingTask->Get()->IsFinished();
		const bool bHoldActive = HasPresentationHoldActive(Actor);
		const bool bHasStartedTargetHold = PresentationTargetHoldStartedMap.FindRef(Actor);
		const FVector InteractionLookAt = bMovingToSecondary ? PrimaryTarget : SecondaryTarget;
		const EMiningPresentationState OccupiedState =
			Role == EMiningPresentationRole::Worker
				? (bMovingToSecondary ? EMiningPresentationState::Depositing : EMiningPresentationState::Working)
				: GetActiveStateForRole(Role);
		const bool bAtCurrentTarget = FVector::DistSquared2D(Actor->GetActorLocation(), CurrentTarget) <= FMath::Square(90.0f);

		if (!bUseFullFidelity)
		{
			ReleaseSmartObjectForActor(Actor);

			if (bAtCurrentTarget)
			{
				if (!bHasStartedTargetHold)
				{
					BeginPresentationHold(Actor, GetInteractionDurationForActor(SiteActor.MiningSiteComponent, Role, Actor));
					PresentationTargetHoldStartedMap.Add(Actor, true);
				}

				UpdatePresentationFacing(Actor, InteractionLookAt);
				ApplyAgentPresentation(Actor, Role, EMiningPresentationState::Idle, &InteractionLookAt);

				if (!bHoldActive && bHasStartedTargetHold)
				{
					PresentationRoutePhaseMap.Add(Actor, !bMovingToSecondary);
					PresentationTargetHoldStartedMap.Add(Actor, false);
				}
				continue;
			}

			PresentationTargetHoldStartedMap.Add(Actor, false);
			TryMovePresentationActor(SiteActor, Actor, CurrentTarget, Role, false);
			continue;
		}

		if (!bHasActiveInteraction && !bHoldActive && bAtCurrentTarget)
		{
			PresentationRoutePhaseMap.Add(Actor, !bMovingToSecondary);
		}

		if (bHoldActive)
		{
			UpdatePresentationFacing(Actor, InteractionLookAt);
			ApplyAgentPresentation(Actor, Role, OccupiedState, &InteractionLookAt);
			continue;
		}

		if (TryStartSmartObjectBehavior(SiteActor, Actor, SmartObjectComponent, Role))
		{
			const bool bAtInteractionSlot = FVector::DistSquared2D(Actor->GetActorLocation(), CurrentTarget) <= FMath::Square(60.0f);

			if (bAtInteractionSlot)
			{
				UpdatePresentationFacing(Actor, InteractionLookAt);
				ApplyAgentPresentation(Actor, Role, OccupiedState, &InteractionLookAt);
			}
			else
			{
				ApplyAgentPresentation(Actor, Role, EMiningPresentationState::Traveling, &CurrentTarget);
			}
			continue;
		}

		ReleaseSmartObjectForActor(Actor);
		if (bAtCurrentTarget)
		{
			if (!bHasStartedTargetHold)
			{
				BeginPresentationHold(Actor, GetInteractionDurationForActor(SiteActor.MiningSiteComponent, Role, Actor));
				PresentationTargetHoldStartedMap.Add(Actor, true);
			}

			UpdatePresentationFacing(Actor, InteractionLookAt);
			ApplyAgentPresentation(Actor, Role, OccupiedState, &InteractionLookAt);

			if (!bHoldActive && bHasStartedTargetHold)
			{
				PresentationRoutePhaseMap.Add(Actor, !bMovingToSecondary);
				PresentationTargetHoldStartedMap.Add(Actor, false);
			}
			continue;
		}

		PresentationTargetHoldStartedMap.Add(Actor, false);
		TryMovePresentationActor(SiteActor, Actor, PresentationRoutePhaseMap.FindRef(Actor) ? SecondaryTarget : PrimaryTarget, Role);
	}
}

void UMiningSitePresentationCoordinatorComponent::UpdateCourierPresentationRoute(
	AMiningSiteActor& SiteActor,
	TObjectPtr<AActor>& SpawnedCourierActor,
	float PrimaryRadius,
	float PrimaryAngleOffsetDegrees,
	float SecondaryRadius,
	float SecondaryAngleOffsetDegrees)
{
	if (!SpawnedCourierActor)
	{
		return;
	}

	FVector PrimaryTarget = GetPresentationSlotLocation(SiteActor, PrimaryRadius, PrimaryAngleOffsetDegrees);
	FVector SecondaryTarget = GetPresentationSlotLocation(SiteActor, SecondaryRadius, SecondaryAngleOffsetDegrees);
	const FMiningPresentationRoleConfig* CourierConfig = GetRoleConfig(SiteActor.MiningSiteComponent, EMiningPresentationRole::Courier);
	if (!CourierConfig || !CourierConfig->Stations.IsValidIndex(0))
	{
		ReleaseRoutineAssignment(SpawnedCourierActor);
		EmitValidationWarning(SiteActor, TEXT("Courier station is not configured correctly."));
		return;
	}

	const FMiningPresentationStation& CourierStation = CourierConfig->Stations[0];
	if (CourierStation.PrimaryMarkerName.IsNone() || !TryGetRouteMarkerLocation(SiteActor, CourierStation.PrimaryMarkerName.ToString(), PrimaryTarget))
	{
		ReleaseRoutineAssignment(SpawnedCourierActor);
		EmitValidationWarning(SiteActor, TEXT("Courier primary station marker is missing or invalid."));
		return;
	}

	if (AMiningSettlementStockpileActor* StockpileActor = Cast<AMiningSettlementStockpileActor>(SiteActor.SettlementResourceActor))
	{
		SecondaryTarget = StockpileActor->GetCourierUnloadLocation();
	}
	else if (CourierStation.SecondaryMarkerName.IsNone() || !TryGetRouteMarkerLocation(SiteActor, CourierStation.SecondaryMarkerName.ToString(), SecondaryTarget))
	{
		ReleaseRoutineAssignment(SpawnedCourierActor);
		EmitValidationWarning(SiteActor, TEXT("Courier secondary station marker is missing or invalid."));
		return;
	}

	const UMiningSitePresentationConfig* Config = GetPresentationConfig(SiteActor.MiningSiteComponent);
	if (Config && Config->bUseACFRoutineJobs)
	{
		ConfigureRoutineJob(SiteActor, SpawnedCourierActor, EMiningPresentationRole::Courier, nullptr, PrimaryTarget, SecondaryTarget);
		return;
	}
	const bool bMovingToSecondary = PresentationRoutePhaseMap.FindRef(SpawnedCourierActor);
	ReleaseRoutineAssignment(SpawnedCourierActor);
	const FVector CurrentTarget = bMovingToSecondary ? SecondaryTarget : PrimaryTarget;
	const bool bHoldActive = HasPresentationHoldActive(SpawnedCourierActor);
	const bool bHasStartedTargetHold = PresentationTargetHoldStartedMap.FindRef(SpawnedCourierActor);
	const FVector InteractionLookAt = bMovingToSecondary ? PrimaryTarget : SecondaryTarget;
	const EMiningPresentationState OccupiedState = bMovingToSecondary ? EMiningPresentationState::Unloading : EMiningPresentationState::Loading;
	const bool bAtCurrentTarget = FVector::DistSquared(SpawnedCourierActor->GetActorLocation(), CurrentTarget) <= FMath::Square(110.0f);

	if (bHoldActive)
	{
		UpdatePresentationFacing(SpawnedCourierActor, InteractionLookAt);
		ApplyAgentPresentation(SpawnedCourierActor, EMiningPresentationRole::Courier, OccupiedState, &InteractionLookAt);
		return;
	}

	if (bAtCurrentTarget && bHasStartedTargetHold)
	{
		PresentationRoutePhaseMap.Add(SpawnedCourierActor, !bMovingToSecondary);
		PresentationTargetHoldStartedMap.Add(SpawnedCourierActor, false);
		return;
	}

	if (bAtCurrentTarget)
	{
		if (!bHasStartedTargetHold)
		{
			BeginPresentationHold(SpawnedCourierActor, GetInteractionDurationForActor(SiteActor.MiningSiteComponent, EMiningPresentationRole::Courier, SpawnedCourierActor));
			PresentationTargetHoldStartedMap.Add(SpawnedCourierActor, true);
		}

		UpdatePresentationFacing(SpawnedCourierActor, InteractionLookAt);
		ApplyAgentPresentation(SpawnedCourierActor, EMiningPresentationRole::Courier, OccupiedState, &InteractionLookAt);

		if (!bHoldActive && bHasStartedTargetHold)
		{
			PresentationRoutePhaseMap.Add(SpawnedCourierActor, !bMovingToSecondary);
			PresentationTargetHoldStartedMap.Add(SpawnedCourierActor, false);
		}
		return;
	}

	PresentationTargetHoldStartedMap.Add(SpawnedCourierActor, false);
	ReleaseSmartObjectForActor(SpawnedCourierActor);
	TryMovePresentationActor(SiteActor, SpawnedCourierActor, CurrentTarget, EMiningPresentationRole::Courier, true);
}
