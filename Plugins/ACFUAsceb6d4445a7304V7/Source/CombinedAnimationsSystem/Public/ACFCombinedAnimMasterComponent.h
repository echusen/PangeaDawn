// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ACFCombinedAnimComponent.h"
#include "CASTypes.h"

#include "ACFCombinedAnimMasterComponent.generated.h"

class UACFCombinedAnimationsDataAsset;

/**
 * Component used on characters that act as masters in a combined animation scenario.
 * Responsible for selecting and evaluating appropriate combined animations and executions
 */
UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class COMBINEDANIMATIONSSYSTEM_API UACFCombinedAnimMasterComponent : public UACFCombinedAnimComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFCombinedAnimMasterComponent();

    /**
     * Checks if the given actor is eligible to play the specified combined animation with this master.
     *
     * @param combinedAnim The animation configuration to validate.
     * @param targetActor The actor to evaluate as a potential animation partner.
     * @return True if the animation can be played with the given actor.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    bool CanPlayCombinedAnimWithActor(const FCombinedAnimConfig& combinedAnim, AActor* targetActor);

    /**
     * Searches for the most suitable combined animation that can be played with the given actor.
     *
     * @param targetActor The potential animation partner.
     * @param outCombinedAnim The best matching animation config found.
     * @return True if a suitable combined animation was found.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool TryGetBestCombinedAnimForActor(AActor* targetActor, FCombinedAnimConfig& outCombinedAnim);

    /**
     * Returns all combined animation configs whose AnimTag matches the given tag.
     *
     * @param AnimTag The gameplay tag to filter by.
     * @param outCombinedAnims Output array filled with matching animation configs.
     * @return True if at least one matching animation was found.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GetContextualAnimsByTag(const FGameplayTag& AnimTag, TArray<FCombinedAnimConfig>& outCombinedAnims);

    /**
     * Returns the combined animation that is currently playing, or the last one that has been selected.
     *
     * @return The cached combined animation config.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FCombinedAnimConfig GetCurrentCombinedAnim() const { return currentCombinedAnim; }

    /**
     * Caches the combined animation that is about to be played. Called automatically by
     * TryGetBestCombinedAnimForActor; call it manually when the config is picked somewhere else,
     * so that the component can play the right camera sequence once the scene starts.
     *
     * @param inCombinedAnim The animation config that is going to be played.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetCurrentCombinedAnim(const FCombinedAnimConfig& inCombinedAnim) { currentCombinedAnim = inCombinedAnim; }

protected:
    /** Reference to the data asset containing all possible combined animations for this master */
    UPROPERTY(EditAnywhere, Category = ACF)
    UACFCombinedAnimationsDataAsset* CombinedAnimations;

    /** The combined animation currently playing, cached when it gets selected */
    UPROPERTY()
    FCombinedAnimConfig currentCombinedAnim;

    // Called when the game starts
    virtual void BeginPlay() override;

    /** Starts the camera sequence of the current combined anim, if any */
    virtual void OnJoinedScene_Implementation() override;

    /** Stops the camera sequence started by this component, if any */
    virtual void OnLeftScene_Implementation() override;

private:
    void TriggerCurrentCameraSequence();

    /** True while this component is the one that started the camera sequence in play */
    bool bHasTriggeredCameraSequence = false;
};
