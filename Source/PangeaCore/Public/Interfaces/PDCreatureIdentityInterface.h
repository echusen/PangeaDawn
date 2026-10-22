#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PDCreatureIdentityInterface.generated.h"

UINTERFACE(BlueprintType)
class UPDCreatureIdentityInterface : public UInterface
{
	GENERATED_BODY()
};

/** Actor-owned identity outlives optional Game Feature components. Queries never create a new identity. */
class PANGEACORE_API IPDCreatureIdentityInterface
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Identity")
	FGuid GetCreatureId() const;
};
