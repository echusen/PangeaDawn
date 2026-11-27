#pragma once

#include "CoreMinimal.h"
#include "Definitions/BaseUpgradeFragment.h"
#include "BaseSaveRulesFragment.generated.h"

/** Optional fragment that controls how a base restores its upgrade state after a save is loaded. */
UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UBaseSaveRulesFragment : public UBaseUpgradeFragment
{
	GENERATED_BODY()

public:
	/** When true, load runs the reconstructable actions of completed milestones again. When false, load skips that step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save")
	bool bReplayCompletedMilestonesOnLoad = true;
};
