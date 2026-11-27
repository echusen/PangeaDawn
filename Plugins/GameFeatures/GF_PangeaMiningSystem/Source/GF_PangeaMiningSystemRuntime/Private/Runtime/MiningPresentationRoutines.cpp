#include "Components/MiningSitePresentationCoordinatorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"
#include "Actors/MiningSiteActor.h"
#include "Components/ACFAIRoutineComponent.h"
#include "Components/MiningRoutineJobComponent.h"
#include "Data/ACFAIRoutineDataAsset.h"

void UMiningSitePresentationCoordinatorComponent::ApplyRoutineTravelSpeed(AActor* Actor, EMiningPresentationRole Role, float Speed) const
{
	if (!IsValid(Actor) || !Actor->HasAuthority() || !FMath::IsFinite(Speed) || Speed <= 0.f)
	{
		return;
	}

	EnsurePresentationAgentComponent(Actor);

	if (Actor->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
	{
		IMiningPresentationAgentInterface::Execute_SetMiningTravelSpeed(Actor, Role, Speed);
		return;
	}

	TInlineComponentArray<UActorComponent*> Components(Actor);

	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
		{
			IMiningPresentationAgentInterface::Execute_SetMiningTravelSpeed(Component, Role, Speed);
			return;
		}
	}
}

void UMiningSitePresentationCoordinatorComponent::ApplyRoutinePresentation(
	AActor* Actor, EMiningPresentationRole Role, EMiningPresentationState State, const FVector& Focus) const
{
	ApplyAgentPresentation(Actor, Role, State, &Focus);
}

float UMiningSitePresentationCoordinatorComponent::GetRoutinePresentationDuration(AActor* Actor, EMiningPresentationState State, float Fallback) const
{
	if (!IsValid(Actor))
	{
		return Fallback;
	}

	auto ReadDuration = [State, Fallback](UObject* Agent)
	{
		const float Duration = IMiningPresentationAgentInterface::Execute_GetMiningPresentationDuration(Agent, State);
		return FMath::IsFinite(Duration) && Duration > 0.f ? Duration : Fallback;
	};

	if (Actor->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
	{
		return ReadDuration(Actor);
	}

	TInlineComponentArray<UActorComponent*> Components(Actor);

	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->GetClass()->ImplementsInterface(UMiningPresentationAgentInterface::StaticClass()))
		{
			return ReadDuration(Component);
		}
	}

	return Fallback;
}

AActor* UMiningSitePresentationCoordinatorComponent::SpawnConfiguredPresentationActor(AMiningSiteActor& SiteActor, UClass* ActorClass, const FVector& Location)
{
	const UMiningSitePresentationConfig* const Config = GetPresentationConfig(SiteActor.MiningSiteComponent);
	UClass* ControllerClass = nullptr;

	if (Config != nullptr && Config->bUseACFRoutineJobs)
	{
		ControllerClass = Config->RoutineControllerClass.LoadSynchronous();
		const AACFAIController* const Defaults = ControllerClass != nullptr ? Cast<AACFAIController>(ControllerClass->GetDefaultObject()) : nullptr;
		if (!ActorClass->IsChildOf(AACFCharacter::StaticClass()) || Defaults == nullptr || !Defaults->GetBehaviorTree()
			|| !Defaults->GetDefaultAIState().MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("AIState.Routine"))))
		{
			EmitValidationWarning(SiteActor,
				TEXT(
					"ACF routine jobs require a pawn and an ACF controller with BehaviorTree and AIState.Routine default. No legacy movement fallback will be used."));
			return nullptr;
		}
	}

	FActorSpawnParameters Params;
	Params.Owner = &SiteActor;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Params.bDeferConstruction = ControllerClass != nullptr;
	AActor* const Actor = SiteActor.GetWorld()->SpawnActor<AActor>(ActorClass, Location, SiteActor.GetActorRotation(), Params);
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (APawn* Pawn = Cast<APawn>(Actor); Pawn != nullptr && ControllerClass != nullptr)
	{
		Pawn->AIControllerClass = ControllerClass;
		Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
	}

	if (ControllerClass != nullptr)
	{
		Actor->FinishSpawning(FTransform(SiteActor.GetActorRotation(), Location));
	}

	if (ControllerClass != nullptr)
	{
		APawn* const Pawn = CastChecked<APawn>(Actor);

		// The reusable controller inherits a sample schedule. Create a fresh controller
		// and replace that schedule on THIS INSTANCE before possession starts its BT.
		// Never edit the sample or let its shared mutable tasks execute for mining NPCs.
		if (Pawn->GetController() || Pawn->AIControllerClass.Get() != ControllerClass)
		{
			EmitValidationWarning(SiteActor,
				TEXT(
					"Spawned pawn did not retain the configured ACF routine controller/component. Check construction and character-definition controller overrides."));
			Actor->Destroy();

			return nullptr;
		}

		FActorSpawnParameters ControllerParams;

		// Possession sets Pawn.Owner to the controller. Owning the controller by
		// that pawn would create a cycle (including recursive network ownership queries).
		ControllerParams.Owner = &SiteActor;
		ControllerParams.ObjectFlags |= RF_Transient;
		AACFAIController* const Controller =
			SiteActor.GetWorld()->SpawnActor<AACFAIController>(ControllerClass, Location, SiteActor.GetActorRotation(), ControllerParams);
		UACFAIRoutineComponent* const Routine = Controller != nullptr ? Controller->FindComponentByClass<UACFAIRoutineComponent>() : nullptr;
		if (Routine == nullptr)
		{
			EmitValidationWarning(SiteActor, TEXT("Configured ACF controller has no routine component; mining pawn was not possessed."));

			if (Controller != nullptr)
			{
				Controller->Destroy();
			}

			Actor->Destroy();
			return nullptr;
		}

		Routine->SetRoutineDataAsset(NewObject<UACFAIRoutineDataAsset>(Controller, NAME_None, RF_Transient));
		Controller->Possess(Pawn);
		if (Controller->GetPawn() != Pawn)
		{
			Controller->Destroy();
			Actor->Destroy();

			return nullptr;
		}

		Pawn->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	}

	return Actor;
}

