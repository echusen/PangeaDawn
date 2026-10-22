#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "Types/MiningTypes.h"
#include "MiningSiteDefinition.generated.h"

class UUserWidget;
class AActor;
class UPrimaryDataAsset;

UCLASS(BlueprintType)
class GF_PANGEAMININGSYSTEMRUNTIME_API UMiningSiteDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining")
	FGameplayTag SiteTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining")
	FGameplayTag RequiredSettlementMilestoneTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining", meta=(ClampMin="1.0"))
	float SimulatedDaySeconds = 86400.0f;

	/** Any possessed player pawn within this radius prevents the day-end unattended loss roll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining|Workforce", meta=(ClampMin="1.0", Units="cm"))
	float WorkerAttendanceRadius = 5000.0f;

	/** Migration gate: leave disabled until physical courier/chariot/escort authoring is complete. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining|Transport")
	bool bUseHybridTransport = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining")
	TArray<FGameplayTag> AllowedStorageItemTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining")
	TSoftClassPtr<UUserWidget> SetupPromptWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining")
	TSoftClassPtr<UUserWidget> SiteMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining|Storage")
	TSoftClassPtr<AActor> SiteChestActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining|Storage")
	FTransform SiteChestRelativeTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining|Presentation")
	TSoftObjectPtr<UPrimaryDataAsset> PresentationConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mining")
	TArray<FMiningSiteLevelDefinition> Levels;

	UFUNCTION(BlueprintPure, Category="Mining")
	bool GetLevelDefinition(int32 Level, FMiningSiteLevelDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category="Mining")
	int32 GetMaxLevel() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
