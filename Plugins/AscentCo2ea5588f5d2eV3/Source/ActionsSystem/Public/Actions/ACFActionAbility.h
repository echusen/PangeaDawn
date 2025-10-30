// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFActionTypes.h"
#include "Animation/AnimMontage.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include <Abilities/GameplayAbility.h>
#include <GameplayTagContainer.h>

#include "ACFActionAbility.generated.h"

class UAnimMontage;
class UARSStatisticsComponent;

/**
 *
 * A specialized Gameplay Ability used in ACF to represent animated abilities.
 * When triggered, it plays an animation montage and automatically ends when the montage finishes.
 * This class encapsulates logic for warp reproduction, animation-driven execution flow,
 * and optional substate and effect hooks.
 *
 */
UCLASS(Blueprintable, BlueprintType, DefaultToInstanced, EditInlineNew)
class ACTIONSSYSTEM_API UACFActionAbility : public UGameplayAbility {
    GENERATED_BODY()

    friend UACFAbilitySystemComponent;

public:
    UACFActionAbility();

    // GETTERS & SETTERS//
    /**
     * Returns the current action configuration for this action.
     * @return A copy of the current FActionConfig struct.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FActionConfig GetActionConfig() const
    {
        return ActionConfig;
    }

    /**
     * Sets a new configuration for the action.
     * @param newConfig The new action configuration to assign.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetActionConfig(const FActionConfig& newConfig);

    /**
     * Assigns a new animation montage for this action.
     * @param newMontage The animation montage to use.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetAnimMontage(UAnimMontage* newMontage);

    /**
     * Returns the animation montage used for this action.
     * @return A pointer to the animation montage asset.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    UAnimMontage* GetAnimMontage() const
    {
        return animMontage;
    }

    /**
     * Gets the gameplay tag associated with this action.
     * @return The FGameplayTag identifying this action.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FGameplayTag GetActionTag() const
    {
        return ActionTag;
    }

    bool IsFullyInit() const;

protected:
    /**
     * Called on the server when the action is successfully triggered.
     */
    UFUNCTION(BlueprintNativeEvent, DisplayName = "ServerAbilityStarted", Category = ACF)
    void OnActionStarted();

    /**
     * Called on the clients when the action is successfully triggered.
     */
    UFUNCTION(BlueprintNativeEvent, DisplayName = "ClientAbilityStarted", Category = ACF)
    void ClientsOnActionStarted();

    /**
     * Called on the server when the action finishes.
     * Usually triggered by montage ending if BindActionToAnimation is true.
     */
    UFUNCTION(BlueprintNativeEvent, DisplayName = "ServerEndAbility", Category = ACF)
    void OnActionEnded();

    /**
     * Called on the clients when the action finishes.
     */
    UFUNCTION(BlueprintNativeEvent, DisplayName = "ClientEndAbility", Category = ACF)
    void ClientsOnActionEnded();

    /**
     * Plays any gameplay effects or visual/audio effects related to this action.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ACF)
    void PlayEffects();

    /**
     * Deprecated. Use CanActivateAbility() instead.
     * Custom logic for determining whether this action can be executed.
     * @param owner Optional character context.
     * @return True if the action can be executed.
     */
    UFUNCTION(BlueprintNativeEvent, meta = (DeprecatedFunction, DeprecationMessage = "Use CanActivateAbility() instead"), Category = ACF)
    bool CanExecuteAction(class ACharacter* owner = nullptr) const;

    /**
     * Gets the name of the montage section to play.
     * @return The section name to be played.
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    FName GetMontageSectionName();

    /**
     * Fills warp reproduction info when montage type is set to Warp.
     * @param outWarpInfo Output parameter to fill with warp data.
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    void GetWarpInfo(FACFWarpReproductionInfo& outWarpInfo);

    /**
     * Gets the transform to be used for the warp target.
     * Only called if MontageReproductionType is set to Warp.
     * @return The transform of the warp target.
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    FTransform GetWarpTransform();

    /**
     * Gets the component used as warp target.
     * Only called if MontageReproductionType is set to Warp.
     * @return The scene component to warp to.
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    class USceneComponent* GetWarpTargetComponent();

    /**
     * Called when a notable point in the animation is reached (via ACFNotifyAction).
     */
    UFUNCTION(BlueprintNativeEvent, DisplayName = "NotablePointReached", Category = ACF)
    void OnNotablePointReached();

