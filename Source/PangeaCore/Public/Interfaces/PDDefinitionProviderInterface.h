#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PDDefinitionProviderInterface.generated.h"

class UPangeaCreatureDefinition;

UINTERFACE(BlueprintType)
class UPDDefinitionProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class PANGEACORE_API IPDDefinitionProviderInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Definition")
	UPangeaCreatureDefinition* GetCreatureDefinition() const;

	/** Assign before component initialization (use deferred spawning at runtime). Returns false if unsupported or too late. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintAuthorityOnly, Category="Definition")
	bool SetCreatureDefinition(UPangeaCreatureDefinition* Definition);
};
