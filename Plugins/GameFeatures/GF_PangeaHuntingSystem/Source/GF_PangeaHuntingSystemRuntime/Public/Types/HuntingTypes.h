#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HuntingTypes.generated.h"

/** Kind of evidence a creature leaves behind for a hunter to find and identify. */
UENUM(BlueprintType)
enum class EHuntClueType : uint8
{
	/** Regular track point left while the creature moves. Shown by the hunter sense as a pooled mesh. */
	Footprint UMETA(DisplayName = "Footprint"),
	/** Blood left by a creature whose health is at or below the species blood threshold. */
	Blood UMETA(DisplayName = "Blood"),
	/** Vegetation damaged by a sprinting creature. */
	BrokenFoliage UMETA(DisplayName = "Broken Foliage"),
	/** Leftover food, configured through the species additional special clues. */
	EatenFood UMETA(DisplayName = "Eaten Food"),
	/** Scent trail, configured through the species additional special clues. Has no identification flag. */
	Scent UMETA(DisplayName = "Scent"),
	/** Droppings, configured through the species additional special clues. */
	Droppings UMETA(DisplayName = "Droppings"),
	/** Project-defined clue with no dedicated handling. Also the default for unassigned slots. */
	Custom UMETA(DisplayName = "Custom")
};

/**
 * One replicated entry of a creature's trail. The track emitter records these on the
 * server and the hunter sense reads them on the client to place and fade the visuals.
 */
USTRUCT(BlueprintType)
struct GF_PANGEAHUNTINGSYSTEMRUNTIME_API FHuntTrackPoint
{
	GENERATED_BODY()

	/** World location of the track, snapped to the ground when a ground hit was found. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	FVector_NetQuantize Location = FVector::ZeroVector;

	/** World rotation of the track, aligned to the ground normal when a ground hit was found. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	FRotator Rotation = FRotator::ZeroRotator;

	/** World time in seconds on the server when the track was created. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	float CreatedServerTime = 0.f;

	/** Seconds the track stays valid after creation. Zero or less means it never expires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	float Lifetime = 60.f;

	/** Kind of clue this track point represents. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	EHuntClueType ClueType = EHuntClueType::Footprint;

	/** Gameplay tag named after the physical material under the creature, when such a tag exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	FGameplayTag SurfaceTag;

	/** Gait of the creature when the track was left. Not written by the track emitter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	FGameplayTag GaitTag;

	/** Scale multiplier for the track visual. Raised while the creature sprints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunting")
	float Strength = 1.f;
};
