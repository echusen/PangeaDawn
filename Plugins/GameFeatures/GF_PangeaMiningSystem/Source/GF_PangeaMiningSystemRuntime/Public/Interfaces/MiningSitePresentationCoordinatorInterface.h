#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MiningSitePresentationCoordinatorInterface.generated.h"

class AActor;
class AMiningSiteActor;
class UMiningSiteComponent;
class USmartObjectComponent;

/** UObject side of IMiningSitePresentationCoordinatorInterface, required by the reflection system. */
UINTERFACE()
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningSitePresentationCoordinatorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract between a mining site actor and the object that manages its cosmetic worker, guard and courier NPCs.
 * The site actor owns the actor and smart object arrays and passes them in on every call.
 */
class GF_PANGEAMININGSYSTEMRUNTIME_API IMiningSitePresentationCoordinatorInterface
{
	GENERATED_BODY()

public:
	/** Brings the spawned presentation actors in line with the current state of the site. */
	virtual void RefreshPresentationActors(AMiningSiteActor& SiteActor,
		UMiningSiteComponent* MiningSiteComponent,
		AActor* SpawnedSiteChest,
		TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
		TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
		TObjectPtr<AActor>& SpawnedCourierActor) = 0;

	/** Removes the presentation actors held in the given containers. */
	virtual void ClearPresentationActors(
		TArray<TObjectPtr<AActor>>& SpawnedWorkerActors, TArray<TObjectPtr<AActor>>& SpawnedGuardActors, TObjectPtr<AActor>& SpawnedCourierActor) = 0;

	/** Configures the worker and guard smart object components of the site. */
	virtual void ConfigureSmartObjectComponents(AMiningSiteActor& SiteActor,
		UMiningSiteComponent* MiningSiteComponent,
		TArray<TObjectPtr<USmartObjectComponent>>& WorkerSmartObjectComponents,
		TArray<TObjectPtr<USmartObjectComponent>>& GuardSmartObjectComponents) const = 0;

	/** Advances the movement and station routes of the presentation actors. */
	virtual void UpdatePresentationActorMovement(AMiningSiteActor& SiteActor,
		UMiningSiteComponent* MiningSiteComponent,
		AActor* SpawnedSiteChest,
		TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
		TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
		TObjectPtr<AActor>& SpawnedCourierActor,
		const TArray<TObjectPtr<USmartObjectComponent>>& WorkerSmartObjectComponents,
		const TArray<TObjectPtr<USmartObjectComponent>>& GuardSmartObjectComponents) = 0;

	/** Seconds between calls to RefreshPresentationActors. */
	virtual float GetPresentationRefreshInterval() const = 0;

	/** Seconds between calls to UpdatePresentationActorMovement. */
	virtual float GetPresentationMovementInterval() const = 0;
};
