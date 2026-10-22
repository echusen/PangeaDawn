#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "FacilityManagerComponent.generated.h"


class AFacilityGroup;

USTRUCT()
struct FFacilityEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag FacilityTag;

	UPROPERTY()
	TWeakObjectPtr<AFacilityGroup> Group;

	UPROPERTY()
	bool bUnlocked = false;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Pangea), meta=(BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UPangeaFacilityManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPangeaFacilityManagerComponent();

	virtual void BeginPlay() override;

	/** Rebuild transient attached-facility references before restoring absolute unlock state. */
	UFUNCTION(BlueprintCallable, Category="Facilities")
	void DiscoverFacilityGroups();

	/* Enable/disable a facility by tag */
	void EnableFacility(const FGameplayTag& FacilityTag);
	void DisableFacility(const FGameplayTag& FacilityTag);

	/* Check state */
	bool IsFacilityEnabled(const FGameplayTag& FacilityTag) const;
	
	//Get all facilities
	const TArray<FFacilityEntry>& GetAllFacilities() const { return Facilities; }

protected:

	/* Storage */
	UPROPERTY()
	TArray<FFacilityEntry> Facilities;
};
