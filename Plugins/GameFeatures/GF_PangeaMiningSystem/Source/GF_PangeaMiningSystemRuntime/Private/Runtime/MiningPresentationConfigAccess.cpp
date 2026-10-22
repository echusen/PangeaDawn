#include "Components/MiningSitePresentationCoordinatorComponent.h"

#include "Actors/MiningSiteActor.h"
#include "Components/ActorComponent.h"
#include "Components/MiningSiteComponent.h"
#include "DataAssets/MiningSiteDefinition.h"
#include "DataAssets/MiningSitePresentationConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningPresentationConfigAccess, Log, All);

const UMiningSitePresentationConfig* UMiningSitePresentationCoordinatorComponent::GetPresentationConfig(const UMiningSiteComponent* MiningSiteComponent) const
{
	if (!MiningSiteComponent || !MiningSiteComponent->SiteDefinition || MiningSiteComponent->SiteDefinition->PresentationConfig.IsNull())
	{
		return nullptr;
	}

	return Cast<UMiningSitePresentationConfig>(MiningSiteComponent->SiteDefinition->PresentationConfig.LoadSynchronous());
}

const FMiningPresentationRoleConfig* UMiningSitePresentationCoordinatorComponent::GetRoleConfig(const UMiningSiteComponent* MiningSiteComponent, const EMiningPresentationRole Role) const
{
	const UMiningSitePresentationConfig* PresentationConfig = GetPresentationConfig(MiningSiteComponent);
	return PresentationConfig ? PresentationConfig->FindRoleConfig(Role) : nullptr;
}

bool UMiningSitePresentationCoordinatorComponent::TryGetConfiguredStationTargets(
	const AMiningSiteActor& SiteActor,
	const UMiningSiteComponent* MiningSiteComponent,
	const EMiningPresentationRole Role,
	const int32 StationIndex,
	FVector& OutPrimaryTarget,
	FVector& OutSecondaryTarget) const
{
	const FMiningPresentationRoleConfig* RoleConfig = GetRoleConfig(MiningSiteComponent, Role);
	if (!RoleConfig || !RoleConfig->Stations.IsValidIndex(StationIndex))
	{
		return false;
	}

	const FMiningPresentationStation& Station = RoleConfig->Stations[StationIndex];
	const bool bFoundPrimary = !Station.PrimaryMarkerName.IsNone() && TryGetRouteMarkerLocation(SiteActor, Station.PrimaryMarkerName.ToString(), OutPrimaryTarget);
	const bool bFoundSecondary = !Station.SecondaryMarkerName.IsNone() && TryGetRouteMarkerLocation(SiteActor, Station.SecondaryMarkerName.ToString(), OutSecondaryTarget);
	return bFoundPrimary && bFoundSecondary;
}

void UMiningSitePresentationCoordinatorComponent::EmitValidationWarning(const AMiningSiteActor& SiteActor, const FString& Message) const
{
	const FString Key = FString::Printf(TEXT("%s::%s"), *SiteActor.GetName(), *Message);
	if (ValidationWarningsIssued.Contains(Key))
	{
		return;
	}

	ValidationWarningsIssued.Add(Key);
	UE_LOG(LogPangeaMiningPresentationConfigAccess, Warning, TEXT("Mining presentation validation. Site=%s %s"), *GetNameSafe(&SiteActor), *Message);
}

