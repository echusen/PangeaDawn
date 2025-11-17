// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeAction.h"
#include "UA_EnableFacility.generated.h"

UENUM(BlueprintType)
enum class EFacilityEnableMode : uint8
{
	RevealExisting UMETA(DisplayName = "Reveal Existing Actors"),
	SpawnNew UMETA(DisplayName = "Spawn New NPCs"),
	Both UMETA(DisplayName = "Reveal + Spawn")
};


/**
 * 
 */
UCLASS(Blueprintable)
class PANGEABASEUPGRADESYSTEM_API UUA_EnableFacility : public UUpgradeAction
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
	FGameplayTag FacilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
	EFacilityEnableMode EnableMode = EFacilityEnableMode::RevealExisting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility", meta = (EditCondition = "EnableMode == EFacilityEnableMode::SpawnNew || EnableMode == EFacilityEnableMode::Both"))
	TArray<TSubclassOf<APawn>> NPCsToSpawn;

	virtual void Execute_Implementation(UObject* ContextObject) override;
};
