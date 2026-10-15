// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_ACFCameraAim.h"
#include "CoreMinimal.h"

#include "AnimGraphNode_ACFCameraAim.generated.h"

UCLASS(MinimalAPI, meta = (Keywords = "ACF Camera Aim Spine Chain Socket Lock Stabilize View"))
class UAnimGraphNode_ACFCameraAim : public UAnimGraphNode_SkeletalControlBase {
    GENERATED_UCLASS_BODY()

public:
    UPROPERTY(EditAnywhere, Category = Settings)
    FAnimNode_ACFCameraAim Node;

    // UEdGraphNode interface
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    // End of UEdGraphNode interface

    // UAnimGraphNode_Base interface
    virtual FString GetNodeCategory() const override;
    virtual void ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog) override;
    virtual void PreloadRequiredAssets() override;
    // End of UAnimGraphNode_Base interface

protected:
    // UAnimGraphNode_SkeletalControlBase interface
    virtual FText GetControllerDescription() const override;
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
    // End of UAnimGraphNode_SkeletalControlBase interface
};
