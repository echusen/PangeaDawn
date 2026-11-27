#include "Components/MiningSitePresentationCoordinatorComponent.h"

#include "AIController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

#include "Actors/MiningSiteActor.h"
#include "Components/MiningSiteComponent.h"
#include "DataAssets/MiningSitePresentationConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningPresentationMovement, Log, All);

bool UMiningSitePresentationCoordinatorComponent::HasPresentationHoldActive(AActor* Actor) const
{
	if (Actor == nullptr || GetWorld() == nullptr)
	{
		return false;
	}

	const float* const HoldUntilTime = PresentationHoldUntilTimeMap.Find(Actor);
	return HoldUntilTime != nullptr && *HoldUntilTime > GetWorld()->GetTimeSeconds();
}

void UMiningSitePresentationCoordinatorComponent::BeginPresentationHold(AActor* Actor, float Duration)
{
	if (Actor == nullptr || GetWorld() == nullptr || Duration <= 0.0f)
	{
		return;
	}

	PresentationHoldUntilTimeMap.Add(Actor, GetWorld()->GetTimeSeconds() + Duration);
}

void UMiningSitePresentationCoordinatorComponent::UpdatePresentationFacing(AActor* Actor, const FVector& LookAtLocation) const
{
	if (Actor == nullptr)
	{
		return;
	}

	FVector ToTarget = LookAtLocation - Actor->GetActorLocation();
	ToTarget.Z = 0.0f;

	if (ToTarget.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FRotator DesiredRotation = ToTarget.Rotation();
	const float DeltaSeconds = GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : PresentationMovementInterval;
	const FRotator NewRotation =
		FMath::RInterpTo(Actor->GetActorRotation(), FRotator(0.0f, DesiredRotation.Yaw, 0.0f), DeltaSeconds, PresentationFacingInterpSpeed);
	Actor->SetActorRotation(NewRotation);
}

void UMiningSitePresentationCoordinatorComponent::TryMovePresentationActor(
	AMiningSiteActor& SiteActor, AActor* Actor, const FVector& Destination, EMiningPresentationRole Role, bool bAllowFullFidelityNav)
{
	if (Actor == nullptr)
	{
		return;
	}

	ApplyAgentPresentation(Actor, Role, EMiningPresentationState::Traveling, &Destination);

	APawn* const Pawn = Cast<APawn>(Actor);
	if (Pawn != nullptr && !Pawn->GetController())
	{
		Pawn->SpawnDefaultController();
	}

	const FVector CurrentLocation = Actor->GetActorLocation();
	FVector Delta = Destination - CurrentLocation;
	Delta.Z = 0.0f;
	const float DistanceToDestination = Delta.Size();
	if (DistanceToDestination <= 15.0f)
	{
		if (ACharacter* Character = Cast<ACharacter>(Actor))
		{
			if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
			{
				CharacterMovement->Velocity.X = 0.0f;
				CharacterMovement->Velocity.Y = 0.0f;
			}
		}

		if (APawn* PawnAtGoal = Cast<APawn>(Actor))
		{
			if (AAIController* AIController = Cast<AAIController>(PawnAtGoal->GetController()))
			{
				AIController->StopMovement();
			}
		}

		return;
	}

	float MoveSpeed = PresentationMoveSpeed;

	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			MoveSpeed = CharacterMovement->MaxWalkSpeed;

			if (bAllowFullFidelityNav && IsFullFidelityPresentationRelevant(SiteActor))
			{
				if (APawn* PawnCharacter = Cast<APawn>(Character))
				{
					if (AAIController* AIController = Cast<AAIController>(PawnCharacter->GetController()))
					{
						const FVector ProjectedDestination(Destination.X, Destination.Y, Character->GetActorLocation().Z);
						const FVector* const LastRequestedDestination = PresentationMoveTargetMap.Find(Actor);
						const bool bTargetChanged = LastRequestedDestination == nullptr
							|| FVector::DistSquared2D(*LastRequestedDestination, ProjectedDestination) > FMath::Square(50.0f);
						const bool bNotMoving = AIController->GetMoveStatus() != EPathFollowingStatus::Moving;

						if (bTargetChanged || bNotMoving)
						{
							FVector MoveDestination = ProjectedDestination;
							bool bUsePathfinding = true;

							if (Role == EMiningPresentationRole::Courier)
							{
								bUsePathfinding = false;

								if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(SiteActor.GetWorld()))
								{
									FNavLocation ProjectedNavLocation;

									if (NavigationSystem->ProjectPointToNavigation(ProjectedDestination, ProjectedNavLocation, FVector(300.0f, 300.0f, 500.0f)))
									{
										MoveDestination = ProjectedNavLocation.Location;
									}
								}
							}

							const EPathFollowingRequestResult::Type MoveResult =
								AIController->MoveToLocation(MoveDestination, 35.0f, true, bUsePathfinding, true, true, nullptr, true);
							if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
							{
								PresentationMoveTargetMap.Add(Actor, MoveDestination);
								return;
							}

							UE_LOG(LogPangeaMiningPresentationMovement,
								Warning,
								TEXT("Full-fidelity move request failed. Site=%s Pawn=%s Controller=%s Result=%d Destination=%s"),
								*GetNameSafe(&SiteActor),
								*GetNameSafe(PawnCharacter),
								*GetNameSafe(AIController),
								static_cast<int32>(MoveResult),
								*MoveDestination.ToString());
						}
						else
						{
							return;
						}
					}
				}
			}
		}
	}

	const float MaxStepDistance = FMath::Max(2.0f, MoveSpeed * PresentationMovementInterval);
	const FVector StepDelta = Delta.GetSafeNormal() * FMath::Min(DistanceToDestination, MaxStepDistance);
	FVector NewLocation = CurrentLocation + StepDelta;
	NewLocation.Z = CurrentLocation.Z;

	Actor->SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);

	if (!StepDelta.IsNearlyZero())
	{
		Actor->SetActorRotation(StepDelta.Rotation());
	}
}

