// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFAITypes.h"

#include "Actors/ACFCharacter.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "CoreMinimal.h"
#include "Game/ACFDamageType.h"
#include "Game/ACFTypes.h"
#include <Components/ActorComponent.h>

#include "ACFCombatBehaviourComponent.generated.h"

struct FACFDamageEvent;
class AACFAIController;
class UACFCombatBehaviorDataAsset;
/**
 *
 */

UCLASS(ClassGroup = (ACF), Blueprintable, meta = (BlueprintSpawnableComponent))
class AIFRAMEWORK_API UACFCombatBehaviourComponent : public UActorComponent {
    GENERATED_BODY()

public:
    /**
     * Tries to execute one of the actions defined in ActionByCombatState for the specified combat state.
     *
     * @param combatState The current combat state to evaluate actions for.
     * @return true if at least one action was executed successfully; false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    virtual bool TryExecuteActionByCombatState(EAICombatState combatState);

    /**
     * Evaluates if an action (element) should be executed based on its chance or "ticket".
     *
     * @param elem The action and its execution chance to evaluate.
     * @return true if the action passes the evaluation and can be executed.
     */
    bool EvaluateTicket(const FActionChances& elem);

    /**
     * Tries to execute one of the actions defined in ActionByCondition based on evaluated conditions.
     *
     * @return true if a conditional action was executed successfully; false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    virtual bool TryExecuteConditionAction();

    /**
     * Checks whether the given target is within melee range of this AI.
     *
     * @param target The actor to check distance to.
     * @return true if the target is within melee range; false otherwise.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsTargetInMeleeRange(AActor* target);

    /**
     * Determines the best combat state for the AI to switch to, based on the distance from the target.
     *
     * @param targetDistance The distance from the AI to the target.
     * @return The most suitable combat state for the given distance.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    EAICombatState GetBestCombatStateByTargetDistance(float targetDistance);

    /**
     * Gets the ideal engagement distance for a specific combat state.
     *
     * @param combatState The combat state to get the ideal distance for.
     * @return The optimal distance in centimeters for the specified combat state.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    float GetIdealDistanceByCombatState(EAICombatState combatState) const;

    UFUNCTION(BlueprintPure, Category = ACF)
    UACFCombatBehaviorDataAsset* GetCombatBehaviour() const { return CombatBehaviour; }

    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetCombatBehaviour(UACFCombatBehaviorDataAsset* combatBehav) { CombatBehaviour = combatBehav; }

    UACFCombatBehaviourComponent();

protected:
    /* A data asset describing how this AI should behave in combat*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    UACFCombatBehaviorDataAsset* CombatBehaviour;

    /* Action to be triggered by this ai to equip a Melee weapon*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DefaultActionsTag")
    FGameplayTag EquipMeleeAction;

    /* Action to be triggered by this ai to equip a Ranged weapon*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DefaultActionsTag")
    FGameplayTag EquipRangedAction;

    /* Action to be triggered when combat starts*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DefaultActionsTag")
    FGameplayTag EngagingAction;

    /* If this ai needs an equipped weapon to start fighting*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DEPRECATED")
    bool bNeedsWeapon = false;

    /*The default combat behavior (melee/ranged) for this AI. Could change during combat if multiple
    behaviors have been defined in Allowed Behaviors*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DEPRECATED")
    ECombatBehaviorType DefaultCombatBehaviorType;

    /*The default combat state  for this AI, triggered once no other states are available*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DEPRECATED")
    EAICombatState DefaultCombatState = EAICombatState::EMeleeCombat;

    /*Configuration of each Combat State*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DEPRECATED")
    TArray<FAICombatStateConfig> CombatStatesConfig;

    /*The actions that should be performed by the AI for every combat state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DEPRECATED")
    TMap<EAICombatState, FActionsChances> ActionByCombatState;

    /*Generic conditionals action you can define by creating your own ActionCondition class*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|DEPRECATED")
    TArray<FConditions> ActionByCondition;

    friend class AACFAIController;

private:
    bool VerifyCondition(const FConditions& condition);

    void InitBehavior(class AACFAIController* _controller);

    void TryEquipWeapon();

    void UninitBehavior();

    bool CheckEquipment();

    void UpdateCombatLocomotion(EAICombatState combatState);

    TObjectPtr<AACFCharacter> characterOwner;

    TObjectPtr<AACFAIController> aiController;

    bool EvaluateCombatState(EAICombatState combatState);
};
