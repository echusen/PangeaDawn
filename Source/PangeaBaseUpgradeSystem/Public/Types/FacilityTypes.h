#pragma once

#include "CoreMinimal.h"
#include "FacilityTypes.generated.h"

UENUM(BlueprintType)
enum class EFacilitySlotType : uint8
{
	NPC         UMETA(DisplayName="NPC"),
	Decoration  UMETA(DisplayName="Decoration"),
	Interactable UMETA(DisplayName="Interactable"),
	Custom      UMETA(DisplayName="Custom")
};