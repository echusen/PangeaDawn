// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Game/ACFDamageTypeCalculator.h"
#include "ARSStatisticsComponent.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFDefenseStanceComponent.h"
#include "Game/ACFDamageType.h"
#include "Game/ACFFunctionLibrary.h"
#include "Game/ACFTypes.h"
#include "Logging.h"
#include "ACFLegacyStatisticsComponent.h"

UACFDamageTypeCalculator::UACFDamageTypeCalculator()
{
}

float UACFDamageTypeCalculator::Internal_CalculateDamage(const FACFDamageEvent& inDamageEvent)
{

    // Verify received input data
    if (!inDamageEvent.DamageReceiver) {
        UE_LOG(ACFLog, Error, TEXT("Missing Damage receiver!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }

    if (!inDamageEvent.DamageDealer) {
        UE_LOG(ACFLog, Error, TEXT("Missing Damage Dealer!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }

    if (!inDamageEvent.DamageClass) {
        UE_LOG(ACFLog, Error, TEXT("Missing Damage Class!!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }
    UACFDamageType* damageClass = GetDamageType(inDamageEvent);

    if (!damageClass) {
        UE_LOG(ACFLog, Error, TEXT("DamageClass Influence NOT Set!!!! - UACFDamageCalculation::CalculateFinalDamage"));
        return inDamageEvent.FinalDamage;
    }
    float totalDamage = inDamageEvent.FinalDamage;

    FDamageInfluences damagesInf = damageClass->DamageScaling;
    const UACFLegacyStatisticsComponent* dealerComp = inDamageEvent.DamageDealer->FindComponentByClass<UACFLegacyStatisticsComponent>();
    UACFLegacyStatisticsComponent* receiverComp = inDamageEvent.DamageReceiver->FindComponentByClass<UACFLegacyStatisticsComponent>();

    // First we calculate the sum of every parameter influence
    for (const auto& damInf : damagesInf.AttackParametersInfluence) {
        totalDamage += dealerComp->GetCurrentAttributeValue(damInf.Parameter) * damInf.ScalingFactor;
    }

    for (const auto& damInf : damagesInf.DefenseParametersPercentages) {
        totalDamage = UACFFunctionLibrary::ReduceDamageByPercentage(totalDamage,
            receiverComp->GetCurrentAttributeValue(damInf.Parameter) * damInf.ScalingFactor);
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

bool UACFDamageTypeCalculator::IsCriticalDamage_Implementation(const FACFDamageEvent& inDamageEvent)
{
    if (inDamageEvent.DamageDealer) {
        const FDamageInfluence* critChance = CritChancePercentageByParameter.Find(inDamageEvent.DamageClass);
        const UACFLegacyStatisticsComponent* dealerComp = inDamageEvent.DamageDealer->FindComponentByClass<UACFLegacyStatisticsComponent>();

        if (critChance && dealerComp) {
            const float percentage = dealerComp->GetCurrentAttributeValue(critChance->Parameter) * critChance->ScalingFactor;
            if (FMath::RandRange(0.f, 100.f) < percentage) {
                return true;
            }
        }
    }
    return false;
}

float UACFDamageTypeCalculator::CalculateFinalDamage_Implementation(const FACFDamageEvent& inDamageEvent, TArray<FAttributeData>& otherAffectedAttributes)
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

    //  const FDamageInfluences* damagesInf = DamageInfluencesByParameter.Find(inDamageEvent.DamageClass);

    // starting from the base damage
    float totalDamage = Internal_CalculateDamage(inDamageEvent);

    const UACFLegacyStatisticsComponent* dealerComp = inDamageEvent.DamageDealer->FindComponentByClass<UACFLegacyStatisticsComponent>();
    UACFLegacyStatisticsComponent* receiverComp = inDamageEvent.DamageReceiver->FindComponentByClass<UACFLegacyStatisticsComponent>();
    UACFDefenseStanceComponent* defComp = inDamageEvent.DamageReceiver->FindComponentByClass<UACFDefenseStanceComponent>();
    const float finalDamgeTemp = totalDamage * damageClass->StaggerMutliplier;

    if (defComp && inDamageEvent.HitResponseAction == defComp->GetBlockAction()) {
        otherAffectedAttributes.Add(FAttributeData(defComp->GetDamagedStatistic(), finalDamgeTemp * defComp->GetDamagedStatisticMultiplier()));
    }

    if (finalDamgeTemp > 0.f) {
        otherAffectedAttributes.Add(FAttributeData(StaggerResistanceStastistic, finalDamgeTemp));
    }
    // Block Check
    if (defComp && inDamageEvent.HitResponseAction == defComp->GetBlockAction()) {
        const float reducedPercentage = receiverComp->GetCurrentAttributeValue(DefenseStanceParameterWhenBlocked);
        totalDamage = UACFFunctionLibrary::ReduceDamageByPercentage(totalDamage, reducedPercentage);
    }

    // Damage Zones
    const float* zoneMult = DamageZoneToDamageMultiplier.Find(inDamageEvent.DamageZone);
    if (zoneMult) {
        totalDamage *= *zoneMult;
    }

    // Hit Responses
    const float* hitMult = HitResponseActionMultiplier.Find(inDamageEvent.HitResponseAction);
    if (hitMult) {
        totalDamage *= *hitMult;
    }

    return totalDamage;
}

FGameplayTag UACFDamageTypeCalculator::EvaluateHitResponseAction_Implementation(const FACFDamageEvent& damageEvent, const TArray<FOnHitActionChances>& hitResponseActions)
{
    UACFDefenseStanceComponent* defComp = damageEvent.DamageReceiver->FindComponentByClass<UACFDefenseStanceComponent>();
    FGameplayTag outResponse;

    if (!damageEvent.DamageDealer) {
        return FGameplayTag();
    }

    UACFDamageType* damageType = GetDamageType(damageEvent);
    const float DamgeTemp = Internal_CalculateDamage(damageEvent);
    const float finalDamgeTemp = DamgeTemp * damageType->StaggerMutliplier;
    UARSStatisticsComponent* receiverComp = damageEvent.DamageReceiver->FindComponentByClass<UARSStatisticsComponent>();

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

    if (receiverComp && damageType && StaggerResistanceStastistic != FGameplayTag() && outResponse == UACFFunctionLibrary::GetDefaultHitState()) {

        // receiverComp->ModifyStatistic(StaggerResistanceStastistic, -finalDamgeTemp);
        if (receiverComp->GetCurrentValueForStatitstic(StaggerResistanceStastistic) > DamgeTemp) {
            return FGameplayTag();
        }
    }

    if (receiverComp && StaggerResistanceStastistic != FGameplayTag() && HeavyHitReaction != FGameplayTag()) {

        const float currentResistance = receiverComp->GetCurrentValueForStatitstic(StaggerResistanceStastistic);
        const float HeavyHitThreshold = -StaggerResistanceForHeavyHitMultiplier * receiverComp->GetMaxValueForStatitstic(StaggerResistanceStastistic);
        if (currentResistance < HeavyHitThreshold) {
            outResponse = HeavyHitReaction;
        }
    }

    return outResponse;
}

