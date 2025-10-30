// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.
#include "Game/ACFGASDamageCalculator.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFDefenseStanceComponent.h"
#include "Game/ACFDamageType.h"
#include "Game/ACFFunctionLibrary.h"
#include <AbilitySystemComponent.h>
#include <GameplayTagContainer.h>

bool UACFGASDamageCalculator::IsCriticalDamage_Implementation(const FACFDamageEvent& inDamageEvent)
{
    if (inDamageEvent.DamageDealer) {
        const UAbilitySystemComponent* dealerComp = inDamageEvent.DamageDealer->FindComponentByClass<UAbilitySystemComponent>();

        if (dealerComp) {
            const float critChance = dealerComp->GetNumericAttribute(CritChanceAttribute);
            if (FMath::RandRange(0.f, 100.f) < critChance) {
                return true;
            }
        }
    }
    return false;
}

float UACFGASDamageCalculator::CalculateFinalDamage_Implementation(const FACFDamageEvent& inDamageEvent, TArray<FAttributeData>& otherAffectedAttributes)
{
    // Verify received input data
    if (!inDamageEvent.DamageReceiver) {
        UE_LOG(LogTemp, Error, TEXT("Missing Damage receiver!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }

    if (!inDamageEvent.DamageDealer) {
        UE_LOG(LogTemp, Error, TEXT("Missing Damage Dealer!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }

    if (!inDamageEvent.DamageClass) {
        UE_LOG(LogTemp, Error, TEXT("Missing Damage Class!!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }
    UACFDamageType* damageClass = GetDamageType(inDamageEvent);

    if (!damageClass) {
        UE_LOG(LogTemp, Error, TEXT("DamageClass Influence NOT Set!!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }
    FGASDamageInfluences damagesInf = damageClass->GASDamageScaling;

    // starting from the base damage
    float totalDamage = Internal_CalculateDamage(inDamageEvent);
    const float finalStaggerDamage = totalDamage * damageClass->StaggerMutliplier;

    const UAbilitySystemComponent* dealerComp = inDamageEvent.DamageDealer->FindComponentByClass<UAbilitySystemComponent>();
    UAbilitySystemComponent* receiverComp = inDamageEvent.DamageReceiver->FindComponentByClass<UAbilitySystemComponent>();

    UACFDefenseStanceComponent* defComp = inDamageEvent.DamageReceiver->FindComponentByClass<UACFDefenseStanceComponent>();
    FGameplayTag outResponse;

    if (defComp && inDamageEvent.HitResponseAction == defComp->GetBlockAction()) {
        otherAffectedAttributes.Add(FAttributeData(BlockDamagedDataTag, finalStaggerDamage * defComp->GetDamagedStatisticMultiplier()));
    }

  
    if (finalStaggerDamage > 0.f) {
        otherAffectedAttributes.Add(FAttributeData(StaggerResistanceDataTag, finalStaggerDamage));
    }

        // Damage Zones
    const float* zoneMult = DamageZoneToDamageMultiplier.Find(inDamageEvent.DamageZone);
    if (zoneMult) {
        totalDamage *= *zoneMult;
    }
    if (defComp && defComp->GetBlockAction() == inDamageEvent.HitResponseAction) {
        const float reducedPercentage = receiverComp->GetNumericAttribute(BlockingDefenseAttribute);
        totalDamage = UACFFunctionLibrary::ReduceDamageByPercentage(totalDamage, reducedPercentage);
    }


    // Hit Responses
    const float* hitMult = HitResponseActionMultiplier.Find(inDamageEvent.HitResponseAction);
    if (hitMult) {
        totalDamage *= *hitMult;
    }

    return totalDamage;
}

FGameplayTag UACFGASDamageCalculator::EvaluateHitResponseAction_Implementation(const FACFDamageEvent& damageEvent,
    const TArray<FOnHitActionChances>& hitResponseActions)
{
    UACFDefenseStanceComponent* defComp = damageEvent.DamageReceiver->FindComponentByClass<UACFDefenseStanceComponent>();
    FGameplayTag outResponse = FGameplayTag();

    if (!damageEvent.DamageDealer) {
        return FGameplayTag();
    }
    UACFDamageType* damageType = GetDamageType(damageEvent);
    const float DamgeTemp = Internal_CalculateDamage(damageEvent);

    // Evaluate Blocks or Parry
    if (defComp && defComp->IsInDefensePosition() && defComp->TryBlockIncomingDamage(damageEvent, DamgeTemp, outResponse) && outResponse != FGameplayTag()) {
        return outResponse;
    }

    if (defComp && defComp->TryCounterAttack(damageEvent, outResponse)) {
        return outResponse;
    }
    const AACFCharacter* charOwn = Cast<AACFCharacter>(damageEvent.DamageReceiver);
    if (charOwn) {
        TArray<float> weights;
        TArray<FActionChances> executableActions;
        for (const auto& elem : hitResponseActions) {
            if (UACFFunctionLibrary::ShouldExecuteAction(elem, charOwn) && EvaluetHitResponseAction(elem, damageEvent)) {
                executableActions.Add(elem);
                weights.Add(elem.Weight);
            }
        }
        const int32 index = UACFFunctionLibrary::ExtractIndexWithProbability(weights);
        if (executableActions.IsValidIndex(index)) {
            outResponse = executableActions[index].ActionTag;
        }
    }

    UAbilitySystemComponent* receiverComp = damageEvent.DamageReceiver->FindComponentByClass<UAbilitySystemComponent>();

    if (receiverComp && damageType && outResponse == UACFFunctionLibrary::GetDefaultHitState()) {

        if (receiverComp->GetNumericAttribute(StaggerResistanceAttribute) > DamgeTemp) {
            return FGameplayTag();
        }
    }

    if (receiverComp && HeavyHitReaction != FGameplayTag()) {

        const float currentResistance = receiverComp->GetNumericAttribute(StaggerResistanceAttribute);
        const float HeavyHitThreshold = -StaggerResistanceForHeavyHitMultiplier * receiverComp->GetNumericAttribute(MaxStaggerResistanceAttribute);
        if (currentResistance < HeavyHitThreshold) {
            outResponse = HeavyHitReaction;
        }
    }

    return outResponse;
}



float UACFGASDamageCalculator::Internal_CalculateDamage(const FACFDamageEvent& inDamageEvent)
{
    float totalDamage = inDamageEvent.FinalDamage;

    UACFDamageType* damageClass = GetDamageType(inDamageEvent);
    FGASDamageInfluences damagesInf = damageClass->GASDamageScaling;

    const UAbilitySystemComponent* dealerComp = inDamageEvent.DamageDealer->FindComponentByClass<UAbilitySystemComponent>();
    UAbilitySystemComponent* receiverComp = inDamageEvent.DamageReceiver->FindComponentByClass<UAbilitySystemComponent>();

    // First we calculate the sum of every parameter influence
    for (const auto& damInf : damagesInf.AttackParametersInfluence) {
        totalDamage += dealerComp->GetNumericAttribute(damInf.Parameter) * damInf.ScalingFactor;
    }

    // Then reduces it for defenses
    for (const auto& damInf : damagesInf.DefenseParametersPercentages) {
        totalDamage = UACFFunctionLibrary::ReduceDamageByPercentage(totalDamage,
            receiverComp->GetNumericAttribute(damInf.Parameter) * damInf.ScalingFactor);
    }

    if (inDamageEvent.bIsCritical) {
        totalDamage *= critMultiplier;
    }

    // Final Randomization
    if (totalDamage != 0.f) {
        const float deviation = totalDamage * DefaultRandomDamageDeviationPercentage / 100;
        totalDamage = FMath::FRandRange(totalDamage - deviation, totalDamage + deviation);
    }

    return totalDamage;
}

bool UACFGASDamageCalculator::EvaluetHitResponseAction(const FOnHitActionChances& action, const FACFDamageEvent& damageEvent)
{
    if ((uint8)damageEvent.DamageDirection != (uint8)action.AllowedFromDirection && action.AllowedFromDirection != EActionDirection::EveryDirection)
        return false;

    for (const TSubclassOf<UDamageType>& damageType : action.AllowedDamageTypes) {
        if (damageEvent.DamageClass->IsChildOf(damageType))
            return true;
    }
    return false;
}
