#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/HuntingTypes.h"
#include "HuntSpeciesConfig.generated.h"

class AActor;
class APangeaHuntClueActor;
class UCurveFloat;
class UMaterialInterface;
class UStaticMesh;
class UUserWidget;

/**
 * Spawn rules and presentation of one extra clue type (eaten food, droppings, scent, custom)
 * that a species can leave in addition to footprints, blood and broken foliage.
 */
USTRUCT(BlueprintType)
struct GF_PANGEAHUNTINGSYSTEMRUNTIME_API FHuntSpecialClueConfig
{
	GENERATED_BODY()

	/** Clue type this entry configures. Footprint, Blood and Broken Foliage entries are never picked for spawning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting")
	EHuntClueType ClueType = EHuntClueType::Custom;

	/** Disabled entries are never picked for spawning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting")
	bool bEnabled = true;

	/** When true, an eligible track rolls ChancePerEligibleTrack. When false, every eligible track spawns this clue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting")
	bool bUseRandomChance = true;

	/** Probability from 0 to 1 that an eligible track becomes this clue. Used only with bUseRandomChance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChancePerEligibleTrack = 0.08f;

	/** Lower bound in seconds of the time that must pass since the creature's last special clue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting", meta = (ClampMin = "0.0"))
	float MinInterval = 15.f;

	/** Upper bound in seconds of that time. The required interval is rolled between MinInterval and this value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting", meta = (ClampMin = "0.0"))
	float MaxInterval = 35.f;

	/** Seconds this clue stays in the world. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting", meta = (ClampMin = "1.0"))
	float Lifetime = 300.f;

	/** Decal material of the clue. The clue actor uses its fallback decal material when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting")
	TObjectPtr<UMaterialInterface> DecalMaterial;

	/** Decal extent of the clue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting")
	FVector DecalSize = FVector(16.f, 72.f, 72.f);

	/** Horizontal distance within which a pawn can interact with the clue. Never smaller than the decal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting", meta = (ClampMin = "1.0"))
	float InteractionRadius = 88.f;
};

/**
 * Per-species tuning of the hunting loop: how a creature leaves tracks and special clues,
 * how far away they are revealed and how the footprint focus identification behaves.
 */
UCLASS(BlueprintType)
class GF_PANGEAHUNTINGSYSTEMRUNTIME_API UHuntSpeciesConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** When false, creatures using this config record no tracks at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	bool bLeavesTracks = true;

