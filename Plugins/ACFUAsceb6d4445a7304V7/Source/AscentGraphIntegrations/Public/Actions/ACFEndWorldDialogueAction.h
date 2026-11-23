// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGSAction.h"
#include "ACFEndWorldDialogueAction.generated.h"

class UADSWorldDialogue;

/** Ends the world dialogue this node belongs to. From a graph that is not a world dialogue (a quest, a state machine),
 * it ends every world dialogue currently playing instead. */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories,
    meta = (DisplayName = "ACF End World Dialogue", Keywords = "Dialogue World End Stop Terminate Quest"))
class ASCENTGRAPHINTEGRATIONS_API UACFEndWorldDialogueAction : public UAGSAction
{
    GENERATED_BODY()

public:
    /** Ends the world dialogue, audio and camera included. The node holding this action still finishes activating,
     * so the dialogue closes on the next frame. Returns false if no world dialogue is playing. */
    UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "End World Dialogue", ToolTip = "Ends the world dialogue that owns this node, or every world dialogue currently playing when this graph is not a dialogue. The end is deferred one frame so the current node can finish activating."))
    bool EndWorldDialogue();

protected:
    virtual void ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner) override;

private:
    /** World dialogue that owns this action's node, if any. */
    UADSWorldDialogue* GetOwningWorldDialogue() const;

    /** Ends the owning world dialogue, or falls back to every world dialogue currently playing. */
    void EndWorldDialogueNow();

    /** Guards against scheduling the deferred end twice when the node is activated again in the same frame. */
    bool bEndScheduled = false;
};
