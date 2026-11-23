// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Nodes/AnimNode_ACFProceduralMotion.h"
#include "AnimGraphNode_ACFProceduralMotion.generated.h"

UCLASS()
class ASCENTFIRSTPERSONUNCOOKED_API UAnimGraphNode_ACFProceduralMotion : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings")
	FAnimNode_ACFProceduralMotion Node;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual FString GetNodeCategory() const override;

	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
