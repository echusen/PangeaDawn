#include "Components/MiningSitePresentationCoordinatorComponent.h"

#include "Actors/MiningSiteActor.h"
#include "Components/MiningSiteComponent.h"
#include "DataAssets/MiningSitePresentationConfig.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningPresentationCoordinator, Log, All);

UMiningSitePresentationCoordinatorComponent::UMiningSitePresentationCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMiningSitePresentationCoordinatorComponent::RefreshPresentationActors(
	AMiningSiteActor& SiteActor,
	UMiningSiteComponent* MiningSiteComponent,
	AActor* SpawnedSiteChest,
	TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
	TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
	TObjectPtr<AActor>& SpawnedCourierActor)
{
	if (!SiteActor.HasAuthority() || !MiningSiteComponent || !MiningSiteComponent->IsEstablished())
	{
		ClearPresentationActors(SpawnedWorkerActors, SpawnedGuardActors, SpawnedCourierActor);
		return;
	}

	FMiningSiteLevelDefinition LevelDefinition;
	if (!MiningSiteComponent->GetCurrentLevelDefinition(LevelDefinition))
	{
		ClearPresentationActors(SpawnedWorkerActors, SpawnedGuardActors, SpawnedCourierActor);
		return;
	}

	const UMiningSitePresentationConfig* PresentationConfig = GetPresentationConfig(MiningSiteComponent);
	if (!PresentationConfig)
	{
		EmitValidationWarning(SiteActor, TEXT("Missing presentation config asset on site definition. Presentation actors will not spawn."));
		ClearPresentationActors(SpawnedWorkerActors, SpawnedGuardActors, SpawnedCourierActor);
		return;
	}

	const bool bPresentationRelevant = IsPresentationRelevant(SiteActor);
	const int32 DesiredWorkerCount = bPresentationRelevant ? FMath::Max(0, LevelDefinition.WorkerCount) : 0;
	const int32 DesiredGuardCount = bPresentationRelevant ? FMath::Max(0, LevelDefinition.GuardCount) : 0;
	const bool bShouldShowCourier = bPresentationRelevant && LevelDefinition.bShipmentUnlocked && !LevelDefinition.CourierClass.IsNull();
	ValidatePresentationSetup(SiteActor, MiningSiteComponent, DesiredWorkerCount, DesiredGuardCount, bShouldShowCourier);

	const FMiningPresentationRoleConfig* WorkerConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Worker);
	const FMiningPresentationRoleConfig* GuardConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Guard);
	const FMiningPresentationRoleConfig* CourierConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Courier);

	SyncPresentationActorsForRole(
		SiteActor,
		SpawnedWorkerActors,
		WorkerConfig && !WorkerConfig->ActorClassOverride.IsNull() ? WorkerConfig->ActorClassOverride : LevelDefinition.WorkerClass,
		DesiredWorkerCount,
		260.0f,
		-30.0f,
		EMiningPresentationRole::Worker);
	SyncPresentationActorsForRole(
		SiteActor,
		SpawnedGuardActors,
		GuardConfig && !GuardConfig->ActorClassOverride.IsNull() ? GuardConfig->ActorClassOverride : LevelDefinition.GuardClass,
		DesiredGuardCount,
		340.0f,
		160.0f,
		EMiningPresentationRole::Guard);
	SyncCourierPresentationActor(
		SiteActor,
		SpawnedCourierActor,
		CourierConfig && !CourierConfig->ActorClassOverride.IsNull() ? CourierConfig->ActorClassOverride : LevelDefinition.CourierClass,
		bShouldShowCourier,
		420.0f,
		-140.0f);
}

