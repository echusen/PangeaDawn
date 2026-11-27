#pragma once

#include "CoreMinimal.h"
#include "Definitions/BaseUpgradeFragment.h"
#include "Types/BaseUpgradeTypes.h"
#include "BaseProgressionFragment.generated.h"

/** Fragment that holds the upgrade levels of a base. A valid definition has exactly one. */
UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UBaseProgressionFragment : public UBaseUpgradeFragment
{
	GENERATED_BODY()

public:
	/** Authored levels. Level numbers must be consecutive and start at 1. Storage order does not matter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FUpgradeLevelDefinition> Levels;
};
