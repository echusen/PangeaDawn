#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaDefinitionFragment.h"
#include "PangeaMiningFragment.generated.h"

class UMiningSiteDefinition;

/** Definition fragment that attaches a mining site definition to a Pangea definition asset. */
UCLASS(BlueprintType, EditInlineNew)
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningFragment : public UPangeaDefinitionFragment
{
	GENERATED_BODY()

public:
	/** Mining site definition carried by the owning definition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	TObjectPtr<UMiningSiteDefinition> MiningSiteDefinition;
};
