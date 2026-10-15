// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Nodes/AnimNode_ACFProceduralHandOffset.h"
#include "AnimGraphNode_ACFProceduralHandOffset.generated.h"

UCLASS()
class ASCENTFIRSTPERSONUNCOOKED_API UAnimGraphNode_ACFProceduralHandOffset : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settings")
	FAnimNode_ACFProceduralHandOffset Node;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual FString GetNodeCategory() const override;

	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
