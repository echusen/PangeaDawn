#pragma once

#include "CoreMinimal.h"

#include "Components/ACFStorageComponent.h"

#include "PangeaMiningTransportStorageComponent.generated.h"

/** Supported ACF subclass exposing the actual accepted quantity, not inventory-count guesses. */
UCLASS(ClassGroup = (Pangea), BlueprintType, meta = (BlueprintSpawnableComponent))
class GF_PANGEAMININGSYSTEMRUNTIME_API UPangeaMiningTransportStorageComponent : public UACFStorageComponent
{
	GENERATED_BODY()
public:
	/** Authority only. Adds the shipment items to this storage and returns how many were actually accepted (0 to ShipmentItems.Count). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Shipments")
	int32 AcceptShipmentItems(const FBaseItem& ShipmentItems);

private:
	bool bAcceptingShipment = false;
};
