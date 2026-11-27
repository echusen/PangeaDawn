#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BaseUpgradeFragment.generated.h"

class UBaseUpgradeDefinition;

/**
 * Base class for one inline block of data inside a base upgrade definition.
 * Fragments let a definition be composed from optional parts instead of one large asset class.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class PANGEABASEUPGRADESYSTEM_API UBaseUpgradeFragment : public UObject
{
	GENERATED_BODY()

public:
	/** Returns the upgrade definition this fragment is an inline subobject of, or null. */
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	UBaseUpgradeDefinition* GetOwningDefinition() const;
};
