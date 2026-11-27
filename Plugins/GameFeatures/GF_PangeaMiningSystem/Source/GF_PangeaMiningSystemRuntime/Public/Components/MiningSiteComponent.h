#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "Types/MiningTypes.h"

#include "MiningSiteComponent.generated.h"

class FLifetimeProperty;
class UACFStorageComponent;
class UMiningSiteDefinition;
class UPangeaMiningTransportComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogPangeaMiningSite, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMiningSiteLevelChangedSignature, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMiningSiteStorageChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMiningWorkforceChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMiningDayEndedSignature, int64, CompletedDay, int32, LostWorkers);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FMiningSiteShipmentResolvedSignature, const FMiningItemQuantity&, RequestedShipment, int32, DeliveredQuantity, bool, bLost);

/**
 * Authoritative economy state of one mining site: level, stored resources, automated
 * production, shipment cooldown and workforce. Exists so the simulation lives in one
 * replicated, savable component that is independent of the site's presentation actors.
 */
UCLASS(ClassGroup = (Pangea), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningSiteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMiningSiteComponent();

	/** Writes a complete SaveGame snapshot when saving and resets timing/workforce state before a SaveGame load. */
	virtual void Serialize(FArchive& Ar) override;

	/** Accounts elapsed world time (production and mining days) on the authority once per tick interval. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** ALS pre-save hook: settle online production without dispatching a shipment. */
	UFUNCTION(BlueprintCallable, Category = "Mining|Timing")
	void OnComponentSaved();

	/** ALS post-load hook: resume saved progress against this session's game clock. */
	UFUNCTION(BlueprintCallable, Category = "Mining|Timing")
	void OnComponentLoaded();

	/** Fraction of one output unit (0 to 1) already produced but not yet stored. */
	UFUNCTION(BlueprintPure, Category = "Mining|Timing")
	double GetProductionRemainder() const
	{
		return ProductionRemainder;
	}

	/** Seconds until the next automatic shipment may be dispatched. Clients get the last replicated snapshot. */
	UFUNCTION(BlueprintPure, Category = "Mining|Timing")
	double GetShipmentCooldownRemaining() const;

	/** Data asset that defines this site's levels, storage rules, day length and transport mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining")
	TObjectPtr<UMiningSiteDefinition> SiteDefinition;

	/** Fired when the site is established (OldLevel is INDEX_NONE) and after every upgrade. */
	UPROPERTY(BlueprintAssignable, Category = "Mining")
	FMiningSiteLevelChangedSignature OnSiteLevelChanged;

	/** Fired after stored items are added, removed or rebuilt from the linked storage component. */
	UPROPERTY(BlueprintAssignable, Category = "Mining")
	FMiningSiteStorageChangedSignature OnStorageChanged;

	/** Fired when an instant (non-hybrid) automatic shipment is resolved, with the delivered quantity and whether it was lost. */
	UPROPERTY(BlueprintAssignable, Category = "Mining")
	FMiningSiteShipmentResolvedSignature OnShipmentResolved;

	/** Fired when the number of worker vacancies changes through a death, a day-end loss or a recruitment. */
	UPROPERTY(BlueprintAssignable, Category = "Mining|Workforce")
	FMiningWorkforceChangedSignature OnWorkforceChanged;

	/** Fired when one or more simulated mining days complete, with the total of completed days and the workers lost at that day end. */
	UPROPERTY(BlueprintAssignable, Category = "Mining|Workforce")
	FMiningDayEndedSignature OnMiningDayEnded;

	/** Workers authored for the current level minus the current vacancies; zero while the site is not established. */
	UFUNCTION(BlueprintPure, Category = "Mining|Workforce")
	int32 GetLivingWorkerCount() const;

	/** Number of worker positions that are currently empty and can be refilled by recruitment. */
	UFUNCTION(BlueprintPure, Category = "Mining|Workforce")
	int32 GetWorkerVacancies() const
	{
		return WorkerVacancies;
	}

	/** Seconds of online time accounted in the current simulated mining day. */
	UFUNCTION(BlueprintPure, Category = "Mining|Workforce")
	double GetMiningDayElapsedSeconds() const
	{
		return MiningDayElapsedSeconds;
	}

	/** Number of simulated mining days this site has completed. */
	UFUNCTION(BlueprintPure, Category = "Mining|Workforce")
	int64 GetCompletedMiningDays() const
	{
		return CompletedMiningDays;
	}

	/** True when a player pawn is within the definition's worker attendance radius of the site. */
	UFUNCTION(BlueprintPure, Category = "Mining|Workforce")
	bool IsSiteAttended() const;

	/** True when the established site is idle, has a vacancy, its level has a recruitment cost and CostContext can pay it. */
	UFUNCTION(BlueprintPure, Category = "Mining|Workforce")
	bool CanRecruitWorker(UObject* CostContext) const;

	/** Authority only; the caller must validate the requesting player and distance. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mining|Workforce")
	bool RecruitWorker(UObject* CostContext);

	/** Called by the owned worker's ACF OnDeath, never by presentation culling/OnDestroyed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mining|Workforce")
	bool RecordWorkerDeath();

	/** True once EstablishSite has succeeded (or an established site was loaded). */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool IsEstablished() const
	{
		return bEstablished;
	}

	/** Current level of the site; a newly established site starts at level 0. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	int32 GetCurrentLevel() const
	{
		return CurrentLevel;
	}

	/** Maximum number of stored units at the current level; zero when the level cannot be resolved. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	int32 GetStorageCapacity() const;

	/** Total number of units currently stored, across all item entries. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	int32 GetStoredUnits() const;

	/** Storage capacity that is still free. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	int32 GetAvailableStorage() const;

	/** Stored quantity matching Item by class or, when Item has no class, by tag. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	int32 GetStoredQuantity(const FMiningItemQuantity& Item) const;

	/** Manual mining speed multiplier of the current level; 1 when the level cannot be resolved. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	float GetManualMiningSpeedMultiplier() const;

	/** Reads the definition of the current level. Returns false when there is no site definition or level entry. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool GetCurrentLevelDefinition(FMiningSiteLevelDefinition& OutDefinition) const;

	/** Reads the definition of the level after the current one. Returns false when there is none. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool GetNextLevelDefinition(FMiningSiteLevelDefinition& OutDefinition) const;

	/** Outputs the upgrade cost of the next level; empty when there is no next level. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	void GetNextUpgradeCost(TArray<FMiningItemQuantity>& OutCost) const;

	/** Authority only. Establishes the site at level 0 and starts its timing; fails when already established or no level 0 exists. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool EstablishSite();

	/** True when the site is established, not waiting for a load restore or inside another timing/upgrade operation, and a next level exists. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool CanUpgradeSite() const;

	/** Authority only. Raises the site one level without charging any cost. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool UpgradeSite();

	/** True when the site can be upgraded and UpgradeContext can pay the next level's cost. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool CanPurchaseUpgrade(UObject* UpgradeContext) const;

	/** Authority only. Consumes the next level's cost from UpgradeContext and upgrades the site. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool PurchaseUpgrade(UObject* UpgradeContext);

	/** Checks whether Cost can be paid. The native version reads the ACF inventory of UpgradeContext (an actor, or a component's owner). */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Mining|Costs")
	bool CanPayUpgradeCost(UObject* UpgradeContext, const TArray<FMiningItemQuantity>& Cost) const;

	/** Removes Cost from the payer. The native version removes the items from the ACF inventory of UpgradeContext. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining|Costs")
	bool ConsumeUpgradeCost(UObject* UpgradeContext, const TArray<FMiningItemQuantity>& Cost);

	/** Authority only. Stores as much of Item as capacity and the allowed-item tags permit; returns the accepted quantity. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	int32 AddStoredItem(const FMiningItemQuantity& Item);

	/** Authority only. Removes up to Item.Quantity matching units from storage; returns the removed quantity. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	int32 RemoveStoredItem(const FMiningItemQuantity& Item);

	/** Adds explicit simulation time; does not replace/advance the online world clock. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	int32 ProduceForElapsedSeconds(float ElapsedSeconds);

	/** Accounts elapsed time and tries one automatic shipment. With hybrid transport it asks the transport component to dispatch instead. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool TryAutoShipment();

	/** Accounts production for the world time elapsed since the last accounting, then tries an instant automatic shipment. */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void SyncProductionFromWorldTime();

	/** Authority only. Makes the given ACF storage component the backing store of the site's items; existing stored items are moved into it when it is empty. */
	UFUNCTION(BlueprintCallable, Category = "Mining|Storage")
	void SetLinkedStorageComponent(UACFStorageComponent* InStorageComponent);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Whether the site has been established and takes part in the simulation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining")
	bool bEstablished = false;

	/** Current level of the site, resolved against the site definition. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining")
	int32 CurrentLevel = 0;

	/** Items currently held by the site; mirrors the linked storage component when one is set. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining")
	TArray<FMiningStoredItem> StoredItems;

	// Legacy archive fields only. Session timestamps cannot be interpreted after loading.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining")
	double LastProductionWorldSeconds = 0.0;

	/** Legacy archive field only; see LastProductionWorldSeconds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Mining")
	double LastShipmentWorldSeconds = 0.0;

	/** Fraction of one output unit that has been produced but not yet stored. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining|Timing")
	double ProductionRemainder = 0.0;

	/** Seconds left before the next automatic shipment, as of the last time accounting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining|Timing")
	double ShipmentCooldownRemaining = 0.0;

	/** Save format version of the timing fields; 0 marks a legacy save that is migrated on load. */
	UPROPERTY(SaveGame)
	int32 MiningTimingVersion = 1;

	/** Worker positions of the current level that are empty. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining|Workforce")
	int32 WorkerVacancies = 0;

	/** Seconds accounted in the current simulated mining day. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining|Workforce")
	double MiningDayElapsedSeconds = 0.0;

	/** Number of simulated mining days completed by this site. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category = "Mining|Workforce")
	int64 CompletedMiningDays = 0;

	/** Saved seed for day-end worker loss rolls, so loading does not reroll a resolved day. */
	UPROPERTY(SaveGame)
	int32 WorkerRiskSeed = 0;

	/** Save format version of the workforce fields; 0 marks a pre-workforce save that is migrated on load. */
	UPROPERTY(SaveGame)
	int32 MiningWorkforceVersion = 1;

private:
	friend class UPangeaMiningTransportComponent;

	// Only the authoritative ledger may withdraw cargo without immediately delivering it.
	bool ReserveTransportCargo(FMiningItemQuantity& OutCargo);
	UPROPERTY(Transient)
	TObjectPtr<UACFStorageComponent> LinkedStorageComponent;

	double LastAccountingWorldSeconds = 0.0;
	bool bClockInitialized = false;
	bool bAwaitingTimingRestore = false;
	bool bTimingOperationInProgress = false;
	bool bUpgradeInProgress = false;

	bool IsTimingReady() const;
	void AccountElapsedTime();
	int32 AdvanceProduction(double ElapsedSeconds);
	int32 ProduceSegment(double ElapsedSeconds);
	void ResolveMiningDay();
	bool DispatchAutoShipment();
	bool UpgradeSiteInternal();
	double GetConfiguredShipmentInterval() const;

	bool HasAuthority() const;
	int32 FindStoredItemIndex(const FMiningItemQuantity& Item) const;
	bool CanStoreItem(const FMiningItemQuantity& Item) const;
	double GetWorldSeconds() const;
	UFUNCTION()
	void HandleLinkedStorageChanged();
	void RebuildStoredItemsFromLinkedStorage();
};
