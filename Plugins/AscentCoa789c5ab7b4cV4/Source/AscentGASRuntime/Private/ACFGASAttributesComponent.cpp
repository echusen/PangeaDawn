// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "ACFGASAttributesComponent.h"
#include "ACFGASDeveloperSettings.h"
#include "ACFGASTypes.h"
#include "AttributeSet.h"
#include "Engine/DataTable.h"
#include <AbilitySystemComponent.h>
#include <Engine/DataTable.h>
#include <GameplayEffect.h>
#include <GameplayEffectTypes.h>

// Sets default values for this component's properties
UACFGASAttributesComponent::UACFGASAttributesComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;

    bLoaded = false;
}

UAbilitySystemComponent* UACFGASAttributesComponent::GetOwnerAbilityComponent() const
{
    return abilityComp;
}

void UACFGASAttributesComponent::AssignPerkToAttribute_Implementation(FGameplayAttribute attribute, int32 numPerks /*= 1*/)
{
    if (GetAvailablePerks() >= numPerks) {
        float newValue = abilityComp->GetNumericAttributeBase(attribute);
        newValue += numPerks;
        abilityComp->SetNumericAttributeBase(attribute, newValue);
        ConsumePerks(numPerks);
    }
}

void UACFGASAttributesComponent::SetCharacterRow(FDataTableRowHandle val)
{
    CharacterRow = val;
}

void UACFGASAttributesComponent::InitializeAttributeSet()
{
    InitAttributesValue();
    ApplyPermanentEffects();
}

// Called when the game starts
void UACFGASAttributesComponent::BeginPlay()
{
    Super::BeginPlay();
    abilityComp = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
    allAttributes = GetAttributeToSave();
    if (GetOwner()->HasAuthority() && bAutoInitialize) {
        InitializeAttributeSet();
    }
}

void UACFGASAttributesComponent::InitAttributesValue()
{
    if (abilityComp) {
        for (const auto& startData : abilityComp->DefaultStartingData) {
            abilityComp->InitStats(startData.Attributes, startData.DefaultStartingTable);
        }
        if (bLoaded) {
            return;
        }
        if (GetLevelingType() != ELevelingType::EGenerateNewStatsFromCurves) {
            InitAttributesFromDT();

        } else {
            InitAttributesFromLevelCurves();
        }
    }
}

void UACFGASAttributesComponent::InitAttributesFromDT()
{
    for (const auto& startData : abilityComp->DefaultStartingData) {
        if (!startData.DefaultStartingTable) {
            continue;
        }
        const FACFAttributeInits* attInits = startData.DefaultStartingTable->FindRow<FACFAttributeInits>(CharacterRow.RowName, "");

        if (!attInits) {
            continue;
        }

        for (const auto& attribute : attInits->PawnAttributesInit) {
            if (abilityComp->HasAttributeSetForAttribute(attribute.Attribute)) {
                abilityComp->SetNumericAttributeBase(attribute.Attribute, attribute.InitValue);
            }
        }
    }
}

void UACFGASAttributesComponent::InitAttributesFromLevelCurves()
{
    if (!abilityComp || !AttributesByLevelCurve) {
        return;
    }

    const UACFGASDeveloperSettings* Settings = GetDefault<UACFGASDeveloperSettings>();

    const UDataTable* AttributesByCurveRow = Settings->GetAttributesByCurveRow();

    if (!AttributesByCurveRow) {
        return;
    }

    for (const TPair<FName, uint8*>& Elem : AttributesByCurveRow->GetRowMap()) {
        const FName& RowName = Elem.Key;
        const FAttributeSerializeKeys* AttributeKey = AttributesByCurveRow->FindRow<FAttributeSerializeKeys>(Elem.Key, TEXT("InitAttributes"));

        if (!AttributeKey) {
            continue;
        }

        const FRealCurve* Curve = AttributesByLevelCurve->FindCurve(RowName, FString());
        if (Curve) {
            const float Value = Curve->Eval(static_cast<float>(CharacterLevel));
            if (abilityComp->HasAttributeSetForAttribute(AttributeKey->Attribute)) {
                abilityComp->SetNumericAttributeBase(AttributeKey->Attribute, Value);
            }
        } else {
            UE_LOG(LogTemp, Warning, TEXT("Missing curve row for attribute: %s"), *RowName.ToString());
        }
    }
}

void UACFGASAttributesComponent::ApplyPermanentEffects()
{
    UAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilityComponent();
    if (AbilitySystemComponent) {
        permanentEffects.Empty();
        for (const auto& effect : StartingEffects) {
            FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
            FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(effect, 1.0f, EffectContext);
            if (SpecHandle.IsValid()) {
                permanentEffects.Add(AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()));
            }
        }
    }
}

void UACFGASAttributesComponent::OnComponentLoaded_Implementation()
{
    if (allAttributes && abilityComp) {

        for (const auto& attribute : SaveableAttributes) {
            FAttributeSerializeKeys* attName = allAttributes->FindRow<FAttributeSerializeKeys>(attribute.AttributeName, "");
            if (attName) {
                abilityComp->SetNumericAttributeBase(attName->Attribute, attribute.Value);
            }
        }
    }
}

void UACFGASAttributesComponent::OnComponentSaved_Implementation()
{
    SaveableAttributes.Empty();

    if (abilityComp && allAttributes) {
        const auto allAttStruct = allAttributes->GetRowMap();
        for (const auto& att : allAttStruct) {
            FAttributeSerializeKeys* AttSerialize = (FAttributeSerializeKeys*)att.Value;
            const float baseValue = abilityComp->GetNumericAttributeBase(AttSerialize->Attribute);
            SaveableAttributes.Add(FAttributeSerializeNames(att.Key, baseValue));
        }
    }
}

UDataTable* UACFGASAttributesComponent::GetAttributeToSave() const
{

    UACFGASDeveloperSettings* settings = GetMutableDefault<UACFGASDeveloperSettings>();

    if (settings) {
        return settings->GetAttributeKeys();
    }
    return nullptr;
}
