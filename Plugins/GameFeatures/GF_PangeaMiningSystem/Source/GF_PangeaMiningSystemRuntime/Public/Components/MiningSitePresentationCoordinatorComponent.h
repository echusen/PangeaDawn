#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataAssets/MiningSitePresentationConfig.h"
#include "Interfaces/MiningPresentationAgentInterface.h"
#include "Interfaces/MiningSitePresentationCoordinatorInterface.h"
#include "SmartObjectRuntime.h"
#include "MiningSitePresentationCoordinatorComponent.generated.h"

class AActor;
class AMiningSiteActor;
class UMiningSmartObjectInteractionTask;
class UMiningRoutineJobComponent;
class UMiningSiteComponent;
class USmartObjectComponent;

UCLASS(ClassGroup=(Pangea), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningSitePresentationCoordinatorComponent : public UActorComponent, public IMiningSitePresentationCoordinatorInterface
{
	GENERATED_BODY()

public:
	UMiningSitePresentationCoordinatorComponent();

	/** Presentation only: never grants resources or chooses the NPC's AI state. */
	UFUNCTION(BlueprintCallable, Category="Presentation")
	void ApplyRoutinePresentation(AActor* Actor, EMiningPresentationRole Role, EMiningPresentationState State, const FVector& Focus) const;
	UFUNCTION(BlueprintCallable, Category="Presentation")
	float GetRoutinePresentationDuration(AActor* Actor, EMiningPresentationState State, float Fallback) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Presentation")
	void ApplyRoutineTravelSpeed(AActor* Actor, EMiningPresentationRole Role, float Speed) const;

	void RefreshPresentationActors(
		AMiningSiteActor& SiteActor,
		UMiningSiteComponent* MiningSiteComponent,
		AActor* SpawnedSiteChest,
		TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
		TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
		TObjectPtr<AActor>& SpawnedCourierActor);

	void ClearPresentationActors(
		TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
		TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
		TObjectPtr<AActor>& SpawnedCourierActor);

	void ConfigureSmartObjectComponents(
		AMiningSiteActor& SiteActor,
		UMiningSiteComponent* MiningSiteComponent,
		TArray<TObjectPtr<USmartObjectComponent>>& WorkerSmartObjectComponents,
		TArray<TObjectPtr<USmartObjectComponent>>& GuardSmartObjectComponents) const;

	void UpdatePresentationActorMovement(
		AMiningSiteActor& SiteActor,
		UMiningSiteComponent* MiningSiteComponent,
		AActor* SpawnedSiteChest,
		TArray<TObjectPtr<AActor>>& SpawnedWorkerActors,
		TArray<TObjectPtr<AActor>>& SpawnedGuardActors,
		TObjectPtr<AActor>& SpawnedCourierActor,
		const TArray<TObjectPtr<USmartObjectComponent>>& WorkerSmartObjectComponents,
		const TArray<TObjectPtr<USmartObjectComponent>>& GuardSmartObjectComponents);

	virtual float GetPresentationRefreshInterval() const override { return PresentationRefreshInterval; }
	virtual float GetPresentationMovementInterval() const override { return PresentationMovementInterval; }

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationRefreshInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationMovementInterval = 0.033f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationActivationRadius = 2500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationActivationExitRadius = 2800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationFullFidelityRadius = 1200.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationFullFidelityExitRadius = 1450.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationMoveSpeed = 275.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float WorkerInteractionDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float GuardInteractionDuration = 2.25f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float CourierInteractionDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Mining|AI")
	float PresentationFacingInterpSpeed = 6.0f;

private:
	friend class UPangeaMiningConvoyComponent;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void CancelAllSmartObjectTasks();
	bool bClearingSmartObjectTasks = false;
	TArray<TWeakObjectPtr<UMiningRoutineJobComponent>> RoutineJobs;
	AActor* SpawnConfiguredPresentationActor(AMiningSiteActor& SiteActor, UClass* ActorClass, const FVector& Location);
	void ConfigureRoutineJob(AMiningSiteActor& SiteActor, AActor* Actor, EMiningPresentationRole Role, USmartObjectComponent* Station, const FVector& Primary, const FVector& Secondary);
	void ReleaseRoutineAssignment(AActor* Actor);

	TMap<TWeakObjectPtr<AActor>, bool> PresentationRoutePhaseMap;
	TMap<TWeakObjectPtr<AActor>, FVector> PresentationMoveTargetMap;
	TMap<TWeakObjectPtr<AActor>, FSmartObjectClaimHandle> PresentationClaimHandleMap;
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<AActor>, TObjectPtr<UMiningSmartObjectInteractionTask>> PresentationSmartObjectTaskMap;
	TMap<TWeakObjectPtr<AActor>, FString> PresentationSmartObjectLastStatusMap;
	TMap<TWeakObjectPtr<AActor>, float> PresentationSmartObjectStartTimeMap;
	TMap<TWeakObjectPtr<AActor>, float> PresentationHoldUntilTimeMap;
	TMap<TWeakObjectPtr<AActor>, bool> PresentationTargetHoldStartedMap;
	bool bPresentationRelevantCached = false;
	bool bFullFidelityRelevantCached = false;
	mutable TSet<FString> ValidationWarningsIssued;

	void SyncPresentationActorsForRole(
		AMiningSiteActor& SiteActor,
		TArray<TObjectPtr<AActor>>& ActorArray,
		TSoftClassPtr<AActor> ActorClass,
		int32 DesiredCount,
		float Radius,
		float AngleOffsetDegrees,
		EMiningPresentationRole Role);

	void SyncCourierPresentationActor(
		AMiningSiteActor& SiteActor,
		TObjectPtr<AActor>& SpawnedCourierActor,
		TSoftClassPtr<AActor> ActorClass,
		bool bShouldExist,
		float Distance,
		float AngleOffsetDegrees);

	void UpdatePresentationActorRoute(
		AMiningSiteActor& SiteActor,
		TArray<TObjectPtr<AActor>>& ActorArray,
		EMiningPresentationRole Role,
		const TArray<TObjectPtr<USmartObjectComponent>>& SmartObjectComponents,
		float PrimaryRadius,
		float PrimaryAngleOffsetDegrees,
		float SecondaryRadius,
		float SecondaryAngleOffsetDegrees);

	void UpdateCourierPresentationRoute(
		AMiningSiteActor& SiteActor,
		TObjectPtr<AActor>& SpawnedCourierActor,
		float PrimaryRadius,
		float PrimaryAngleOffsetDegrees,
		float SecondaryRadius,
		float SecondaryAngleOffsetDegrees);

	bool HasSmartObjectInteractionTimedOut(AActor* Actor, float MaxDuration) const;
	bool HasPresentationHoldActive(AActor* Actor) const;
	void BeginPresentationHold(AActor* Actor, float Duration);
	void UpdatePresentationFacing(AActor* Actor, const FVector& LookAtLocation) const;
	bool TryStartSmartObjectBehavior(AMiningSiteActor& SiteActor, AActor* Actor, USmartObjectComponent* SmartObjectComponent, EMiningPresentationRole Role, int32 PreferredSlotIndex = INDEX_NONE);
	void ReleaseSmartObjectForActor(AActor* Actor);
	void TryMovePresentationActor(AMiningSiteActor& SiteActor, AActor* Actor, const FVector& Destination, EMiningPresentationRole Role, bool bAllowFullFidelityNav = true);
	FVector GetPresentationSlotLocation(const AMiningSiteActor& SiteActor, float Radius, float AngleDegrees) const;
	bool TryGetRouteMarkerLocation(const AMiningSiteActor& SiteActor, const FString& MarkerName, FVector& OutLocation) const;
	bool IsPresentationRelevant(const AMiningSiteActor& SiteActor) const;
	bool IsFullFidelityPresentationRelevant(const AMiningSiteActor& SiteActor) const;
	float GetInteractionDurationForRole(const UMiningSiteComponent* MiningSiteComponent, EMiningPresentationRole Role) const;
	float GetInteractionDurationForActor(const UMiningSiteComponent* MiningSiteComponent, EMiningPresentationRole Role, const AActor* Actor) const;
	EMiningPresentationState GetActiveStateForRole(EMiningPresentationRole Role) const;
	void ApplyAgentPresentation(AActor* Actor, EMiningPresentationRole Role, EMiningPresentationState State, const FVector* FocusLocation = nullptr) const;
	void EnsurePresentationAgentComponent(AActor* Actor) const;
	const UMiningSitePresentationConfig* GetPresentationConfig(const UMiningSiteComponent* MiningSiteComponent) const;
	const FMiningPresentationRoleConfig* GetRoleConfig(const UMiningSiteComponent* MiningSiteComponent, EMiningPresentationRole Role) const;
	bool TryGetConfiguredStationTargets(const AMiningSiteActor& SiteActor, const UMiningSiteComponent* MiningSiteComponent, EMiningPresentationRole Role, int32 StationIndex, FVector& OutPrimaryTarget, FVector& OutSecondaryTarget) const;
	void ValidatePresentationSetup(const AMiningSiteActor& SiteActor, const UMiningSiteComponent* MiningSiteComponent, int32 WorkerCount, int32 GuardCount, bool bNeedsCourier) const;
	void EmitValidationWarning(const AMiningSiteActor& SiteActor, const FString& Message) const;
	void StopActorMovement(AActor* Actor) const;
};
