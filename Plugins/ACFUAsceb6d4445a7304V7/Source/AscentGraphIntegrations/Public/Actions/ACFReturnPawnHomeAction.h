// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGSAction.h"
#include "ACFReturnPawnHomeAction.generated.h"

class APawn;
class ATargetPoint;

/** Assigns a new home and requests ReturnHome through the pawn's existing ACF AI behavior. */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories,
    meta = (DisplayName = "ACF ReturnPawnHome", Keywords = "Pawn Home Return Quest Dialogue"))
class ASCENTGRAPHINTEGRATIONS_API UACFReturnPawnHomeAction : public UAGSAction
{
    GENERATED_BODY()

public:
    /** Sets home and current AI state. Success means the request was issued, not that the pawn has arrived. */
    UFUNCTION(BlueprintCallable, Category = "ACF", meta = (DisplayName = "Return Pawn Home"))
    bool ReturnPawnHome();

protected:
    virtual void ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner) override;

    /** Loaded level pawn, possessed by an ACFAIController. Executes only on authority. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TSoftObjectPtr<APawn> TargetPawn;

    /** Loaded destination actor whose position becomes the pawn's new home. Does not teleport the pawn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TSoftObjectPtr<ATargetPoint> TargetPoint;

};
