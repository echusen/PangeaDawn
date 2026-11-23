// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFActionTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BTTask_BoatBroadsideFire.generated.h"

/**
 * Triggers the boat's broadside-fire ability by tag on the pawn's ACFAbilitySystemComponent.
 * The ability itself (granted to the boat via ability set) picks the firing side, resolves the
 * cannon sockets and fires the volley — same path a player-controlled boat uses.
 */
UCLASS()
class VEHICLESYSTEM_API UBTTask_BoatBroadsideFire : public UBTTaskNode {
    GENERATED_BODY()

public:
    UBTTask_BoatBroadsideFire();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
    /*Action tag of the broadside-fire ability granted to the boat*/
    UPROPERTY(EditAnywhere, Category = ACF, meta = (Categories = "Actions,Ability"))
    FGameplayTag FireAbilityTag;

    UPROPERTY(EditAnywhere, Category = ACF)
    EActionPriority Priority = EActionPriority::EHigh;
};