	/** Horizontal distance the creature must travel from its last track before leaving a new one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tracks", meta = (ClampMin = "1.0"))
	float MinDistanceBetweenTracks = 150.f;

	/** Horizontal speed below which the creature leaves no tracks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tracks", meta = (ClampMin = "0.0"))
	float MinSpeedToLeaveTracks = 50.f;

	/** Seconds a footprint stays valid. Also used for extra clue types that have no entry in AdditionalSpecialClues. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tracks", meta = (ClampMin = "1.0"))
	float TrackLifetime = 120.f;

	/** Maximum number of track points kept per creature. The oldest points are dropped first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tracks", meta = (ClampMin = "1"))
	int32 MaxTrackPoints = 32;

	/** Distance a track is lifted off the ground to avoid z-fighting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	float TrackZOffset = 5.f;

	/** Sideways distance of each track from the creature's center line. Alternates left and right. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tracks")
	float FootLateralOffset = 28.f;

	/** Distance from the hunter within which this creature's tracks are revealed, before hunter bonuses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Reveal", meta = (ClampMin = "100.0"))
	float RevealRadius = 2500.f;

	/** Optional remap of track freshness (1 is new, 0 is expired) to visibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Reveal")
	TObjectPtr<UCurveFloat> FreshnessVisibilityCurve;

	/** Actor class for footprint visuals. Not read by the hunting C++ code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Visuals")
	TSubclassOf<AActor> FootprintVisualActorClass;

	/** Decal material for footprints. Not read by the hunting C++ code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Visuals")
	TObjectPtr<UMaterialInterface> FootprintDecalMaterial;

	/** Mesh the hunter sense uses for each footprint. The engine plane is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Visuals")
	TObjectPtr<UStaticMesh> FootprintStaticMesh;

	/** Material for the footprint mesh. The plugin track material is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Visuals")
	TObjectPtr<UMaterialInterface> FootprintStaticMeshMaterial;

	/** Horizontal scale of the footprint mesh, multiplied by the strength of each track point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Visuals", meta = (ClampMin = "0.01"))
	float FootprintVisualScale = 0.35f;

	/** When true, a wounded creature leaves blood clues. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues")
	bool bSpawnBloodClues = true;

	/** When true, a sprinting creature leaves broken foliage clues. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues")
	bool bSpawnBrokenFoliageClues = true;

	/** Normalized health at or below which the creature leaves blood clues. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BloodHealthThreshold = 0.55f;

	/** Minimum seconds between two special clues of any type from the same creature. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues", meta = (ClampMin = "0.1"))
	float MinSpecialClueInterval = 4.0f;

	/** When true, the required interval is rolled between MinSpecialClueInterval and MaxSpecialClueInterval. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues")
	bool bUseRandomSpecialClueInterval = false;

	/** Upper bound in seconds of the random special clue interval. */
	UPROPERTY(
		EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues", meta = (ClampMin = "0.1", EditCondition = "bUseRandomSpecialClueInterval"))
	float MaxSpecialClueInterval = 12.0f;

	/** Extra clue types this species can leave. Checked in order after blood and broken foliage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues")
	TArray<FHuntSpecialClueConfig> AdditionalSpecialClues;

	/** Actor class spawned for every clue that is not a footprint. The native clue actor is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues")
	TSubclassOf<APangeaHuntClueActor> SpecialClueActorClass;

	/** Decal material of blood clues. The clue actor uses its fallback blood material when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Blood")
	TObjectPtr<UMaterialInterface> BloodDecalMaterial;

	/** Decal extent of blood clues. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Blood")
	FVector BloodDecalSize = FVector(16.f, 72.f, 72.f);

	/** Seconds a blood clue stays in the world. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Blood", meta = (ClampMin = "1.0"))
	float BloodClueLifetime = 420.f;

	/** Horizontal distance within which a pawn can interact with a blood clue. Never smaller than the decal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Blood", meta = (ClampMin = "1.0"))
	float BloodInteractionRadius = 88.f;

	/** Decal material of broken foliage clues. The clue actor uses its fallback foliage material when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Broken Foliage")
	TObjectPtr<UMaterialInterface> BrokenFoliageDecalMaterial;

	/** Decal extent of broken foliage clues. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Broken Foliage")
	FVector BrokenFoliageDecalSize = FVector(16.f, 96.f, 96.f);

	/** Seconds a broken foliage clue stays in the world. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Broken Foliage", meta = (ClampMin = "1.0"))
	float BrokenFoliageClueLifetime = 240.f;

	/** Horizontal distance within which a pawn can interact with a broken foliage clue. Never smaller than the decal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|SpecialClues|Broken Foliage", meta = (ClampMin = "1.0"))
	float BrokenFoliageInteractionRadius = 96.f;

	/** Whether footprints of this species can be identified by focusing them. Replaces the hunter sense setting while this species is focused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Focus")
	bool bEnableFootprintFocusIdentification = true;

	/** Seconds the crosshair must stay on a footprint to fill the focus ring. Replaces the hunter sense setting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Focus", meta = (ClampMin = "0.05"))
	float FootprintFocusDuration = 1.25f;

	/** Screen distance in pixels from the crosshair within which a footprint of this species counts as focused. The hunter sense radius applies as well. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Focus", meta = (ClampMin = "1.0"))
	float FootprintFocusScreenRadius = 48.f;

	/** When true, the footprints are identified as soon as the focus ring is full. Replaces the hunter sense setting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Focus")
	bool bAutoIdentifyFocusedFootprints = true;

	/** Whether the focus ring widget is shown for this species. Replaces the hunter sense setting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Focus")
	bool bShowFootprintFocusWidget = true;

	/** Size of the focus ring widget on the HUD canvas. Replaces the hunter sense setting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Focus")
	FVector2D FootprintFocusWidgetSize = FVector2D(96.f, 96.f);

	/** Offset in pixels of the focus ring widget from the screen center. Replaces the hunter sense setting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Focus")
	FVector2D FootprintFocusWidgetScreenOffset = FVector2D::ZeroVector;

	/** ACF notification list widget class. Not read by the hunting C++ code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Messages")
	TSoftClassPtr<UUserWidget> ACFNotificationsListWidgetClass = TSoftClassPtr<UUserWidget>(
		FSoftClassPath(TEXT("/Game/FullSample/Integrations/UIIntegrations/Widgets/ACF_NotificationsList_WB.ACF_NotificationsList_WB_C")));

	/** Fallback on-screen message widget class. Not read by the hunting C++ code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Messages")
	TSoftClassPtr<UUserWidget> FallbackOnScreenMessageWidgetClass =
		TSoftClassPtr<UUserWidget>(FSoftClassPath(TEXT("/Game/FullSample/UI/ACF_OnScreenMessage_WBP.ACF_OnScreenMessage_WBP_C")));

	/** Notification text shown when the player identifies footprints of this species. No notification when empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Messages")
	FText TracksIdentifiedMessage = FText::FromString(TEXT("Tracks Identified"));

	/** Seconds the identified notification stays on screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Messages", meta = (ClampMin = "0.1"))
	float TracksIdentifiedMessageDuration = 5.f;

	/** Copied to the track emitter at BeginPlay to turn its on-screen debug messages on or off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Debug")
	bool bEnableDebugMessages = false;

	/** Tag describing this species' tracks. Not read by the hunting C++ code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tags")
	FGameplayTag HuntingTrackTag;

	/** Tag a hunter needs to reveal this species' tracks. Not read by the hunting C++ code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunting|Tags")
	FGameplayTag HuntingRevealRequiredTag;
};