void UMiningSitePresentationCoordinatorComponent::ConfigureRoutineJob(
	AMiningSiteActor& SiteActor, AActor* Actor, EMiningPresentationRole Role, USmartObjectComponent* Station, const FVector& Primary, const FVector& Secondary)
{
	APawn* const Pawn = Cast<APawn>(Actor);
	AACFAIController* const Controller = Pawn != nullptr ? Cast<AACFAIController>(Pawn->GetController()) : nullptr;
	if (Controller == nullptr)
	{
		EmitValidationWarning(SiteActor, TEXT("Routine assignment requires an ACF-controlled pawn; legacy movement is disabled."));
		return;
	}

	ReleaseSmartObjectForActor(Actor); // A live opt-in must retire any legacy task before the new owner starts.
	UMiningRoutineJobComponent* Job = Controller->FindComponentByClass<UMiningRoutineJobComponent>();
	if (Job == nullptr)
	{
		Job = NewObject<UMiningRoutineJobComponent>(Controller, NAME_None, RF_Transient);
		Controller->AddInstanceComponent(Job);
		Job->RegisterComponent();
	}

	RoutineJobs.RemoveAll([](const TWeakObjectPtr<UMiningRoutineJobComponent>& Entry) { return !Entry.IsValid(); });
	RoutineJobs.AddUnique(Job);
	const UMiningSitePresentationConfig* const Config = GetPresentationConfig(SiteActor.MiningSiteComponent);
	if (Config == nullptr
		|| !Job->ConfigureAssignment(this,
			Pawn,
			Role,
			Station,
			Primary,
			Secondary,
			GetInteractionDurationForActor(SiteActor.MiningSiteComponent, Role, Pawn),
			Config->NavigationTimeoutSeconds,
			Config->BlockedRetryDelaySeconds))
	{
		EmitValidationWarning(
			SiteActor, TEXT("ACF duty assignment rejected. Controller needs a routine component and BehaviorTree; external schedule ownership is respected."));
	}
}

void UMiningSitePresentationCoordinatorComponent::ReleaseRoutineAssignment(AActor* Actor)
{
	const APawn* const Pawn = Cast<APawn>(Actor);
	if (Pawn != nullptr && Pawn->GetController())
	{
		if (UMiningRoutineJobComponent* Job = Pawn->GetController()->FindComponentByClass<UMiningRoutineJobComponent>();
			Job != nullptr && Job->GetJobStatus() != EMiningRoutineJobStatus::Unassigned)
		{
			Job->ReleaseAssignment();
		}
	}
}
