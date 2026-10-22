#include "Components/MiningSitePresentationCoordinatorComponent.h"

#include "Actors/MiningSiteActor.h"
#include "AIController.h"
#include "Components/ACFInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Runtime/MiningPresentationActorRetirement.h"

DEFINE_LOG_CATEGORY_STATIC(LogPangeaMiningPresentationActorSync, Log, All);

namespace
{
	static FName GetMiningSiteTag(const AMiningSiteActor& SiteActor)
	{
		return FName(*FString::Printf(TEXT("Mining.Site.%s"), *SiteActor.GetName()));
	}
}

void UMiningSitePresentationCoordinatorComponent::SyncPresentationActorsForRole(
	AMiningSiteActor& SiteActor,
	TArray<TObjectPtr<AActor>>& ActorArray,
	TSoftClassPtr<AActor> ActorClass,
	int32 DesiredCount,
	float Radius,
	float AngleOffsetDegrees,
	EMiningPresentationRole Role)
{
	while (ActorArray.Num() > DesiredCount)
	{
		if (AActor* ExistingActor = ActorArray.Pop())
		{
			ReleaseRoutineAssignment(ExistingActor);
			ReleaseSmartObjectForActor(ExistingActor);
			PresentationRoutePhaseMap.Remove(ExistingActor);
			PresentationMoveTargetMap.Remove(ExistingActor);
			PresentationSmartObjectLastStatusMap.Remove(ExistingActor);
			PresentationSmartObjectStartTimeMap.Remove(ExistingActor);
			PresentationTargetHoldStartedMap.Remove(ExistingActor);
			ApplyAgentPresentation(ExistingActor, Role, EMiningPresentationState::Idle);
			PangeaMiningPrivate::RetirePresentationActor(ExistingActor);
		}
	}

	if (DesiredCount <= 0 || ActorClass.IsNull())
	{
		return;
	}

	UClass* LoadedClass = ActorClass.LoadSynchronous();
	if (!LoadedClass)
	{
		return;
	}

	while (ActorArray.Num() < DesiredCount)
	{
		const int32 SpawnIndex = ActorArray.Num();
		const float AngleStep = DesiredCount > 1 ? 40.0f : 0.0f;
		const FVector SpawnLocation = GetPresentationSlotLocation(SiteActor, Radius, AngleOffsetDegrees + (SpawnIndex * AngleStep));

		AActor* SpawnedActor = SpawnConfiguredPresentationActor(SiteActor, LoadedClass, SpawnLocation);
		if (!SpawnedActor)
		{
			break;
		}

		const UMiningSitePresentationConfig* Config = GetPresentationConfig(SiteActor.MiningSiteComponent);
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		if (!Config || !Config->bUseACFRoutineJobs) SpawnedActor->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!PrimitiveComponent)
			{
				continue;
			}

			if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(PrimitiveComponent))
			{
				CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Block);
				CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
				CapsuleComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			}
			else
			{
				PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		if (UACFInteractionComponent* InteractionComponent = SpawnedActor->FindComponentByClass<UACFInteractionComponent>())
		{
			InteractionComponent->Deactivate();
		}

		SpawnedActor->Tags.Add(Role == EMiningPresentationRole::Worker ? FName(TEXT("Mining.Worker")) : FName(TEXT("Mining.Guard")));
		SpawnedActor->Tags.Add(FName(*FString::Printf(TEXT("Mining.Site.%s"), *SiteActor.GetName())));
		if (APawn* SpawnedPawn = Cast<APawn>(SpawnedActor))
		{
			SpawnedPawn->SpawnDefaultController();
			UE_LOG(LogPangeaMiningPresentationActorSync, Log, TEXT("Spawned mining pawn controller sync. Site=%s Pawn=%s Controller=%s"), *GetNameSafe(&SiteActor), *GetNameSafe(SpawnedPawn), *GetNameSafe(SpawnedPawn->GetController()));
		}

		PresentationRoutePhaseMap.Add(SpawnedActor, (SpawnIndex % 2) == 1);
		PresentationTargetHoldStartedMap.Add(SpawnedActor, false);
		ApplyAgentPresentation(SpawnedActor, Role, EMiningPresentationState::Idle);
		ActorArray.Add(SpawnedActor);
		UE_LOG(LogPangeaMiningPresentationActorSync, Log, TEXT("Spawned mining presentation actor. Site=%s Actor=%s Class=%s"), *GetNameSafe(&SiteActor), *GetNameSafe(SpawnedActor), *GetNameSafe(LoadedClass));
	}
}

