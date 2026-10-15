// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "AGSAction.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ACFAssignTeamToActorsAction.generated.h"

class UACFTeamComponent;

/**
 * Graph action that assigns an ACF team to a set of actors referenced in the level through soft references.
 * Each referenced actor is searched for an ACF Team Component, which is the component holding the team tag.
 * Typical use case: a quest node turning a group of NPCs hostile, or making a faction friendly again.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories,
    AutoExpandCategories = ("Default"), meta = (DisplayName = "Assign ACF Team To Actors", Keywords = "ACF Team Faction Hostile Friendly Quest Dialogue"))
class ASCENTGRAPHINTEGRATIONS_API UACFAssignTeamToActorsAction : public UAGSAction {
    GENERATED_BODY()

public:
    /** Assigns TeamToAssign to every resolvable target actor. Returns how many actors were actually updated. */
    UFUNCTION(BlueprintCallable, Category = "ACF | Graph Integrations", meta = (DisplayName = "Apply Team To Targets"))
    int32 ApplyTeamToTargets();

    /** Resolves the target soft references to the actors currently loaded in the world. */
    UFUNCTION(BlueprintCallable, Category = "ACF | Graph Integrations", meta = (DisplayName = "Get Resolved Target Actors"))
    void GetResolvedTargetActors(TArray<AActor*>& outActors) const;

protected:
    virtual void ExecuteAction_Implementation(class APlayerController* playerController, UAGSGraphNode* nodeOwner) override;

    /** Team assigned to every target actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Teams"), Category = "Default")
    FGameplayTag TeamToAssign;

    /** Actors placed in the level receiving the new team. Actors streamed out at execution time are skipped. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
    TArray<TSoftObjectPtr<AActor>> TargetActors;

    /** When a target pawn has no ACF Team Component, look for one on its controller before giving up. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
    bool bFallbackToControllerComponent = true;

private:
    UACFTeamComponent* FindTeamComponent(AActor* targetActor) const;
};
