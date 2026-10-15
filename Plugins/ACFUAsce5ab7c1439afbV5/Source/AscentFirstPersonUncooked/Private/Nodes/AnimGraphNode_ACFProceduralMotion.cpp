// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimGraphNode_ACFProceduralMotion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_ACFProceduralMotion)

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFProceduralMotion"

FText UAnimGraphNode_ACFProceduralMotion::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Procedural Motion");
}

FLinearColor UAnimGraphNode_ACFProceduralMotion::GetNodeTitleColor() const
{
	return { 0.8f, 0.2f, 0.2f };
}

FText UAnimGraphNode_ACFProceduralMotion::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Perform procedural motion to selected bone based on the ACFProceduralMotionComponent");
}

FText UAnimGraphNode_ACFProceduralMotion::GetMenuCategory() const
{
	return LOCTEXT("Category", "ACF|First Person");
}

FString UAnimGraphNode_ACFProceduralMotion::GetNodeCategory() const
{
	return GetMenuCategory().ToString();
}

FText UAnimGraphNode_ACFProceduralMotion::GetControllerDescription() const
{
	return LOCTEXT("Title", "Procedural Motion");
}

#undef LOCTEXT_NAMESPACE
