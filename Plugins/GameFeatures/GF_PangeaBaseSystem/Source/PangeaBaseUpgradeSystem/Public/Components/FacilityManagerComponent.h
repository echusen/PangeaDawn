#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "FacilityManagerComponent.generated.h"

class AFacilityGroup;

/** Runtime record of one facility group attached to a base: its tag, its actor and whether it is unlocked. */
USTRUCT()
struct FFacilityEntry
{
	GENERATED_BODY()

	/** Tag copied from the facility group when it was discovered. */
	UPROPERTY()
	FGameplayTag FacilityTag;

	/** Facility group actor. Weak, because the group can be unloaded or destroyed. */
	UPROPERTY()
	TWeakObjectPtr<AFacilityGroup> Group;

	/** True after the facility was enabled. Reset to false each time groups are discovered again. */
	UPROPERTY()
	bool bUnlocked = false;
};

/**
 * Component that tracks the facility groups attached to its owner and enables or disables them by tag.
 * It gives upgrade actions and requirements one place to change and read facility state.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Pangea), meta = (BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UPangeaFacilityManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPangeaFacilityManagerComponent();

	virtual void BeginPlay() override;

	/** Rebuild transient attached-facility references before restoring absolute unlock state. */
	UFUNCTION(BlueprintCallable, Category = "Facilities")
	void DiscoverFacilityGroups();

	/* Enable/disable a facility by tag */
	void EnableFacility(const FGameplayTag& FacilityTag);
	void DisableFacility(const FGameplayTag& FacilityTag);

	/* Check state */
	bool IsFacilityEnabled(const FGameplayTag& FacilityTag) const;

	//Get all facilities
	const TArray<FFacilityEntry>& GetAllFacilities() const
	{
		return Facilities;
	}

protected:
	/* Storage */
	UPROPERTY()
	TArray<FFacilityEntry> Facilities;
};
