// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Nodes/AnimNode_ACFProceduralCurveBlend.h"
#include "AnimGraphNode_ACFProceduralCurveBlend.generated.h"

UCLASS()
class ASCENTFIRSTPERSONUNCOOKED_API UAnimGraphNode_ACFProceduralCurveBlend : public UAnimGraphNode_Base
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FAnimNode_ACFProceduralCurveBlend Node;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual FString GetNodeCategory() const override;
};
