// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFRPGTypes.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ACFRPGFunctionLibrary.generated.h"

/**
 *
 */
UCLASS()
class ASCENTGASRUNTIME_API UACFRPGFunctionLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()

public:
    /**
     * Attempts to extract the float values of the modifiers from a single GameplayEffect.
     *
     * @param effectClass The configuration of the GameplayEffect to inspect.
     * @param outModifiers The array that will be filled with extracted modifiers.
     * @return true if at least one modifier was extracted successfully, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    static bool TryGetModifiersFromGameplayEffect(const FGameplayEffectConfig& effectClass, TArray<FGEModifier>& outModifiers);

    /**
     * Attempts to extract the float values of the modifiers from multiple GameplayEffects.
     *
     * @param effects The list of GameplayEffect configurations to inspect.
     * @param outModifiers The array that will be filled with extracted modifiers.
     * @return true if at least one modifier was extracted successfully, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    static bool TryGetModifiersFromGameplayEffects(const TArray<FGameplayEffectConfig>& effects, TArray<FGEModifier>& outModifiers);

    /**
     * Applies the specified GameplayEffect to the target Actor's Ability System Component.
     *
     * @param effect The GameplayEffect configuration to apply.
     * @param targetActor The actor who will receive the effect.
     * @return The handle to the applied GameplayEffect, or an invalid handle if application failed.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    static FActiveGameplayEffectHandle AddGameplayEffectToActor(FGameplayEffectConfig effect, AActor* targetActor);

    /**
     * Attempts to remove the specified active GameplayEffect from the target Actor's Ability System Component.
     *
     * @param effect The handle of the active GameplayEffect to remove.
     * @param targetActor The actor from whom the effect should be removed.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    static void RemovesActiveGameplayEffectFromActor(const FActiveGameplayEffectHandle& effect, AActor* targetActor);

    /**
     * Adds a loose gameplay tag to the Ability System Component of the given actor.
     * This tag will be considered by the GAS for ability activation checks and tag queries.
     *
     * @param TargetActor The actor that owns the ASC.
     * @param TagToAdd The gameplay tag to add.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    static void AddGameplayTagToActor(AActor* TargetActor, const FGameplayTag& TagToAdd);

    /**
     * Remove a loose gameplay tag to the Ability System Component of the given actor.
     * This tag will be considered by the GAS for ability activation checks and tag queries.
     *
     * @param TargetActor The actor that owns the ASC.
     * @param TagToAdd The gameplay tag to add.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    static void RemoveGameplayTagFromActor(AActor* TargetActor, const FGameplayTag& TagToRemove);
};
