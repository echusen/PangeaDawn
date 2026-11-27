#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaDefinitionFragment.h"
#include "PangeaHuntingFragment.generated.h"

class UHuntSpeciesConfig;

/**
 * Creature definition fragment that carries the hunting configuration of a species.
 * The track emitter reads it when no config was assigned on the component itself.
 */
UCLASS(BlueprintType, EditInlineNew)
class GF_PANGEAHUNTINGSYSTEMRUNTIME_API UPangeaHuntingFragment : public UPangeaDefinitionFragment
{
	GENERATED_BODY()

public:
	/** Hunting configuration used by track emitters on creatures with this definition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting")
	TObjectPtr<UHuntSpeciesConfig> HuntSpeciesConfig;
};
