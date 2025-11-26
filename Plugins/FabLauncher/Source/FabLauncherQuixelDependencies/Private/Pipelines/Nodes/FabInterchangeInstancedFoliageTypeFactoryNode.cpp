// Copyright Epic Games, Inc. All Rights Reserved.
#include "FabInterchangeInstancedFoliageTypeFactoryNode.h"

#include "FoliageType_InstancedStaticMesh.h"

FString UFabInterchangeInstancedFoliageTypeFactoryNode::GetNodeUidFromStaticMeshFactoryUid(const FString& StaticMeshFactoryUid)
{
	return UInterchangeFactoryBaseNode::BuildFactoryNodeUid(TEXT("InstancedFoliageType") + StaticMeshFactoryUid);
}

UClass* UFabInterchangeInstancedFoliageTypeFactoryNode::GetObjectClass() const
{
	return UFoliageType_InstancedStaticMesh::StaticClass();
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomStaticMesh(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(StaticMesh, FString);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomStaticMesh(const FString& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(StaticMesh, FString);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomScaling(EFoliageScaling& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(Scaling, EFoliageScaling);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomScaling(const EFoliageScaling AttributeValue, const bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UFabInterchangeInstancedFoliageTypeFactoryNode, Scaling, EFoliageScaling, UFoliageType_InstancedStaticMesh);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomScaleX(FVector2f& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ScaleX, FVector2f);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomScaleX(const FVector2f& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ScaleX, FVector2f);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomScaleY(FVector2f& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ScaleY, FVector2f);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomScaleY(const FVector2f& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ScaleY, FVector2f);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomScaleZ(FVector2f& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ScaleZ, FVector2f);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomScaleZ(const FVector2f& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ScaleZ, FVector2f);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomAlignToNormal(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(AlignToNormal, bool);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomAlignToNormal(const bool AttributeValue, const bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UFabInterchangeInstancedFoliageTypeFactoryNode, AlignToNormal, bool, UFoliageType_InstancedStaticMesh);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomRandomYaw(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(RandomYaw, bool);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomRandomYaw(const bool AttributeValue, const bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UFabInterchangeInstancedFoliageTypeFactoryNode, RandomYaw, bool, UFoliageType_InstancedStaticMesh);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomRandomPitchAngle(float& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(RandomPitchAngle, float);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomRandomPitchAngle(const float AttributeValue, const bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UFabInterchangeInstancedFoliageTypeFactoryNode, RandomPitchAngle, float, UFoliageType_InstancedStaticMesh);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomAffectDistanceFieldLighting(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(bAffectDistanceFieldLighting, bool);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomAffectDistanceFieldLighting(const bool AttributeValue, const bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UFabInterchangeInstancedFoliageTypeFactoryNode, bAffectDistanceFieldLighting, bool, UFoliageType_InstancedStaticMesh);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::GetCustomWorldPositionOffsetDisableDistance(int32& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(WorldPositionOffsetDisableDistance, int32);
}

bool UFabInterchangeInstancedFoliageTypeFactoryNode::SetCustomWorldPositionOffsetDisableDistance(const int32 AttributeValue, const bool bAddApplyDelegate)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER(UFabInterchangeInstancedFoliageTypeFactoryNode, WorldPositionOffsetDisableDistance, int32, UFoliageType_InstancedStaticMesh);
}