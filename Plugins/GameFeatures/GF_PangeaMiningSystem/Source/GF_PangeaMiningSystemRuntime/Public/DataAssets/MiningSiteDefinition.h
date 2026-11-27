#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "Types/MiningTypes.h"
#include "MiningSiteDefinition.generated.h"

class AActor;
class UPrimaryDataAsset;
class UUserWidget;

/**
 * Authored description of one type of mining site: identity, per-level balance, storage rules,
 * UI classes and presentation config. Keeps site tuning in data so designers can add sites without code.
 */
UCLASS(BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningSiteDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Gameplay tag that identifies this type of mining site. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	FGameplayTag SiteTypeTag;

	/** Name of the site shown to the player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	FText DisplayName;

	/** Settlement milestone tag authored as the requirement for this site. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	FGameplayTag RequiredSettlementMilestoneTag;

	/** Length of one simulated mining day in seconds. Automated production per day is spread over this time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining", meta = (ClampMin = "1.0"))
	float SimulatedDaySeconds = 86400.0f;

	/** Any possessed player pawn within this radius prevents the day-end unattended loss roll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Workforce", meta = (ClampMin = "1.0", Units = "cm"))
	float WorkerAttendanceRadius = 5000.0f;

	/** Migration gate: leave disabled until physical courier/chariot/escort authoring is complete. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Transport")
	bool bUseHybridTransport = false;

	/** Item tags the site storage accepts. Empty accepts every item, and items without a tag are always accepted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	TArray<FGameplayTag> AllowedStorageItemTags;

	/** Widget class for the prompt that offers to set up the site. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	TSoftClassPtr<UUserWidget> SetupPromptWidgetClass;

	/** Widget class for the menu of the site. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	TSoftClassPtr<UUserWidget> SiteMenuWidgetClass;

	/** Actor class of the chest the site spawns for its storage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Storage")
	TSoftClassPtr<AActor> SiteChestActorClass;

	/** Transform of the site chest relative to the site actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Storage")
	FTransform SiteChestRelativeTransform;

	/** Presentation config asset (a UMiningSitePresentationConfig) for the worker, guard and courier NPCs. Must be assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining|Presentation")
	TSoftObjectPtr<UPrimaryDataAsset> PresentationConfig;

	/** Per-level tuning. Level numbers must be unique and level 0 must exist. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mining")
	TArray<FMiningSiteLevelDefinition> Levels;

	/** Copies the entry whose Level matches into OutDefinition. Returns false when no entry matches. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	bool GetLevelDefinition(int32 Level, FMiningSiteLevelDefinition& OutDefinition) const;

	/** Returns the highest authored level number, or INDEX_NONE when no levels are authored. */
	UFUNCTION(BlueprintPure, Category = "Mining")
	int32 GetMaxLevel() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
