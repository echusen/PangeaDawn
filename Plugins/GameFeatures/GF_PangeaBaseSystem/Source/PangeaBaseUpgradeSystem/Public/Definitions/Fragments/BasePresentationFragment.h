#pragma once

#include "CoreMinimal.h"
#include "Definitions/BaseUpgradeFragment.h"
#include "BasePresentationFragment.generated.h"

/** Fragment with player-facing presentation data for a base, kept apart from progression rules. */
UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UBasePresentationFragment : public UBaseUpgradeFragment
{
	GENERATED_BODY()

public:
	/** Interaction prompt of the village. Replaces the village's own text at BeginPlay when not empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FText InteractionText;
};
