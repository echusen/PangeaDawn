#pragma once

#include "CoreMinimal.h"
#include "FacilityTypes.generated.h"

/** What a facility slot spawns. Decides whether the slot uses its actor class or its decoration asset in the editor. */
UENUM(BlueprintType)
enum class EFacilitySlotType : uint8
{
	/** Slot spawns an NPC from the slot's actor class. */
	NPC UMETA(DisplayName = "NPC"),
	/** Slot spawns a static or skeletal mesh from the slot's decoration asset. */
	Decoration UMETA(DisplayName = "Decoration"),
	/** Slot spawns an interactable actor from the slot's actor class. */
	Interactable UMETA(DisplayName = "Interactable"),
	/** Slot spawns any other actor from the slot's actor class. */
	Custom UMETA(DisplayName = "Custom")
};
