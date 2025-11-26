// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "Actions/ACFActionAbility.h"
#include "CoreMinimal.h"

#include "ACFComboAction.generated.h"

/**
 * Base class for all combo-based actions in the ACF system.
 * Handles combo counting, validation and combo input triggering.
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API UACFComboAction : public UACFActionAbility {
    GENERATED_BODY()

public:
    UACFComboAction();

    /**
     * Returns the current combo counter for this action tag.
     * @return The number of consecutive combo actions performed.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    int32 GetComboCounter() const
    {
        return GetActionsManager()->GetComboCount(ActionTag);
    }

    /**
     * Returns whether the combo was successfully executed.
     * @return True if the combo is successful, false otherwise.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE bool IsSuccessfulCombo() const { return bSuccesfulCombo; }

    /**
     * Resets the combo counter associated with this action tag.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ResetComboCounter();

    /**
     * Forces the combo counter to a specific value.
     * @param val The new value to assign to the combo counter.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ForceComboCounter(int32 val)
    {
        GetActionsManager()->SetComboCounter(ActionTag, val);
    }

    /**
     * Sends an input to advance the combo (e.g. player presses attack again).
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SendComboInput();

protected:
    virtual void OnActionStarted_Implementation() override;

    virtual FName GetMontageSectionName_Implementation() override;

    virtual void OnActionEnded_Implementation() override;

    virtual void OnGameplayEventReceived_Implementation(const FGameplayTag eventTag);

    UPROPERTY()
    bool bSuccesfulCombo = false;
};
