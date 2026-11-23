// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Nodes/AnimGraphNode_ACFProceduralBoneRebase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_ACFProceduralBoneRebase)

#define LOCTEXT_NAMESPACE "AnimGraphNode_ACFProceduralBoneRebase"

FText UAnimGraphNode_ACFProceduralBoneRebase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Procedural Bone Rebase");
}

FLinearColor UAnimGraphNode_ACFProceduralBoneRebase::GetNodeTitleColor() const
{
	return { 0.8f, 0.2f, 0.2f };
}

FText UAnimGraphNode_ACFProceduralBoneRebase::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Perform Bone Rebase");
}

FText UAnimGraphNode_ACFProceduralBoneRebase::GetMenuCategory() const
{
	return LOCTEXT("Category", "ACF|First Person");
}

FString UAnimGraphNode_ACFProceduralBoneRebase::GetNodeCategory() const
{
	return GetMenuCategory().ToString();
}

FText UAnimGraphNode_ACFProceduralBoneRebase::GetControllerDescription() const
{
	return LOCTEXT("Title", "Procedural Bone Rebase");
}

#undef LOCTEXT_NAMESPACE