void UMiningSitePresentationCoordinatorComponent::ClearPresentationActors(
	TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
	TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
	TObjectPtr<AActor>& SpawnedCourierActor)
{
	for (AActor* Actor : SpawnedWorkerActors)
	{
		ReleaseSmartObjectForActor(Actor);
		if (Actor)
		{
			ApplyAgentPresentation(Actor, EMiningPresentationRole::Worker, EMiningPresentationState::Idle);
			Actor->Destroy();
		}
	}

	for (AActor* Actor : SpawnedGuardActors)
	{
		ReleaseSmartObjectForActor(Actor);
		if (Actor)
		{
			ApplyAgentPresentation(Actor, EMiningPresentationRole::Guard, EMiningPresentationState::Idle);
			Actor->Destroy();
		}
	}

	SpawnedWorkerActors.Reset();
	SpawnedGuardActors.Reset();

	if (SpawnedCourierActor)
	{
		ReleaseSmartObjectForActor(SpawnedCourierActor);
		ApplyAgentPresentation(SpawnedCourierActor, EMiningPresentationRole::Courier, EMiningPresentationState::Idle);
		SpawnedCourierActor->Destroy();
		SpawnedCourierActor = nullptr;
	}

	PresentationRoutePhaseMap.Reset();
	PresentationMoveTargetMap.Reset();
	PresentationSmartObjectLastStatusMap.Reset();
	PresentationSmartObjectStartTimeMap.Reset();
	PresentationSmartObjectTaskMap.Reset();
	PresentationClaimHandleMap.Reset();
	PresentationHoldUntilTimeMap.Reset();
	PresentationTargetHoldStartedMap.Reset();
	bPresentationRelevantCached = false;
	bFullFidelityRelevantCached = false;
}

void UMiningSitePresentationCoordinatorComponent::ConfigureSmartObjectComponents(
	AMiningSiteActor& SiteActor,
	UMiningSiteComponent* MiningSiteComponent,
	TArray<TObjectPtr<USmartObjectComponent>>& WorkerSmartObjectComponents,
	TArray<TObjectPtr<USmartObjectComponent>>& GuardSmartObjectComponents) const
{
	if (!MiningSiteComponent || !MiningSiteComponent->SiteDefinition)
	{
		return;
	}

	const UMiningSitePresentationConfig* PresentationConfig = GetPresentationConfig(MiningSiteComponent);
	if (!PresentationConfig)
	{
		return;
	}

	auto ConfigureComponent = [&SiteActor, this](USmartObjectComponent* SmartObjectComponent, USmartObjectDefinition* Definition, const FString& MarkerName, bool bShouldEnable)
	{
		if (!SmartObjectComponent)
		{
			return;
		}

		USmartObjectSubsystem* SmartObjectSubsystem = USmartObjectSubsystem::GetCurrent(SiteActor.GetWorld());
		const bool bWasBoundToSimulation = SmartObjectComponent->IsBoundToSimulation();
		if (bWasBoundToSimulation && SmartObjectSubsystem)
		{
			SmartObjectSubsystem->UnregisterSmartObject(SmartObjectComponent);
		}

		if (Definition && SmartObjectComponent->GetDefinition() != Definition)
		{
			SmartObjectComponent->SetDefinition(Definition);
		}

		FVector MarkerLocation;
		if (TryGetRouteMarkerLocation(SiteActor, MarkerName, MarkerLocation))
		{
			SmartObjectComponent->SetWorldLocation(MarkerLocation);
			SmartObjectComponent->SetWorldRotation(SiteActor.GetActorRotation());
		}

		if (SmartObjectSubsystem && SmartObjectComponent->GetDefinition())
		{
			SmartObjectSubsystem->RegisterSmartObject(SmartObjectComponent);
		}

		SmartObjectComponent->K2_SetSmartObjectEnabled(bShouldEnable);
	};

	auto EnsureIndexedComponents =
		[&SiteActor](TArray<TObjectPtr<USmartObjectComponent>>& SmartObjectComponents, const TCHAR* ComponentPrefix, int32 RequiredCount)
	{
		while (SmartObjectComponents.Num() < RequiredCount)
		{
			const int32 ComponentIndex = SmartObjectComponents.Num();
			USmartObjectComponent* NewComponent =
				NewObject<USmartObjectComponent>(&SiteActor, *FString::Printf(TEXT("%s_%d"), ComponentPrefix, ComponentIndex + 1));
			if (!NewComponent)
			{
				break;
			}

			NewComponent->SetupAttachment(SiteActor.SceneRoot);
			NewComponent->RegisterComponent();
			SmartObjectComponents.Add(NewComponent);
		}
	};

	FMiningSiteLevelDefinition LevelDefinition;
	const bool bHasLevelDefinition = MiningSiteComponent->GetCurrentLevelDefinition(LevelDefinition);
	const FMiningPresentationRoleConfig* WorkerConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Worker);
	const FMiningPresentationRoleConfig* GuardConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Guard);
	const int32 WorkerMarkerCount = WorkerConfig ? WorkerConfig->Stations.Num() : 0;
	const int32 GuardMarkerCount = GuardConfig ? GuardConfig->Stations.Num() : 0;

	USmartObjectDefinition* WorkerDefinition = WorkerConfig && !WorkerConfig->SmartObjectDefinition.IsNull()
		? WorkerConfig->SmartObjectDefinition.LoadSynchronous()
		: nullptr;
	USmartObjectDefinition* GuardDefinition = GuardConfig && !GuardConfig->SmartObjectDefinition.IsNull()
		? GuardConfig->SmartObjectDefinition.LoadSynchronous()
		: nullptr;
	EnsureIndexedComponents(WorkerSmartObjectComponents, TEXT("WorkerSmartObjectComponent"), WorkerMarkerCount);
	EnsureIndexedComponents(GuardSmartObjectComponents, TEXT("GuardSmartObjectComponent"), GuardMarkerCount);

	for (int32 Index = 0; Index < WorkerSmartObjectComponents.Num(); ++Index)
	{
		if (!WorkerConfig || !WorkerConfig->Stations.IsValidIndex(Index) || !WorkerDefinition)
		{
			if (WorkerSmartObjectComponents[Index])
			{
				WorkerSmartObjectComponents[Index]->K2_SetSmartObjectEnabled(false);
			}
			continue;
		}

		const FString MarkerName = WorkerConfig->Stations[Index].PrimaryMarkerName.ToString();
		ConfigureComponent(
			WorkerSmartObjectComponents[Index],
			WorkerDefinition,
			MarkerName,
			bHasLevelDefinition && Index < LevelDefinition.WorkerCount);
	}

	for (int32 Index = 0; Index < GuardSmartObjectComponents.Num(); ++Index)
	{
		if (!GuardConfig || !GuardConfig->Stations.IsValidIndex(Index) || !GuardDefinition)
		{
			if (GuardSmartObjectComponents[Index])
			{
				GuardSmartObjectComponents[Index]->K2_SetSmartObjectEnabled(false);
			}
			continue;
		}

		const FString MarkerName = GuardConfig->Stations[Index].PrimaryMarkerName.ToString();
		ConfigureComponent(
			GuardSmartObjectComponents[Index],
			GuardDefinition,
			MarkerName,
			bHasLevelDefinition && Index < LevelDefinition.GuardCount);
	}
}

