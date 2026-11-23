// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGSAction.h"
#include "ACFSetCharacterGravityAction.generated.h"

class ACharacter;

/** Sets CharacterMovement gravity on a loaded character. Does not monitor World Partition or affect ragdoll physics. */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories,
    meta = (DisplayName = "ACF Set Character Gravity", Keywords = "Gravity Character World Partition Quest Dialogue"))
class ASCENTGRAPHINTEGRATIONS_API UACFSetCharacterGravityAction : public UAGSAction
{
    GENERATED_BODY()

public:
    /** Applies the configured gravity on authority. Returns false if the character is unloaded or has no CharacterMovement. */
    UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Apply Character Gravity", ToolTip = "Applies the configured CharacterMovement gravity to the loaded target on authority. Disabling also clears velocity along the gravity axis."))
    bool ApplyCharacterGravity();

protected:
    virtual void ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner) override;

    /** Character to modify. Must already be loaded; this action does not force a World Partition cell to load. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TSoftObjectPtr<ACharacter> TargetCharacter;

    /** Enable the configured gravity scale, or disable gravity and clear velocity along the gravity axis. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF", meta = (DisplayName = "Enable Gravity"))
    bool bEnableGravity = true;

    /** Explicit scale applied when enabling, not a cached previous value. Set this to the character's usual scale. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF", meta = (ClampMin = "0.0", EditCondition = "bEnableGravity"))
    float EnabledGravityScale = 1.0f;
};
