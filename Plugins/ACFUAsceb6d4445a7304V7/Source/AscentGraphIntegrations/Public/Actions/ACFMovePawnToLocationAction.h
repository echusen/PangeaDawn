// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGSAction.h"
#include "TimerManager.h"
#include "ACFMovePawnToLocationAction.generated.h"

class APawn;
class ATargetPoint;

/** Relocates an AI pawn, optionally updating its home, without changing its AI state or target. */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories,
    meta = (DisplayName = "ACF MovePawnToLocation", Keywords = "Teleport Pawn Home Quest Dialogue"))
class ASCENTGRAPHINTEGRATIONS_API UACFMovePawnToLocationAction : public UAGSAction
{
    GENERATED_BODY()

public:
    /** Moves immediately, ignoring Delay. Requires authority, loaded references and an ACF AI controller. */
    UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Move Pawn To Location"))
    bool MovePawnToLocation();

protected:
    virtual void ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner) override;

    /** Level pawn to relocate. Must be possessed by an ACFAIController. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TSoftObjectPtr<APawn> TargetPawn;

    /** Supplies the destination position and rotation; pawn scale is preserved. Both actors must already be loaded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TSoftObjectPtr<ATargetPoint> TargetPoint;

    /** Seconds before relocation. Zero executes immediately. This does not delay graph progression. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF", meta = (ClampMin = "0.0", Units = "s"))
    float Delay = 1.0f;

    /** Updates the AI controller's home after a successful relocation. Disable to preserve its existing home. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF", meta = (DisplayName = "Change Home"))
    bool bChangeHome = true;

private:
    void ExecuteDelayedMove();

    FTimerHandle MoveTimerHandle;
};
