// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_HasItems.generated.h"

USTRUCT(BlueprintType)
struct FRequiredACFItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UACFItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count = 1;
};

/**
 * 
 */
UCLASS(Blueprintable)
class PANGEABASEUPGRADESYSTEM_API UReq_HasItems : public UUpgradeRequirement
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
	TArray<FRequiredACFItem> RequiredItems;

	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
	virtual FText GetFailureMessage() const override;
};
