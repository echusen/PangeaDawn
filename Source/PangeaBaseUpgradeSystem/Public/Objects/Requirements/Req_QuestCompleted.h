// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_QuestCompleted.generated.h"

struct FGameplayTag;
/**
 * 
 */
UCLASS(Blueprintable)
class PANGEABASEUPGRADESYSTEM_API UReq_QuestCompleted : public UUpgradeRequirement
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
	TArray<FGameplayTag> RequiredQuestTags;

	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
	virtual FText GetFailureMessage() const override;
};
