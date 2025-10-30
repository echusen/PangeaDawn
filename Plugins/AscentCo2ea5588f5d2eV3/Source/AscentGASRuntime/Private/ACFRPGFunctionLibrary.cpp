// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ACFRPGFunctionLibrary.h"
#include "ACFRPGTypes.h"
#include <AbilitySystemComponent.h>
#include <GameplayEffect.h>

bool UACFRPGFunctionLibrary::TryGetModifiersFromGameplayEffect(const FGameplayEffectConfig& effectClass, TArray<FGEModifier>& outModifiers)
{
    if (effectClass.Effect) {
        UGameplayEffect* GameplayEffect = effectClass.Effect->GetDefaultObject<UGameplayEffect>();
        bool bFound = false;
        for (const FGameplayModifierInfo& ModifierInfo : GameplayEffect->Modifiers) {
            // Gets the modified attribute
            FGameplayAttribute Attribute = ModifierInfo.Attribute;
            if (!Attribute.IsValid()) {
                continue;
            }

            // Calculate his magnitued
            float ModifierMagnitude;
            if (ModifierInfo.ModifierMagnitude.GetStaticMagnitudeIfPossible(effectClass.Level, ModifierMagnitude)) {
                outModifiers.Add(FGEModifier(Attribute, ModifierMagnitude));
                bFound = true;
            }
        }
        return bFound;
    }
    return false;
}

bool UACFRPGFunctionLibrary::TryGetModifiersFromGameplayEffects(const TArray<FGameplayEffectConfig>& effects, TArray<FGEModifier>& outModifiers)
{
    bool bFound = false;
    for (const auto effect : effects) {
        TArray<FGEModifier> modifiers;
        if (TryGetModifiersFromGameplayEffect(effect, modifiers)) {
            outModifiers.Append(modifiers);
            bFound = true;
        }
    }
    return bFound;
}

FActiveGameplayEffectHandle UACFRPGFunctionLibrary::AddGameplayEffectToActor(FGameplayEffectConfig effect, AActor* targetActor)
{
    if (targetActor && effect.Effect) {
        UAbilitySystemComponent* abilityComp = targetActor->FindComponentByClass<UAbilitySystemComponent>();
        if (abilityComp) {
            FGameplayEffectContextHandle EffectContext = abilityComp->MakeEffectContext();
            UGameplayEffect* GameplayEffect = effect.Effect->GetDefaultObject<UGameplayEffect>();

            return abilityComp->ApplyGameplayEffectToSelf(GameplayEffect, effect.Level, EffectContext);
        }
    }
    return FActiveGameplayEffectHandle();
}

void UACFRPGFunctionLibrary::RemovesActiveGameplayEffectFromActor(const FActiveGameplayEffectHandle& effect, AActor* targetActor)
{
    if (targetActor) {
        UAbilitySystemComponent* abilityComp = targetActor->FindComponentByClass<UAbilitySystemComponent>();
        if (abilityComp) {
            abilityComp->RemoveActiveGameplayEffect(effect);
        }
    }
}

void UACFRPGFunctionLibrary::AddGameplayTagToActor(AActor* TargetActor, const FGameplayTag& TagToAdd)
{
    if (!TargetActor) {
        UE_LOG(LogTemp, Warning, TEXT("TargetActor is null in AddGameplayTagToASC"));
        return;
    }

    UAbilitySystemComponent* ASC = TargetActor->FindComponentByClass<UAbilitySystemComponent>();
    if (!ASC) {
        UE_LOG(LogTemp, Warning, TEXT("No ASC found on %s"), *TargetActor->GetName());
        return;
    }

    ASC->AddReplicatedLooseGameplayTag(TagToAdd);
}

void UACFRPGFunctionLibrary::RemoveGameplayTagFromActor(AActor* TargetActor, const FGameplayTag& TagToAdd)
{
    if (!TargetActor) {
        UE_LOG(LogTemp, Warning, TEXT("TargetActor is null in AddGameplayTagToASC"));
        return;
    }

    UAbilitySystemComponent* ASC = TargetActor->FindComponentByClass<UAbilitySystemComponent>();
    if (!ASC) {
        UE_LOG(LogTemp, Warning, TEXT("No ASC found on %s"), *TargetActor->GetName());
        return;
    }

    ASC->RemoveLooseGameplayTag(TagToAdd);
}