void UMiningSitePresentationCoordinatorComponent::ValidatePresentationSetup(
	const AMiningSiteActor& SiteActor,
	const UMiningSiteComponent* MiningSiteComponent,
	const int32 WorkerCount,
	const int32 GuardCount,
	const bool bNeedsCourier) const
{
	const FMiningPresentationRoleConfig* WorkerConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Worker);
	const FMiningPresentationRoleConfig* GuardConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Guard);
	const FMiningPresentationRoleConfig* CourierConfig = GetRoleConfig(MiningSiteComponent, EMiningPresentationRole::Courier);

	if (!WorkerConfig)
	{
		EmitValidationWarning(SiteActor, TEXT("Missing presentation config for workers."));
	}
	else if (WorkerCount > WorkerConfig->Stations.Num())
	{
		EmitValidationWarning(SiteActor, FString::Printf(TEXT("Worker station count mismatch. Needed=%d Configured=%d"), WorkerCount, WorkerConfig->Stations.Num()));
	}

	if (!GuardConfig)
	{
		EmitValidationWarning(SiteActor, TEXT("Missing presentation config for guards."));
	}
	else if (GuardCount > GuardConfig->Stations.Num())
	{
		EmitValidationWarning(SiteActor, FString::Printf(TEXT("Guard station count mismatch. Needed=%d Configured=%d"), GuardCount, GuardConfig->Stations.Num()));
	}

	if (bNeedsCourier)
	{
		if (!CourierConfig)
		{
			EmitValidationWarning(SiteActor, TEXT("Missing presentation config for courier."));
		}
		else if (CourierConfig->Stations.Num() < 1)
		{
			EmitValidationWarning(SiteActor, TEXT("Courier presentation config has no configured stations."));
		}
		else if (CourierConfig->Stations[0].PrimaryMarkerName.IsNone())
		{
			EmitValidationWarning(SiteActor, TEXT("Courier presentation config is missing the primary marker."));
		}
		else if (!SiteActor.SettlementResourceActor && CourierConfig->Stations[0].SecondaryMarkerName.IsNone())
		{
			EmitValidationWarning(SiteActor, TEXT("Courier presentation config is missing the secondary marker and no settlement unload actor is assigned."));
		}
	}
}

EMiningPresentationState UMiningSitePresentationCoordinatorComponent::GetActiveStateForRole(EMiningPresentationRole Role) const
{
	switch (Role)
	{
	case EMiningPresentationRole::Worker:
		return EMiningPresentationState::Working;
	case EMiningPresentationRole::Guard:
		return EMiningPresentationState::Guarding;
	case EMiningPresentationRole::Courier:
		return EMiningPresentationState::Hauling;
	default:
		return EMiningPresentationState::Idle;
	}
}

void UMiningSitePresentationCoordinatorComponent::ApplyAgentPresentation(AActor* Actor, EMiningPresentationRole Role, EMiningPresentationState State, const FVector* FocusLocation) const
{
	if (!Actor || !Actor->HasAuthority())
	{
		return;
	}

	EnsurePresentationAgentComponent(Actor);

	if (Actor->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
	{
		IMiningPresentationAgentInterface::Execute_SetMiningPresentationRole(Actor, Role);
		IMiningPresentationAgentInterface::Execute_SetMiningPresentationState(Actor, State);
		if (FocusLocation)
		{
			IMiningPresentationAgentInterface::Execute_SetMiningPresentationFocus(Actor, *FocusLocation);
		}
		return;
	}

	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* Component : Components)
	{
		if (!Component || !Component->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
		{
			continue;
		}

		IMiningPresentationAgentInterface::Execute_SetMiningPresentationRole(Component, Role);
		IMiningPresentationAgentInterface::Execute_SetMiningPresentationState(Component, State);
		if (FocusLocation)
		{
			IMiningPresentationAgentInterface::Execute_SetMiningPresentationFocus(Component, *FocusLocation);
		}
		return;
	}
}

void UMiningSitePresentationCoordinatorComponent::EnsurePresentationAgentComponent(AActor* Actor) const
{
	if (!Actor || !Actor->HasAuthority())
	{
		return;
	}

	if (Actor->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
	{
		return;
	}

	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
		{
			return;
		}
	}

	static const TCHAR* AgentComponentClassPath = TEXT("/Script/MiningSystemPresentation.MiningPresentationAgentComponent");
	UClass* AgentComponentClass = LoadClass<UActorComponent>(nullptr, AgentComponentClassPath);
	if (!AgentComponentClass)
	{
		UE_LOG(LogPangeaMiningPresentationConfigAccess, Warning, TEXT("Mining presentation agent component class could not be loaded: %s"), AgentComponentClassPath);
		return;
	}

	UActorComponent* NewComponent = Actor->AddComponentByClass(AgentComponentClass, false, FTransform::Identity, false);
	if (NewComponent)
	{
		NewComponent->SetIsReplicated(true);
		NewComponent->RegisterComponent();
	}
}
