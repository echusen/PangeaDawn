#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FacilityPresetData.generated.h"

USTRUCT(BlueprintType)
struct FFacilityActorSlot
{
	GENERATED_BODY()

	/** Descriptive name for this slot (NPC, Anvil, Chest, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName SlotName;

	/** Class to spawn for this slot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AActor> ActorClass;

	/** Local offset from the marker (in marker's local space) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Offset = FVector::ZeroVector;

	/** Optional rotation override; if zero, marker rotation is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FRotator Rotation = FRotator::ZeroRotator;

	/** Try to snap this actor to the ground below its spawn point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bSnapToGround = true;

	/** Hide this actor until the facility is unlocked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bHideUntilUnlocked = true;
};

UCLASS(BlueprintType)
class PANGEABASEUPGRADESYSTEM_API UFacilityPresetData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Facility this preset represents (e.g. Test.Village.Blacksmith) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag FacilityTag;

	/** Should this facility start unlocked? */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bStartsUnlocked = false;

	/** Set of slot definitions for this facility */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FFacilityActorSlot> Slots;
};