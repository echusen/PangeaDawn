// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimGraphNode_ACFProceduralCurveBlend.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFProceduralCurveBlend"

FText UAnimGraphNode_ACFProceduralCurveBlend::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Procedural Curve Blend");
}

FLinearColor UAnimGraphNode_ACFProceduralCurveBlend::GetNodeTitleColor() const
{
	return { 0.2f, 0.8f, 0.2f };
}

FText UAnimGraphNode_ACFProceduralCurveBlend::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Blends the curves between to poses");
}

FText UAnimGraphNode_ACFProceduralCurveBlend::GetMenuCategory() const
{
	return LOCTEXT("Category", "ACF|First Person");
}

FString UAnimGraphNode_ACFProceduralCurveBlend::GetNodeCategory() const
{
	return GetMenuCategory().ToString();
}

#undef LOCTEXT_NAMESPACE