void UMiningSitePresentationCoordinatorComponent::SyncCourierPresentationActor(
	AMiningSiteActor& SiteActor,
	TObjectPtr<AActor>& SpawnedCourierActor,
	TSoftClassPtr<AActor> ActorClass,
	bool bShouldExist,
	float Distance,
	float AngleOffsetDegrees)
{
	if (!bShouldExist)
	{
		if (SpawnedCourierActor)
		{
			ReleaseRoutineAssignment(SpawnedCourierActor);
			ReleaseSmartObjectForActor(SpawnedCourierActor);
			PresentationRoutePhaseMap.Remove(SpawnedCourierActor);
			PresentationMoveTargetMap.Remove(SpawnedCourierActor);
			PresentationSmartObjectLastStatusMap.Remove(SpawnedCourierActor);
			PresentationSmartObjectStartTimeMap.Remove(SpawnedCourierActor);
			PresentationTargetHoldStartedMap.Remove(SpawnedCourierActor);
			ApplyAgentPresentation(SpawnedCourierActor, EMiningPresentationRole::Courier, EMiningPresentationState::Idle);
			PangeaMiningPrivate::RetirePresentationActor(SpawnedCourierActor);
			SpawnedCourierActor = nullptr;
		}
		return;
	}

	if (SpawnedCourierActor)
	{
		return;
	}

	UClass* LoadedClass = ActorClass.LoadSynchronous();
	if (!LoadedClass)
	{
		return;
	}

	AActor* AdoptedCourierActor = nullptr;
	TArray<AActor*> DuplicateCourierActors;
	const FName SiteTag = GetMiningSiteTag(SiteActor);
	for (TActorIterator<AActor> It(SiteActor.GetWorld(), LoadedClass); It; ++It)
	{
		AActor* Candidate = *It;
		const UMiningSitePresentationConfig* CurrentConfig = GetPresentationConfig(SiteActor.MiningSiteComponent);
		// Hybrid actors are site-owned. Never adopt/destroy another site's courier.
		if (CurrentConfig && CurrentConfig->bUseACFRoutineJobs) continue;
		if (!Candidate || Candidate->IsPendingKillPending())
		{
			continue;
		}

		const bool bTaggedForThisSite = Candidate->ActorHasTag(SiteTag);
		const bool bTaggedAsCourier = Candidate->ActorHasTag(TEXT("Mining.Courier"));
		const bool bNearSite = FVector::DistSquared2D(Candidate->GetActorLocation(), SiteActor.GetActorLocation()) <= FMath::Square(2500.0f);
		if (!bNearSite || (!bTaggedForThisSite && !bTaggedAsCourier))
		{
			continue;
		}

		if (!AdoptedCourierActor)
		{
			AdoptedCourierActor = Candidate;
		}
		else
		{
			DuplicateCourierActors.Add(Candidate);
		}
	}

	for (AActor* DuplicateCourier : DuplicateCourierActors)
	{
		ReleaseRoutineAssignment(DuplicateCourier);
		ReleaseSmartObjectForActor(DuplicateCourier);
		PresentationRoutePhaseMap.Remove(DuplicateCourier);
		PresentationMoveTargetMap.Remove(DuplicateCourier);
		PresentationSmartObjectLastStatusMap.Remove(DuplicateCourier);
		PresentationSmartObjectStartTimeMap.Remove(DuplicateCourier);
		PresentationTargetHoldStartedMap.Remove(DuplicateCourier);
		ApplyAgentPresentation(DuplicateCourier, EMiningPresentationRole::Courier, EMiningPresentationState::Idle);
		PangeaMiningPrivate::RetirePresentationActor(DuplicateCourier);
	}

	if (AdoptedCourierActor)
	{
		SpawnedCourierActor = AdoptedCourierActor;
		SpawnedCourierActor->SetOwner(&SiteActor);
		SpawnedCourierActor->Tags.AddUnique(TEXT("Mining.Courier"));
		SpawnedCourierActor->Tags.AddUnique(SiteTag);
		if (APawn* SpawnedPawn = Cast<APawn>(SpawnedCourierActor))
		{
			if (!SpawnedPawn->GetController())
			{
				SpawnedPawn->SpawnDefaultController();
			}
		}

		if (!PresentationRoutePhaseMap.Contains(SpawnedCourierActor))
		{
			PresentationRoutePhaseMap.Add(SpawnedCourierActor, false);
		}
		if (!PresentationTargetHoldStartedMap.Contains(SpawnedCourierActor))
		{
			PresentationTargetHoldStartedMap.Add(SpawnedCourierActor, false);
		}

		ApplyAgentPresentation(SpawnedCourierActor, EMiningPresentationRole::Courier, EMiningPresentationState::Idle);
		return;
	}

	SpawnedCourierActor = SpawnConfiguredPresentationActor(SiteActor, LoadedClass, GetPresentationSlotLocation(SiteActor, Distance, AngleOffsetDegrees));
	if (!SpawnedCourierActor)
	{
		return;
	}

	const UMiningSitePresentationConfig* Config = GetPresentationConfig(SiteActor.MiningSiteComponent);
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	if (!Config || !Config->bUseACFRoutineJobs) SpawnedCourierActor->GetComponents(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(PrimitiveComponent))
		{
			CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Block);
			CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			CapsuleComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		else
		{
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (UACFInteractionComponent* InteractionComponent = SpawnedCourierActor->FindComponentByClass<UACFInteractionComponent>())
	{
		InteractionComponent->Deactivate();
	}

	SpawnedCourierActor->Tags.Add(FName(TEXT("Mining.Courier")));
	SpawnedCourierActor->Tags.Add(FName(*FString::Printf(TEXT("Mining.Site.%s"), *SiteActor.GetName())));
	if (APawn* SpawnedPawn = Cast<APawn>(SpawnedCourierActor))
	{
		SpawnedPawn->SpawnDefaultController();
		UE_LOG(LogPangeaMiningPresentationActorSync, Log, TEXT("Spawned mining courier controller sync. Site=%s Pawn=%s Controller=%s"), *GetNameSafe(&SiteActor), *GetNameSafe(SpawnedPawn), *GetNameSafe(SpawnedPawn->GetController()));
	}

	PresentationRoutePhaseMap.Add(SpawnedCourierActor, false);
	PresentationTargetHoldStartedMap.Add(SpawnedCourierActor, false);
	ApplyAgentPresentation(SpawnedCourierActor, EMiningPresentationRole::Courier, EMiningPresentationState::Idle);
	UE_LOG(LogPangeaMiningPresentationActorSync, Log, TEXT("Spawned mining courier presentation actor. Site=%s Actor=%s Class=%s"), *GetNameSafe(&SiteActor), *GetNameSafe(SpawnedCourierActor), *GetNameSafe(LoadedClass));
}
