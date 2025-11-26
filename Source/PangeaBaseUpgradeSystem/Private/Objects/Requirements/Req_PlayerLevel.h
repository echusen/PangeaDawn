// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_PlayerLevel.generated.h"

/**
 * 
 */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API UReq_PlayerLevel : public UUpgradeRequirement
{
	GENERATED_BODY()
	
public:
	
	// Minimum player level required
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Requirement")
	int32 RequiredLevel = 1;
	
	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
	virtual FText GetRequirementDescription_Implementation() const override;
};
