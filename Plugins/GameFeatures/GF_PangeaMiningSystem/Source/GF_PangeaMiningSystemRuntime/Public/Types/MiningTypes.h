#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "MiningTypes.generated.h"

class AActor;

/** An amount of one item, identified by class and/or gameplay tag. Used for costs and production output. */
USTRUCT(BlueprintType)
struct GF_PANGEAMININGSYSTEMRUNTIME_API FMiningItemQuantity
{
	GENERATED_BODY()

	/** Class of the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	TSubclassOf<UObject> ItemClass;

	/** Gameplay tag of the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining")
	FGameplayTag ItemTag;

	/** Number of units of the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining", meta = (ClampMin = "0"))
	int32 Quantity = 0;
};

/** One entry of a mining site's storage. Same shape as FMiningItemQuantity, but its fields are saved with the game. */
USTRUCT(BlueprintType)
struct GF_PANGEAMININGSYSTEMRUNTIME_API FMiningStoredItem
{
	GENERATED_BODY()

	/** Class of the stored item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Mining")
	TSubclassOf<UObject> ItemClass;

	/** Gameplay tag of the stored item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Mining")
	FGameplayTag ItemTag;

	/** Number of units stored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Mining", meta = (ClampMin = "0"))
	int32 Quantity = 0;
};

/** Tuning for one upgrade level of a mining site: storage, production, workforce, shipments, transport and NPC classes. */
USTRUCT(BlueprintType)
struct GF_PANGEAMININGSYSTEMRUNTIME_API FMiningSiteLevelDefinition
{
	GENERATED_BODY()

	/** Level number this entry describes. Must be unique within the site definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Level", meta = (ClampMin = "0"))
	int32 Level = 0;

	/** Storage capacity of the site at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Level", meta = (ClampMin = "0"))
	int32 StorageCapacity = 0;

	/** Manual mining speed multiplier the site reports at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Manual", meta = (ClampMin = "0.0"))
	float ManualMiningSpeedMultiplier = 1.0f;

	/** Units of the primary output produced per simulated day at full staffing. Zero disables automated production. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Automation", meta = (ClampMin = "0"))
	int32 AutomatedMineralsPerDay = 0;

	/** Number of worker positions at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Automation", meta = (ClampMin = "0"))
	int32 WorkerCount = 0;

	/** Number of guards at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Automation", meta = (ClampMin = "0"))
	int32 GuardCount = 0;

	/** Chance (0 to 1) used for the worker loss roll while the site is unattended. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Automation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WorkerDeathChance = 0.0f;

	/** Cost to replace ONE vacant miner. Empty means recruitment is not configured, not free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Workforce")
	TArray<FMiningItemQuantity> WorkerRecruitmentCost;

	/** Whether the site sends shipments at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Shipment")
	bool bShipmentUnlocked = false;

	/** Maximum number of units carried by one shipment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Shipment", meta = (ClampMin = "0"))
	int32 MaxShipmentUnits = 0;

	/** Seconds between shipments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Shipment", meta = (ClampMin = "0.0"))
	float ShipmentIntervalSeconds = 3600.0f;

	/** Chance (0 to 1) that a shipment is lost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Shipment", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShipmentLossChance = 0.0f;

	/** Virtual travel uses the same authored ground speed as the physical transport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Transport", meta = (ClampMin = "1.0", Units = "cm/s"))
	float TransportTravelSpeed = 320.f;

	/** Number of escorts that accompany a hybrid transport shipment. Valid range is 0 to 8. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Transport", meta = (ClampMin = "0"))
	int32 TransportEscortCount = 0;

	/** Cart actor class for a hybrid transport shipment. Required when a draft animal class is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Transport")
	TSoftClassPtr<AActor> TransportChariotClass;

	/** Optional ACF draft animal. It navigates independently and tows the cosmetic cart.
	 * The human courier leads on foot; this does not enable player/AI riding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Transport")
	TSoftClassPtr<AActor> TransportDraftAnimalClass;

	/** Items paid to upgrade the site to this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Cost")
	TArray<FMiningItemQuantity> UpgradeCost;

	/** Item the site produces and ships at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Output")
	FMiningItemQuantity PrimaryOutput;

	/** Actor class of the visual set shown for the site at this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Visuals")
	TSoftClassPtr<AActor> VisualSetClass;

	/** Actor class of the worker NPCs, unless the presentation config overrides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|NPC")
	TSoftClassPtr<AActor> WorkerClass;

	/** Actor class of the guard NPCs, unless the presentation config overrides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|NPC")
	TSoftClassPtr<AActor> GuardClass;

	/** Actor class of the courier NPC, unless the presentation config overrides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|NPC")
	TSoftClassPtr<AActor> CourierClass;

	/** State tree asset authored for the worker NPCs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|NPC")
	TSoftObjectPtr<UObject> WorkerStateTreeAsset;

	/** State tree asset authored for the courier NPC. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|NPC")
	TSoftObjectPtr<UObject> CourierStateTreeAsset;
};
