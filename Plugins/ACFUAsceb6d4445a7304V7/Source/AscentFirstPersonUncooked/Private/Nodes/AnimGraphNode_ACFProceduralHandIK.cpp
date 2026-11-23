// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimGraphNode_ACFProceduralHandIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_ACFProceduralHandIK)

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFProceduralHandIK"

FText UAnimGraphNode_ACFProceduralHandIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Procedural Hand IK");
}

FLinearColor UAnimGraphNode_ACFProceduralHandIK::GetNodeTitleColor() const
{
	return { 0.8f, 0.2f, 0.2f };
}

FText UAnimGraphNode_ACFProceduralHandIK::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Performs both Two Bone IK with correct setup for both Hands using Lowerarm as Joint");
}

FText UAnimGraphNode_ACFProceduralHandIK::GetMenuCategory() const
{
	return LOCTEXT("Category", "ACF|First Person");
}

FString UAnimGraphNode_ACFProceduralHandIK::GetNodeCategory() const
{
	return GetMenuCategory().ToString();
}

FText UAnimGraphNode_ACFProceduralHandIK::GetControllerDescription() const
{
	return LOCTEXT("Title", "Procedural Hand IK");
}

#undef LOCTEXT_NAMESPACE