    /**
     * Client-side handler for notable points.
     */
    UFUNCTION(BlueprintNativeEvent, DisplayName = "ClientNotablePoint", Category = ACF)
    void ClientsOnNotablePointReached();

    /**
     * Called when a gameplay event tied to this action is received.
     * @param eventTag The tag of the received gameplay event.
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    void OnGameplayEventReceived(const FGameplayTag eventTag);

    /** Called when the ability system is initialized with a pawn avatar. */
    UFUNCTION(BlueprintImplementableEvent, Category = ACF, DisplayName = "OnPawnAvatarSet")
    void K2_OnPawnAvatarSet();

    /**
     * Returns the play rate of the animation montage.
     * @return Animation playback speed multiplier.
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    float GetPlayRate() const;

    /**
     * Returns the play rate of the animation montage.
     * @return Animation playback speed multiplier.
     */
    UFUNCTION(BlueprintNativeEvent, Category = ACF)
    UAnimMontage* GetMontage() const;

    /**
     * Starts the execution of the action, including animation playback.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ExecuteAction() ;

    /**
     * Sets the montage reproduction strategy.
     * @param reproType The reproduction type to use.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetMontageReproductionType(EMontageReproductionType reproType);

    /**
     * Ends the action explicitly. Doesn't stop animation, but handles logic reset.
     * @param bCancelled If true, the action was interrupted or cancelled.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void ExitAction(bool bCancelled = false);

    /**
     * Returns the action manager component associated with this ability.
     *
     * @return Pointer to the UACFAbilitySystemComponent.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE UACFAbilitySystemComponent* GetActionsManager() const { return ActionsManager; }

    /**
     * Returns the character that owns this ability.
     *
     * @return Pointer to the owning ACharacter.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE ACharacter* GetCharacterOwner() const { return CharacterOwner; }

    /*Configurable*/
    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = ACF)
    UAnimMontage* animMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
    FActionConfig ActionConfig;

    UPROPERTY(BlueprintReadWrite, Category = ACF)
    FACFMontageInfo MontageInfo;

    UPROPERTY(BlueprintReadOnly, Category = ACF)
    FGameplayTag ActionTag;

    /* Deprecated: use GetActionsManager() instead */
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    TObjectPtr<class UACFAbilitySystemComponent> ActionsManager;

    /* Deprecated: use GetCharacterOwner() instead */
    UPROPERTY(BlueprintReadOnly, Category = ACF)
    TObjectPtr<class ACharacter> CharacterOwner;

    bool bIsExecutingAction = false;

    bool bAutoCommit = true;

    virtual void Internal_OnActivated(class UACFAbilitySystemComponent* actionmanger, class UAnimMontage* inAnimMontage);
    virtual void Internal_OnDeactivated();

    void PrepareMontageInfo();

    UWorld* GetWorld() const override { return GetCharacterOwner() ? GetCharacterOwner()->GetWorld() : nullptr; }

    void PlayCurrentMontage();

    //**GAMEPLAY ABILITY OVERRIDE **/
    virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData) override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr)
        const override;
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const;
    virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
    virtual bool CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
    virtual bool CommitAbilityCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool ForceCooldown, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual float GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const;
    virtual const FGameplayTagContainer* GetCooldownTags() const;
    bool UsingDefaultCooldown() const;

    bool bBindActionToAnimation = true;

private:
    // Init Value From a Template Action
    void InitAbility();
    UFUNCTION()
    void HandleMontageFinished();

    UFUNCTION()
    void HandleMontageInterrupted();

    void InitWarp();
    void UpdateWarp();
    FGameplayAbilityActorInfo actorInfo;
    FGameplayAbilityActivationInfo activationInfo;
    FGameplayAbilitySpecHandle selfHandle;
    TObjectPtr<UARSStatisticsComponent> StatisticComp;

    UFUNCTION()
    void HandleGameplayEventReceived(FGameplayEventData Payload);

    FActiveGameplayEffectHandle executionEffect;

    FGameplayTagContainer cooldownTags;

    bool bFullyInit;
};
