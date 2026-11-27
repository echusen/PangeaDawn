// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/UpgradeRequirement.h"
#include "Req_QuestCompleted.generated.h"

struct FGameplayTag;

/**
 * Requirement that passes when the interacting player has completed every listed quest.
 * Reads the quest manager of the pawn's own controller, so each player is checked against their own quests.
 */
UCLASS(Blueprintable)
class PANGEABASEUPGRADESYSTEM_API UReq_QuestCompleted : public UUpgradeRequirement
{
	GENERATED_BODY()

public:
	/** Quest tags that must all be completed. An empty list passes once a quest manager is found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirement")
	TArray<FGameplayTag> RequiredQuestTags;

	virtual bool IsRequirementMetForContext(const FBaseUpgradeContext& Context) const override;
	virtual bool IsRequirementMet_Implementation(UObject* ContextObject) const override;
	virtual FText GetFailureMessage() const override;
};
