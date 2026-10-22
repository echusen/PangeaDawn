#pragma once

#include "CoreMinimal.h"
#include "ACFGASStatisticsComponent.h"
#include "PangeaGASStatisticsComponent.generated.h"

/** ACF's profile owns the initial stat pass; legacy creatures retain their auto-init setting. */
UCLASS(BlueprintType, ClassGroup=(Pangea))
class PANGEADINOSAURAI_API UPangeaGASStatisticsComponent : public UACFGASStatisticsComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
