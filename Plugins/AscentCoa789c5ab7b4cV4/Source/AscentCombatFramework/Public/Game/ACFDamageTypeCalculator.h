// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFDamageType.h"
#include "ACFDeveloperSettings.h"
#include "ACFTypes.h"
#include "CoreMinimal.h"
#include "Game/ACFBaseDamageTypeCalculator.h"
#include "Game/ACFDamageCalculation.h"
#include "GameFramework/DamageType.h"
#include "GameplayTagContainer.h"

#include "ACFDamageTypeCalculator.generated.h"

class UDamageType;
struct FDamageInfluence;
struct FOnHitActionChances;

/**
 *
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API UACFDamageTypeCalculator : public UACFBaseDamageTypeCalculator {
    GENERATED_BODY()

    UACFDamageTypeCalculator();

protected:
    /*For every damagetype, the parameter to be used to calculate crit chance. 100 means always crit*/

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF| Critical Damage Config")
    TMap<TSubclassOf<UDamageType>, FDamageInfluence> CritChancePercentageByParameter;

    /*This statistic is used to prevent the owner to go on Hit Action ( = being staggered).
    If set, this statistic will be reduced by the actual damage at every hit and the owner
    will only be staggered when this statistic reaches 0. (make sure regen is activate, otherwise
    the AI after the first stagger, will be always be staggered) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|Hit Responses")
    FGameplayTag StaggerResistanceStastistic;

    /*The parameter to be used to reduce incoming damage when in Defense State*/

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF")
    FGameplayTag DefenseStanceParameterWhenBlocked;

    virtual float CalculateFinalDamage_Implementation(const FACFDamageEvent& inDamageEvent, TArray<FAttributeData>& otherAffectedAttributes) override;

    virtual FGameplayTag EvaluateHitResponseAction_Implementation(const FACFDamageEvent& inDamageEvent,
        const TArray<FOnHitActionChances>& hitResponseActions) override;

    virtual bool IsCriticalDamage_Implementation(const FACFDamageEvent& inDamageEvent) override;

    virtual float Internal_CalculateDamage(const FACFDamageEvent& inDamageEvent) override;

private:
};
