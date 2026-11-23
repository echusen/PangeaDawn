// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimGraphNode_ACFProceduralAimOffset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_ACFProceduralAimOffset)

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFProceduralAimOffset"

FText UAnimGraphNode_ACFProceduralAimOffset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FLinearColor UAnimGraphNode_ACFProceduralAimOffset::GetNodeTitleColor() const
{
	return { 0.8f, 0.2f, 0.2f };
}

FText UAnimGraphNode_ACFProceduralAimOffset::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Orient the selected bones to match the aim rotation (relative space).");
}

FText UAnimGraphNode_ACFProceduralAimOffset::GetMenuCategory() const
{
	return LOCTEXT("Category", "ACF|First Person");
}

FString UAnimGraphNode_ACFProceduralAimOffset::GetNodeCategory() const
{
	return GetMenuCategory().ToString();
}

FText UAnimGraphNode_ACFProceduralAimOffset::GetControllerDescription() const
{
	if (Node.bAutoFillRotationFromLeaning) return LOCTEXT("Title", "Procedural Aim Offset (Leaning)");

	if (Node.bAutoFillRotationFromCamera) return LOCTEXT("Title", "Procedural Aim Offset (Camera)");

	return LOCTEXT("Title", "Procedural Aim Offset");
}

#undef LOCTEXT_NAMESPACE
