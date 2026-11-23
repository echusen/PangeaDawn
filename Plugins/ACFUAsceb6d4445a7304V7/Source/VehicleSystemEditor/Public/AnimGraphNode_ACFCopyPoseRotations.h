// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_ACFCopyPoseRotations.h"
#include "AnimGraphNode_ACFCopyPoseRotations.generated.h"

UCLASS(MinimalAPI, meta = (Keywords = "ACF Vehicle Copy Pose Rotations Door Physics"))
class UAnimGraphNode_ACFCopyPoseRotations : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_ACFCopyPoseRotations Node;

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FString GetNodeCategory() const override;
	// End of UEdGraphNode interface
};
