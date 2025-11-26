// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Nodes/InterchangeFactoryBaseNode.h"

#include "FabInterchangeInstancedFoliageTypeFactoryNode.generated.h"

UCLASS(BlueprintType)
class UFabInterchangeInstancedFoliageTypeFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	static FString GetNodeUidFromStaticMeshFactoryUid(const FString& StaticMeshFactoryUid);

public:
	virtual UClass* GetObjectClass() const override;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomStaticMesh(FString& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomStaticMesh(const FString& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomScaling(EFoliageScaling& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomScaling(const EFoliageScaling AttributeValue, const bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomScaleX(FVector2f& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomScaleX(const FVector2f& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomScaleY(FVector2f& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomScaleY(const FVector2f& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomScaleZ(FVector2f& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomScaleZ(const FVector2f& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomAlignToNormal(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomAlignToNormal(const bool AttributeValue, const bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomRandomYaw(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomRandomYaw(const bool AttributeValue, const bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomRandomPitchAngle(float& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomRandomPitchAngle(const float AttributeValue, const bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomAffectDistanceFieldLighting(bool& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomAffectDistanceFieldLighting(const bool AttributeValue, const bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool GetCustomWorldPositionOffsetDisableDistance(int32& AttributeValue) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | FabInstancedFoliageTypeFactory")
	bool SetCustomWorldPositionOffsetDisableDistance(const int32 AttributeValue, const bool bAddApplyDelegate = true);

private:
	IMPLEMENT_NODE_ATTRIBUTE_KEY(StaticMesh);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(Scaling);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(ScaleX);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(ScaleY);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(ScaleZ);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(AlignToNormal);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(RandomYaw);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(RandomPitchAngle);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(bAffectDistanceFieldLighting);
	IMPLEMENT_NODE_ATTRIBUTE_KEY(WorldPositionOffsetDisableDistance);

};