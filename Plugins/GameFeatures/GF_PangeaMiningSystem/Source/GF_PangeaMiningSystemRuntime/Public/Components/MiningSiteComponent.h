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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMiningSiteShipmentResolvedSignature, const FMiningItemQuantity&, RequestedShipment, int32, DeliveredQuantity, bool, bLost);

UCLASS(ClassGroup=(Pangea), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningSiteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMiningSiteComponent();
	virtual void Serialize(FArchive& Ar) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** ALS pre-save hook: settle online production without dispatching a shipment. */
	UFUNCTION(BlueprintCallable, Category="Mining|Timing")
	void OnComponentSaved();

	/** ALS post-load hook: resume saved progress against this session's game clock. */
	UFUNCTION(BlueprintCallable, Category="Mining|Timing")
	void OnComponentLoaded();

	UFUNCTION(BlueprintPure, Category="Mining|Timing")
	double GetProductionRemainder() const { return ProductionRemainder; }

	UFUNCTION(BlueprintPure, Category="Mining|Timing")
	double GetShipmentCooldownRemaining() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining")
	TObjectPtr<UMiningSiteDefinition> SiteDefinition;

	UPROPERTY(BlueprintAssignable, Category="Mining")
	FMiningSiteLevelChangedSignature OnSiteLevelChanged;

	UPROPERTY(BlueprintAssignable, Category="Mining")
	FMiningSiteStorageChangedSignature OnStorageChanged;

	UPROPERTY(BlueprintAssignable, Category="Mining")
	FMiningSiteShipmentResolvedSignature OnShipmentResolved;

	UPROPERTY(BlueprintAssignable, Category="Mining|Workforce")
	FMiningWorkforceChangedSignature OnWorkforceChanged;

	UPROPERTY(BlueprintAssignable, Category="Mining|Workforce")
	FMiningDayEndedSignature OnMiningDayEnded;

	UFUNCTION(BlueprintPure, Category="Mining|Workforce")
	int32 GetLivingWorkerCount() const;

	UFUNCTION(BlueprintPure, Category="Mining|Workforce")
	int32 GetWorkerVacancies() const { return WorkerVacancies; }

	UFUNCTION(BlueprintPure, Category="Mining|Workforce")
	double GetMiningDayElapsedSeconds() const { return MiningDayElapsedSeconds; }

	UFUNCTION(BlueprintPure, Category="Mining|Workforce")
	int64 GetCompletedMiningDays() const { return CompletedMiningDays; }

	UFUNCTION(BlueprintPure, Category="Mining|Workforce")
	bool IsSiteAttended() const;

	UFUNCTION(BlueprintPure, Category="Mining|Workforce")
	bool CanRecruitWorker(UObject* CostContext) const;

	/** Authority only; the caller must validate the requesting player and distance. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining|Workforce")
	bool RecruitWorker(UObject* CostContext);

	/** Called by the owned worker's ACF OnDeath, never by presentation culling/OnDestroyed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mining|Workforce")
	bool RecordWorkerDeath();

	UFUNCTION(BlueprintPure, Category="Mining")
	bool IsEstablished() const { return bEstablished; }

	UFUNCTION(BlueprintPure, Category="Mining")
	int32 GetCurrentLevel() const { return CurrentLevel; }

	UFUNCTION(BlueprintPure, Category="Mining")
	int32 GetStorageCapacity() const;

	UFUNCTION(BlueprintPure, Category="Mining")
	int32 GetStoredUnits() const;

	UFUNCTION(BlueprintPure, Category="Mining")
	int32 GetAvailableStorage() const;

	UFUNCTION(BlueprintPure, Category="Mining")
	int32 GetStoredQuantity(const FMiningItemQuantity& Item) const;

	UFUNCTION(BlueprintPure, Category="Mining")
	float GetManualMiningSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Mining")
	bool GetCurrentLevelDefinition(FMiningSiteLevelDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category="Mining")
	bool GetNextLevelDefinition(FMiningSiteLevelDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category="Mining")
	void GetNextUpgradeCost(TArray<FMiningItemQuantity>& OutCost) const;

	UFUNCTION(BlueprintCallable, Category="Mining")
	bool EstablishSite();

	UFUNCTION(BlueprintPure, Category="Mining")
	bool CanUpgradeSite() const;

	UFUNCTION(BlueprintCallable, Category="Mining")
	bool UpgradeSite();

	UFUNCTION(BlueprintPure, Category="Mining")
	bool CanPurchaseUpgrade(UObject* UpgradeContext) const;

	UFUNCTION(BlueprintCallable, Category="Mining")
	bool PurchaseUpgrade(UObject* UpgradeContext);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Mining|Costs")
	bool CanPayUpgradeCost(UObject* UpgradeContext, const TArray<FMiningItemQuantity>& Cost) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Mining|Costs")
	bool ConsumeUpgradeCost(UObject* UpgradeContext, const TArray<FMiningItemQuantity>& Cost);

	UFUNCTION(BlueprintCallable, Category="Mining")
	int32 AddStoredItem(const FMiningItemQuantity& Item);

	UFUNCTION(BlueprintCallable, Category="Mining")
	int32 RemoveStoredItem(const FMiningItemQuantity& Item);

	/** Adds explicit simulation time; does not replace/advance the online world clock. */
	UFUNCTION(BlueprintCallable, Category="Mining")
	int32 ProduceForElapsedSeconds(float ElapsedSeconds);

	UFUNCTION(BlueprintCallable, Category="Mining")
	bool TryAutoShipment();


	UFUNCTION(BlueprintCallable, Category="Mining")
	void SyncProductionFromWorldTime();

	UFUNCTION(BlueprintCallable, Category="Mining|Storage")
	void SetLinkedStorageComponent(UACFStorageComponent* InStorageComponent);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining")
	bool bEstablished = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining")
	int32 CurrentLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining")
	TArray<FMiningStoredItem> StoredItems;

	// Legacy archive fields only. Session timestamps cannot be interpreted after loading.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining")
	double LastProductionWorldSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Mining")
	double LastShipmentWorldSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining|Timing")
	double ProductionRemainder = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining|Timing")
	double ShipmentCooldownRemaining = 0.0;

	UPROPERTY(SaveGame)
	int32 MiningTimingVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining|Workforce")
	int32 WorkerVacancies = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining|Workforce")
	double MiningDayElapsedSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Mining|Workforce")
	int64 CompletedMiningDays = 0;

	UPROPERTY(SaveGame)
	int32 WorkerRiskSeed = 0;

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
