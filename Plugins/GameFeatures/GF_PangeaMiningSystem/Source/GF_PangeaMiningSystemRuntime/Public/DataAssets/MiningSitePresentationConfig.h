#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Misc/DataValidation.h"

#include "Interfaces/MiningPresentationAgentInterface.h"

#include "MiningSitePresentationConfig.generated.h"

class AACFAIController;
class AActor;
class UAnimSequenceBase;
class USmartObjectDefinition;

/** One duty station of a presentation NPC: the two named site markers it moves between. */
USTRUCT(BlueprintType)
struct GF_PANGEAMININGSYSTEMRUNTIME_API FMiningPresentationStation
{
	GENERATED_BODY()

	/** Name of the scene component on the site actor that marks the first stop of the route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	FName PrimaryMarkerName;

	/** Name of the scene component on the site actor that marks the second stop of the route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	FName SecondaryMarkerName;
};

/** Presentation settings shared by every NPC of one role (worker, guard or courier) at a mining site. */
USTRUCT(BlueprintType)
struct GF_PANGEAMININGSYSTEMRUNTIME_API FMiningPresentationRoleConfig
{
	GENERATED_BODY()

	/** Role these settings describe. Must match the config slot that holds them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	EMiningPresentationRole Role = EMiningPresentationRole::Worker;

	/** When set, replaces the NPC actor class authored on the site level for this role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	TSoftClassPtr<AActor> ActorClassOverride;

	/** Smart object definition for the stations of this role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	TSoftObjectPtr<USmartObjectDefinition> SmartObjectDefinition;

	/** Seconds an NPC spends at a station. Zero uses the coordinator component's duration for the role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation", meta = (ClampMin = "0.0"))
	float InteractionDuration = 0.0f;

	/** Maximum per-actor offset in seconds added to or subtracted from the interaction duration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation", meta = (ClampMin = "0.0"))
	float InteractionDurationVariance = 0.0f;

	/** Authored travel speed for NPCs of this role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation", meta = (ClampMin = "0.0"))
	float TravelSpeed = 0.0f;

	/** Looping animation authored for an NPC of this role while it occupies a station. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	TSoftObjectPtr<UAnimSequenceBase> OccupiedLoopAnimation;

	/** Duty stations of this role, indexed by NPC. The courier uses only the first entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	TArray<FMiningPresentationStation> Stations;
};

/**
 * Authored presentation setup for the cosmetic worker, guard and courier NPCs of a mining site.
 * Referenced from UMiningSiteDefinition::PresentationConfig and read by the presentation coordinator component.
 */
UCLASS(BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningSitePresentationConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Opt-in migration gate. Legacy assets retain their behavior until explicitly migrated after a full build. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bUseACFRoutineJobs = false;

	/** Reuse an ACF routine controller with a routine component, Behavior Tree and AIState.Routine default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (EditCondition = "bUseACFRoutineJobs"))
	TSoftClassPtr<AACFAIController> RoutineControllerClass;

	/** Navigation timeout in seconds passed to each ACF routine job assignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "1.0", Units = "s", EditCondition = "bUseACFRoutineJobs"))
	float NavigationTimeoutSeconds = 120.0f;

	/** Blocked-retry delay in seconds passed to each ACF routine job assignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (ClampMin = "0.1", Units = "s", EditCondition = "bUseACFRoutineJobs"))
	float BlockedRetryDelaySeconds = 3.0f;

	/** Presentation settings for worker NPCs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	FMiningPresentationRoleConfig WorkerRole;

	/** Presentation settings for guard NPCs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	FMiningPresentationRoleConfig GuardRole;

	/** Presentation settings for the courier NPC. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mining|Presentation")
	FMiningPresentationRoleConfig CourierRole;

	/** Returns the settings for the role, or nullptr for an unknown role value. */
	const FMiningPresentationRoleConfig* FindRoleConfig(EMiningPresentationRole Role) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
