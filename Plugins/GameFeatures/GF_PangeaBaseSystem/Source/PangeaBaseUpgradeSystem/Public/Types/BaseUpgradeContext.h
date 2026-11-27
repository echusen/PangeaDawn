#pragma once

#include "CoreMinimal.h"
#include "BaseUpgradeContext.generated.h"

class APawn;
class APlayerController;
class AVillageBase;
class UPangeaFacilityManagerComponent;
class UPangeaUpgradeSystemComponent;

/**
 * Everything a requirement or action can need to evaluate one upgrade command.
 * Built once per command so requirements and actions do not each resolve the player and the base again.
 */
USTRUCT(BlueprintType)
struct PANGEABASEUPGRADESYSTEM_API FBaseUpgradeContext
{
	GENERATED_BODY()

	/** Object the command was started with. Falls back to the owner of the upgrade component. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UObject> SourceObject = nullptr;

	/** Village that owns the upgrade component, when the owner is a village. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<AVillageBase> Village = nullptr;

	/** Pawn of the player who started the command. Null during a normal save load. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<APawn> InteractingPawn = nullptr;

	/** Player controller of the interacting player, when one was resolved. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<APlayerController> PlayerController = nullptr;

	/** Upgrade component that runs the command. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UPangeaUpgradeSystemComponent> UpgradeSystem = nullptr;

	/** Facility manager on the same owner as the upgrade component, when there is one. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	TObjectPtr<UPangeaFacilityManagerComponent> FacilityManager = nullptr;

	/** Object for legacy requirement checks: the interacting pawn, else the village, else the source object. */
	UObject* GetPreferredRequirementObject() const;

	/** Object for legacy action execution: the village, else the interacting pawn, else the source object. */
	UObject* GetPreferredActionObject() const;
};
