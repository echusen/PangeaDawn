// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimGraphNode_ACFProceduralHandOffset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_ACFProceduralHandOffset)

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFProceduralHandOffset"

FText UAnimGraphNode_ACFProceduralHandOffset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Procedural Hand Offset");
}

FLinearColor UAnimGraphNode_ACFProceduralHandOffset::GetNodeTitleColor() const
{
	return { 0.8f, 0.2f, 0.2f };
}

FText UAnimGraphNode_ACFProceduralHandOffset::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Perform Hand Offset");
}

FText UAnimGraphNode_ACFProceduralHandOffset::GetMenuCategory() const
{
	return LOCTEXT("Category", "ACF|First Person");
}

FString UAnimGraphNode_ACFProceduralHandOffset::GetNodeCategory() const
{
	return GetMenuCategory().ToString();
}

FText UAnimGraphNode_ACFProceduralHandOffset::GetControllerDescription() const
{
	return LOCTEXT("Title", "Procedural Hand Offset");
}

#undef LOCTEXT_NAMESPACE