FVector UMiningSitePresentationCoordinatorComponent::GetPresentationSlotLocation(const AMiningSiteActor& SiteActor, float Radius, float AngleDegrees) const
{
	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	const FVector LocalOffset(FMath::Cos(AngleRadians) * Radius, FMath::Sin(AngleRadians) * Radius, 0.0f);

	return SiteActor.GetActorTransform().TransformPosition(LocalOffset);
}

bool UMiningSitePresentationCoordinatorComponent::TryGetRouteMarkerLocation(
	const AMiningSiteActor& SiteActor, const FString& MarkerName, FVector& OutLocation) const
{
	TInlineComponentArray<USceneComponent*> SceneComponents(&SiteActor);

	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent != nullptr && SceneComponent->GetName() == MarkerName)
		{
			OutLocation = SceneComponent->GetComponentLocation();
			return true;
		}
	}

	return false;
}

bool UMiningSitePresentationCoordinatorComponent::IsPresentationRelevant(const AMiningSiteActor& SiteActor) const
{
	if (SiteActor.GetWorld() == nullptr)
	{
		return false;
	}

	const float ActivationRadiusSq = FMath::Square(bPresentationRelevantCached ? PresentationActivationExitRadius : PresentationActivationRadius);
	bool bRelevantNow = false;

	for (TActorIterator<APawn> PawnIt(SiteActor.GetWorld()); PawnIt; ++PawnIt)
	{
		const APawn* const Pawn = *PawnIt;
		if (Pawn != nullptr && Pawn->IsPlayerControlled() && FVector::DistSquared(Pawn->GetActorLocation(), SiteActor.GetActorLocation()) <= ActivationRadiusSq)
		{
			bRelevantNow = true;
			break;
		}
	}

	const_cast<UMiningSitePresentationCoordinatorComponent*>(this)->bPresentationRelevantCached = bRelevantNow;
	return bRelevantNow;
}

bool UMiningSitePresentationCoordinatorComponent::IsFullFidelityPresentationRelevant(const AMiningSiteActor& SiteActor) const
{
	const bool bRelevantNow = IsPresentationRelevant(SiteActor);
	const_cast<UMiningSitePresentationCoordinatorComponent*>(this)->bFullFidelityRelevantCached = bRelevantNow;

	return bRelevantNow;
}

float UMiningSitePresentationCoordinatorComponent::GetInteractionDurationForRole(
	const UMiningSiteComponent* MiningSiteComponent, EMiningPresentationRole Role) const
{
	if (const FMiningPresentationRoleConfig* RoleConfig = GetRoleConfig(MiningSiteComponent, Role))
	{
		if (RoleConfig->InteractionDuration > 0.0f)
		{
			return RoleConfig->InteractionDuration;
		}
	}

	switch (Role)
	{
		case EMiningPresentationRole::Worker:
			return WorkerInteractionDuration;
		case EMiningPresentationRole::Guard:
			return GuardInteractionDuration;
		case EMiningPresentationRole::Courier:
			return CourierInteractionDuration;
		default:
			return WorkerInteractionDuration;
	}
}

float UMiningSitePresentationCoordinatorComponent::GetInteractionDurationForActor(
	const UMiningSiteComponent* MiningSiteComponent, const EMiningPresentationRole Role, const AActor* Actor) const
{
	const float BaseDuration = GetInteractionDurationForRole(MiningSiteComponent, Role);

	if (Actor == nullptr)
	{
		return BaseDuration;
	}

	const FMiningPresentationRoleConfig* const RoleConfig = GetRoleConfig(MiningSiteComponent, Role);
	if (RoleConfig == nullptr || RoleConfig->InteractionDurationVariance <= 0.0f)
	{
		return BaseDuration;
	}

	const uint32 Hash = GetTypeHash(Actor->GetFName());
	const float Normalized = static_cast<float>(Hash % 1000) / 999.0f;
	const float Offset = FMath::Lerp(-RoleConfig->InteractionDurationVariance, RoleConfig->InteractionDurationVariance, Normalized);

	return FMath::Max(0.1f, BaseDuration + Offset);
}
