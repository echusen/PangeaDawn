// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AnimGraphNode_ACFCopyPoseRotations.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFCopyPoseRotations"

FText UAnimGraphNode_ACFCopyPoseRotations::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "ACF Copy Pose Rotations");
}

FText UAnimGraphNode_ACFCopyPoseRotations::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Copies bone rotations from a source mesh component to the target. Used for vehicle door physics.");
}

FLinearColor UAnimGraphNode_ACFCopyPoseRotations::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.6f, 1.0f);
}

FString UAnimGraphNode_ACFCopyPoseRotations::GetNodeCategory() const
{
	return TEXT("Animation|ACF");
}

#undef LOCTEXT_NAMESPACE
