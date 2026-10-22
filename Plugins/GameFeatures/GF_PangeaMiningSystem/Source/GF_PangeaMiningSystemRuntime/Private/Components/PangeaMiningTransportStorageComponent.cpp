#include "Components/PangeaMiningTransportStorageComponent.h"
#include "GameFramework/Actor.h"

int32 UPangeaMiningTransportStorageComponent::AcceptShipmentItems(const FBaseItem& ShipmentItems)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bAcceptingShipment || !ShipmentItems.ItemClass || ShipmentItems.Count <= 0) return 0;
	TGuardValue<bool> Guard(bAcceptingShipment, true);
	// Internal_AddItem returns its accepted count even when inventory delegates
	// immediately transfer/consume some of the newly added items. A before/after
	// total-count delta would incorrectly put those items back into escrow.
	return FMath::Clamp(Internal_AddItem(ShipmentItems, false), 0, ShipmentItems.Count);
}