void UMiningSitePresentationCoordinatorComponent::UpdatePresentationActorMovement(
	AMiningSiteActor& SiteActor,
	UMiningSiteComponent* MiningSiteComponent,
	AActor* SpawnedSiteChest,
	TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
	TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
	TObjectPtr<AActor>& SpawnedCourierActor,
	const TArray<TObjectPtr<USmartObjectComponent>>& WorkerSmartObjectComponents,
	const TArray<TObjectPtr<USmartObjectComponent>>& GuardSmartObjectComponents)
{
	if (!SiteActor.HasAuthority() || !MiningSiteComponent || !MiningSiteComponent->IsEstablished())
	{
		return;
	}

	if (!GetPresentationConfig(MiningSiteComponent))
	{
		return;
	}

	const FVector ChestLocation = SpawnedSiteChest ? SpawnedSiteChest->GetActorLocation() : GetPresentationSlotLocation(SiteActor, 180.0f, -110.0f);
	const FVector ChestOffset = SiteActor.GetTransform().InverseTransformPosition(ChestLocation);

	UpdatePresentationActorRoute(SiteActor, SpawnedWorkerActors, EMiningPresentationRole::Worker, WorkerSmartObjectComponents, 260.0f, -30.0f, ChestOffset.Size(), FMath::RadiansToDegrees(FMath::Atan2(ChestOffset.Y, ChestOffset.X)));
	UpdatePresentationActorRoute(SiteActor, SpawnedGuardActors, EMiningPresentationRole::Guard, GuardSmartObjectComponents, 340.0f, 160.0f, 430.0f, 205.0f);
	UpdateCourierPresentationRoute(SiteActor, SpawnedCourierActor, 420.0f, -140.0f, 760.0f, -150.0f);
}
