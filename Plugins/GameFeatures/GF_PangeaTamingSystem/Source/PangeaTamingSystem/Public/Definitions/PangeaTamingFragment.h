#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaDefinitionFragment.h"
#include "PangeaTamingFragment.generated.h"

class UTameSpeciesConfig;

/**
 * Creature definition fragment that carries the taming rules of a species, so a
 * UPangeaTamingComponent can resolve its config from the owner's creature definition.
 */
UCLASS(BlueprintType, EditInlineNew)
class PANGEATAMINGSYSTEM_API UPangeaTamingFragment : public UPangeaDefinitionFragment
{
	GENERATED_BODY()

public:
	/** Taming rules used by creatures built from this definition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming")
	TObjectPtr<UTameSpeciesConfig> TameSpeciesConfig;
};
