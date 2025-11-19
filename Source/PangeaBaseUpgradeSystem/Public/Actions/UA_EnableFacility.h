// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objects/UpgradeAction.h"
#include "UA_EnableFacility.generated.h"

UENUM(BlueprintType)
enum class EFacilityEnableMode : uint8
{
	EnableOnly,
	DisableOnly,
	Toggle
};

UCLASS()
class PANGEABASEUPGRADESYSTEM_API UUA_EnableFacility : public UUpgradeAction
{
	GENERATED_BODY()

public:

	/* Which facility to affect */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facility")
	FGameplayTag FacilityTag;

	/* Mode */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facility")
	EFacilityEnableMode Mode = EFacilityEnableMode::EnableOnly;

	virtual void Execute_Implementation(UObject* ContextObject) override;
};
