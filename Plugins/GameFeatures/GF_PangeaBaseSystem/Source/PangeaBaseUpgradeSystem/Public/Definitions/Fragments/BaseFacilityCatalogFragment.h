#pragma once

#include "CoreMinimal.h"
#include "Definitions/BaseUpgradeFragment.h"
#include "Types/BaseUpgradeTypes.h"
#include "BaseFacilityCatalogFragment.generated.h"

/** Fragment that maps facility tags to display names, so the UI can show a name instead of a raw tag. */
UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UBaseFacilityCatalogFragment : public UBaseUpgradeFragment
{
	GENERATED_BODY()

public:
	/** Facilities of this base with the name shown for each facility tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility")
	TArray<FFacilityGroupReference> FacilityGroups;
};
