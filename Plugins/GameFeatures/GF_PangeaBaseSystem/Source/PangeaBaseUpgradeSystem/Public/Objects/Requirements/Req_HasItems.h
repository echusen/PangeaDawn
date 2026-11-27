// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_HasItems.generated.h"

class UACFItem;

/** One item class and the number of it a pawn must own for UReq_HasItems. */
USTRUCT(BlueprintType)
struct FRequiredACFItem
{
	GENERATED_BODY()

	/** Item class the pawn must own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UACFItem> ItemClass;

	/** Minimum number of items of that class in the inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count = 1;
};

/**
 * Requirement that passes when the interacting pawn's ACF inventory holds every listed item.
 * It is an ownership check only. Nothing is removed, payment is authored as explicit level costs.
 */
UCLASS(Blueprintable)
class PANGEABASEUPGRADESYSTEM_API UReq_HasItems : public UUpgradeRequirement
{
	GENERATED_BODY()

public:
	/** Items the pawn must own. Use one entry per item class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirement")
	TArray<FRequiredACFItem> RequiredItems;

	virtual bool IsRequirementMetForContext(const FBaseUpgradeContext& Context) const override;
	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
	virtual FText GetFailureMessage() const override;

	virtual FText GetRequirementDescription_Implementation() const override;
};
