// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include <GameplayTagContainer.h>
#include <InputAction.h>

#include "ACFComboComponent.generated.h"

class UEnhancedInputComponent;
class UEnhancedPlayerInput;
class UACFComboGraph;
class UInputAction;
class UACFAbilitySystemComponent;
class UACFComboNode;
class ACharacter;

/**
 * [Multi-input combo fix] One buffered combo input, timestamped and scoped to the exact combo run
 * and node it was made during, so a press only ever advances that node.
 */
struct FACFBufferedComboInput {
    FGameplayTag InputTag;
    double ReceivedWorldTime = 0.0;
    bool bHasReachedFirstEvaluationPoint = false;
    double ExpiryClockStartTime = 0.0;
    TWeakObjectPtr<UACFComboGraph> CapturedGraph;
    TWeakObjectPtr<UACFComboNode> CapturedNode;
};

/**
 * Handles combo execution logic for the owning actor.
 * Allows starting, stopping, and tracking active combo graphs.
 */
UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTCOMBOGRAPH_API UACFComboComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFComboComponent();

    /**
     * Starts the given combo using the provided triggering action.
     * @param comboToStart The combo graph to execute.
     * @param triggeringAction The tag that triggered the combo.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void StartCombo(UACFComboGraph* comboToStart, const FGameplayTag& triggeringAbility);

    /**
     * Stops the execution of the specified combo graph.
     * @param comboToStop The combo graph to stop.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void StopCombo(UACFComboGraph* comboToStop);

    /**
     * Enables or disables the input buffer during combo execution.
     * @param bEnabled Whether to enable or disable the input buffer.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetInputBufferOpened(bool bEnabled);

    /**
     * Checks if the specified combo graph is currently being executed.
     * @param combo The combo graph to check.
     * @return True if the combo is being executed.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsExecutingCombo(UACFComboGraph* combo) const;

    /**
     * Checks if any combo is currently being executed.
     * @return True if a combo is in progress.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool IsExecutingAnyCombo() const;

    /**
     * Returns the active runtime combo graph instance (may differ from the source asset
     * since StartCombo creates a per-character DuplicateObject).
     * @return The current combo graph instance, or nullptr if no combo is active.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    UACFComboGraph* GetCurrentCombo() const { return currentCombo; }

    /**
     * Checks whether there is a pending input tag stored for the combo system.
     * @return True if an input tag has been stored and not yet consumed.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool HasPendingInput() const
    {
        return PendingComboInputs.Num() > 0;
    }

    /**
     * Gets the last received gameplay tag input used to advance or trigger a combo transition.
     * @return The last gameplay tag input.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FGameplayTag GetLastTagInput() const { return LastTagInput; }

    /**
     * Sends the received input tag to the server for authoritative combo handling.
     * Should be called when a valid input tag is triggered on the client.
     * @param tagInput The gameplay tag representing the input direction or transition.
     */
    UFUNCTION(Server, Reliable)
    void SendInputReceived(const FGameplayTag& tagInput);

    /**
     * Clears the stored input tag after it has been processed.
     * Call this to reset the pending input state.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ClearInputTag();

    /**
     * [Multi-input combo fix] At a node's step-end, consume the best buffered input for this node and,
     * if it maps to a valid transition, advance the graph. Returns true (and the consumed tag) on success.
     */
    bool TryConsumeBufferedTransition(const ACharacter* Character, FGameplayTag& OutConsumedInput);

    /** [Multi-input combo fix] Resets the per-node transition window; call when a new combo node begins. */
    void BeginComboNode();

    /** [Multi-input combo fix] Clears all buffered inputs (and the legacy LastTagInput). */
    void ClearBufferedInputs();

    /**
     * [Multi-input combo fix] Single-use authorization for the combo's OWN next-node re-activation, so
     * CanActivateAbility can tell a legitimate continuation from an external mid-combo re-press.
     */
    void AuthorizeNextNodeActivation(const FGameplayTag& ActionTag);
    bool IsNextNodeActivationAuthorized(const FGameplayTag& ActionTag) const;
    bool ConsumeNextNodeActivationAuthorization(const FGameplayTag& ActionTag);

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    /**
     * Binds combo-related input actions to the provided enhanced input component.
     * Should be called during input setup (e.g. inside SetupPlayerInputComponent or on every Possess).
     *
     * @param EnhancedInputComponent The input component to bind actions to.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetupPlayerInputComponent(UEnhancedInputComponent* EnhancedInputComponent);

    /**
     * True if the component is currently performing a combo.
     */
    UPROPERTY(Replicated)
    bool bIsPerformingCombo;

    /**
     * Map of input actions that can be used to trigger or advance combo sequences
     * and their related transition tag
     */
    UPROPERTY(EditAnywhere, Category = ACF)
    TMap<UInputAction*, FGameplayTag> ComboInputs;

    /** [Multi-input combo fix] How long a buffered input stays eligible AFTER its node opens for
     *  transition evaluation (seconds). 0 = never expires. */
    UPROPERTY(EditAnywhere, Category = "ACF|Combo Buffer", meta = (ClampMin = "0.0"))
    float ComboInputBufferDuration = 0.5f;

private:
    UFUNCTION()
    void HandleComboInputReceived(UInputAction* input);

    void StopCurrentCombo();

    void Internal_StopCombo();

    bool Internal_InputReceived(const FGameplayTag& tagInput);

    UPROPERTY()
    bool bIsInputBufferEnabled;

    UFUNCTION(Client, Reliable)
    void ClientStopCombo();

    UPROPERTY(Replicated)
    FGameplayTag LastTagInput;

    // [Multi-input combo fix] node-scoped, timestamped input buffer + single-use continuation token
    TMap<FGameplayTag, FACFBufferedComboInput> PendingComboInputs;
    bool bTransitionWindowWasOpenedForCurrentNode = false;
    FGameplayTag AuthorizedNextNodeAction;

    UPROPERTY()
    TObjectPtr<UACFComboGraph> currentCombo;
    TObjectPtr<UACFAbilitySystemComponent> actionsComp;
};
