// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFGASStatisticsComponent.h"
#include "ACFDifficultyManagerComponent.h"
#include "ACFGASDeveloperSettings.h"
#include "ACFRPGFunctionLibrary.h"
#include "ACFRPGTypes.h"
#include "ARSFunctionLibrary.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Engine/DataTable.h"
#include <GameplayEffect.h>

UACFGASStatisticsComponent::UACFGASStatisticsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bLoaded = false;
}

UAbilitySystemComponent* UACFGASStatisticsComponent::GetOwnerAbilityComponent() const
{
	return abilityComp;
}

void UACFGASStatisticsComponent::AssignPerkToAttribute_Implementation(FGameplayAttribute attribute, int32 numPerks)
{
	if (!abilityComp) {
		return;
	}

	if (GetAvailablePerks() >= numPerks) {
		const float newValue = abilityComp->GetNumericAttributeBase(attribute) + numPerks;
		abilityComp->SetNumericAttributeBase(attribute, newValue);
		ConsumePerks(numPerks);
	}
}

void UACFGASStatisticsComponent::AssignPerkToPrimaryAttribute_Implementation(FGameplayTag attributeTag, int32 numPerks)
{
	FGameplayAttribute attribute;
	if (UARSFunctionLibrary::TryGetAttributeFromSetByCallerTag(attributeTag, attribute)) {
		AssignPerkToAttribute(attribute, numPerks);
	}
}

void UACFGASStatisticsComponent::SetCharacterRow(FDataTableRowHandle val)
{
	CharacterRow = val;
}

void UACFGASStatisticsComponent::InitializeAttributeSet()
{
	InitAttributesValue();
	ApplyPermanentEffects();
	bIsInitialized = true;
	BindHealthDelegate();
}

void UACFGASStatisticsComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UACFDifficultyManagerComponent* diffComp = UACFRPGFunctionLibrary::GetDifficultyManager(this)) {
		CachedDifficultyManager = diffComp;
		if (!diffComp->OnDifficultyChanged.IsAlreadyBound(this, &UACFGASStatisticsComponent::OnDifficultyLevelChanged)) {
			diffComp->OnDifficultyChanged.AddDynamic(this, &UACFGASStatisticsComponent::OnDifficultyLevelChanged);
		}
	}

	if (GetOwner()->HasAuthority() && bAutoInitialize) {
		InitializeAttributeSet();
	} else {
		abilityComp = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
		BindHealthDelegate();
	}
}

void UACFGASStatisticsComponent::InitAttributesValue()
{
	abilityComp = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
	allAttributes = GetAttributeToSave();

	if (!abilityComp) {
		return;
	}

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

void UACFGASStatisticsComponent::InitAttributesFromDT()
{
	if (!abilityComp || !CharacterRow.DataTable) {
		return;
	}

	const FACFAttributeInits* attInits = CharacterRow.DataTable->FindRow<FACFAttributeInits>(CharacterRow.RowName, TEXT("InitAttributesFromDT"));
	if (!attInits) {
		return;
	}

	const FACFDifficultyScaling* difficultyRow = nullptr;
	if (bAffectedByDifficultyLevel) {
		difficultyRow = GetCachedDifficultyScaling();
	}

	for (const auto& attribute : attInits->PawnAttributesInit) {
		if (!abilityComp->HasAttributeSetForAttribute(attribute.Attribute)) {
			continue;
		}

		float finalValue = attribute.InitValue;
		if (difficultyRow) {
			finalValue *= GetMultiplierForAttribute(*difficultyRow, attribute.Attribute);
		}
		abilityComp->SetNumericAttributeBase(attribute.Attribute, finalValue);
	}
}

void UACFGASStatisticsComponent::InitAttributesFromLevelCurves()
{
	if (!abilityComp || !AttributesByLevelCurve) {
		return;
	}

	const UACFGASDeveloperSettings* settings = GetDefault<UACFGASDeveloperSettings>();
	const UDataTable* attributesByCurveRow = settings ? settings->GetAttributesByCurveRow() : nullptr;
	if (!attributesByCurveRow) {
		return;
	}

	const FACFDifficultyScaling* difficultyRow = nullptr;
	if (bAffectedByDifficultyLevel) {
		difficultyRow = GetCachedDifficultyScaling();
	}

	for (const TPair<FName, uint8*>& elem : attributesByCurveRow->GetRowMap()) {
		const FAttributeSerializeKeys* attributeKey = attributesByCurveRow->FindRow<FAttributeSerializeKeys>(elem.Key, TEXT("InitAttributesFromLevelCurves"));
		if (!attributeKey) {
			continue;
		}

		const FRealCurve* curve = AttributesByLevelCurve->FindCurve(elem.Key, FString());
		if (!curve) {
			UE_LOG(LogTemp, Warning, TEXT("Missing curve row for attribute: %s"), *elem.Key.ToString());
			continue;
		}

		float value = curve->Eval(static_cast<float>(CharacterLevel));
		if (!abilityComp->HasAttributeSetForAttribute(attributeKey->Attribute)) {
			continue;
		}

		if (difficultyRow) {
			value *= GetMultiplierForAttribute(*difficultyRow, attributeKey->Attribute);
		}
		abilityComp->SetNumericAttributeBase(attributeKey->Attribute, value);
	}
}

void UACFGASStatisticsComponent::ApplyPermanentEffects()
{
	if (!abilityComp) {
		return;
	}

	permanentEffects.Empty();
	for (const TSubclassOf<UGameplayEffect>& effect : StartingEffects) {
		const FGameplayEffectContextHandle effectContext = abilityComp->MakeEffectContext();
		const FGameplayEffectSpecHandle specHandle = abilityComp->MakeOutgoingSpec(effect, 1.0f, effectContext);
		if (specHandle.IsValid()) {
			permanentEffects.Add(abilityComp->ApplyGameplayEffectSpecToSelf(*specHandle.Data.Get()));
		}
	}
}

void UACFGASStatisticsComponent::OnComponentLoaded_Implementation()
{
	if (!abilityComp) {
		abilityComp = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
	}
	if (!allAttributes) {
		allAttributes = GetAttributeToSave();
	}

	if (allAttributes && abilityComp) {
		for (const FAttributeSerializeNames& attribute : SaveableAttributes) {
			if (FAttributeSerializeKeys* attName = allAttributes->FindRow<FAttributeSerializeKeys>(attribute.AttributeName, TEXT("OnComponentLoaded"))) {
				abilityComp->SetNumericAttributeBase(attName->Attribute, attribute.Value);
			}
		}
	}

	bLoaded = true;
	RefreshDifficulty();
	BindHealthDelegate();
}

void UACFGASStatisticsComponent::RefreshDifficulty()
{
	if (!bAffectedByDifficultyLevel) {
		return;
	}

	AActor* owner = GetOwner();
	if (!owner || !owner->HasAuthority()) {
		return;
	}

	if (!CachedDifficultyManager.IsValid()) {
		if (UACFDifficultyManagerComponent* diffComp = UACFRPGFunctionLibrary::GetDifficultyManager(this)) {
			CachedDifficultyManager = diffComp;
		}
	}

	if (!abilityComp) {
		abilityComp = owner->FindComponentByClass<UAbilitySystemComponent>();
	}

	if (!abilityComp) {
		return;
	}

	if (GetLevelingType() != ELevelingType::EGenerateNewStatsFromCurves) {
		InitAttributesFromDT();
	} else {
		InitAttributesFromLevelCurves();
	}
}

void UACFGASStatisticsComponent::OnComponentSaved_Implementation()
{
	SaveableAttributes.Empty();

	if (!abilityComp || !allAttributes) {
		return;
	}

	for (const TPair<FName, uint8*>& att : allAttributes->GetRowMap()) {
		const FAttributeSerializeKeys* attSerialize = reinterpret_cast<FAttributeSerializeKeys*>(att.Value);
		if (!attSerialize) {
			continue;
		}
		const float baseValue = abilityComp->GetNumericAttributeBase(attSerialize->Attribute);
		SaveableAttributes.Add(FAttributeSerializeNames(att.Key, baseValue));
	}
}

UDataTable* UACFGASStatisticsComponent::GetAttributeToSave() const
{
	if (UACFGASDeveloperSettings* settings = GetMutableDefault<UACFGASDeveloperSettings>()) {
		return settings->GetAttributeKeys();
	}
	return nullptr;
}

const FACFDifficultyScaling* UACFGASStatisticsComponent::GetCachedDifficultyScaling() const
{
	UACFDifficultyManagerComponent* diffComp = CachedDifficultyManager.IsValid()
		? CachedDifficultyManager.Get()
		: UACFRPGFunctionLibrary::GetDifficultyManager(this);

	if (!diffComp) {
		return nullptr;
	}

	const FGameplayTag difficultyLevel = diffComp->GetCurrentDifficultyLevel();
	if (!difficultyLevel.IsValid()) {
		return nullptr;
	}

	UDataTable* difficultyDT = UACFRPGFunctionLibrary::GetDifficultyScalingTable();
	if (!difficultyDT) {
		return nullptr;
	}

	TArray<FACFDifficultyScaling*> allRows;
	difficultyDT->GetAllRows(TEXT("GetCachedDifficultyScaling"), allRows);

	for (FACFDifficultyScaling* row : allRows) {
		if (row && row->DifficultyLevel == difficultyLevel) {
			return row;
		}
	}

	return nullptr;
}

float UACFGASStatisticsComponent::GetMultiplierForAttribute(const FACFDifficultyScaling& scaling, const FGameplayAttribute& attribute)
{
	for (const FACFDifficultyAttributeMultiplier& entry : scaling.AttributeMultipliers) {
		if (entry.Attribute == attribute) {
			return entry.Multiplier;
		}
	}
	return 1.0f;
}

void UACFGASStatisticsComponent::OnDifficultyLevelChanged(FGameplayTag NewDifficultyLevel)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}

	if (bAffectedByDifficultyLevel) {
		RefreshDifficulty();
	} else if (!bLoaded) {
		InitAttributesValue();
	}
}

FActiveGameplayEffectHandle UACFGASStatisticsComponent::AddAttributeSetModifier(const FAttributesSetModifier& attModifier)
{
	FAttributesSetModifier convertedMod = attModifier;
	if (attModifier.GEModifierType == EGEType::ESetByCallerFromConfig) {
		if (attModifier.StatisticsMod.Num() == 0 && attModifier.PrimaryAttributesMod.Num() == 0 && attModifier.AttributesMod.Num() == 0) {
			return FActiveGameplayEffectHandle();
		}
		convertedMod.GEHandle = UARSFunctionLibrary::CreateAndApplyGameplayEffectFromAttributeModifier(attModifier, GetOwnerAbilityComponent());
	} else {
		convertedMod.GEHandle = UACFRPGFunctionLibrary::AddGameplayEffectToActor(FGameplayEffectConfig(attModifier.GameplayEffectModifier, attModifier.GELevel), GetOwner());
	}

	activeModifiers.AddUnique(convertedMod);
	return convertedMod.GEHandle;
}

void UACFGASStatisticsComponent::RemoveAttributeSetModifier(const FActiveGameplayEffectHandle& modifier)
{
	UACFRPGFunctionLibrary::RemovesActiveGameplayEffectFromActor(modifier, GetOwner());
	activeModifiers.RemoveAll([&modifier](const FAttributesSetModifier& activeModifier)
		{
			return activeModifier.GEHandle == modifier;
		});
}

bool UACFGASStatisticsComponent::CheckCosts(const TArray<FStatisticValue>& Costs) const
{
	if (GetOwnerAbilityComponent()) {
		return UARSFunctionLibrary::VerifyCosts(Costs, GetOwnerAbilityComponent());
	}
	return true;
}

bool UACFGASStatisticsComponent::CheckCost(const FStatisticValue& Cost) const
{
	TArray<FStatisticValue> costs;
	costs.Add(Cost);
	return CheckCosts(costs);
}

void UACFGASStatisticsComponent::ConsumeStatistics(const TArray<FStatisticValue>& Costs)
{
	UARSFunctionLibrary::CreateAndApplyGameplayEffectFromStatisticCost(StatModifierGE, Costs, GetOwnerAbilityComponent());
	OnAttributeSetModified.Broadcast();
}

void UACFGASStatisticsComponent::ModifyStatistic(FGameplayTag Stat, float value)
{
	const TArray<FStatisticValue> costs = { FStatisticValue(Stat, -value) };
	ConsumeStatistics(costs);
}

void UACFGASStatisticsComponent::ModifyStat_Implementation(const FStatisticValue& StatMod)
{
	const TArray<FStatisticValue> costs = { FStatisticValue(StatMod.Statistic, -StatMod.Value) };
	ConsumeStatistics(costs);
}

void UACFGASStatisticsComponent::AddTimedAttributeSetModifier(const FAttributesSetModifier& attModifier, float duration)
{
	if (attModifier.GEModifierType == EGEType::ESetByCallerFromConfig) {
		if (attModifier.StatisticsMod.Num() == 0 && attModifier.PrimaryAttributesMod.Num() == 0 && attModifier.AttributesMod.Num() == 0) {
			return;
		}
		UARSFunctionLibrary::CreateAndApplyGameplayEffectFromAttributeModifier(attModifier, GetOwnerAbilityComponent(), attModifier.GELevel, duration);
	} else {
		UACFRPGFunctionLibrary::AddGameplayEffectToActor(FGameplayEffectConfig(attModifier.GameplayEffectModifier, attModifier.GELevel), GetOwner());
	}
}

float UACFGASStatisticsComponent::GetCurrentValueForStatitstic(FGameplayTag stat) const
{
	FStatisticsConfig statConfig;
	if (UARSFunctionLibrary::TryGetStatisticFromSetByCallerTag(stat, statConfig) && GetOwnerAbilityComponent()) {
		bool bFound = false;
		return GetOwnerAbilityComponent()->GetGameplayAttributeValue(statConfig.CurrentStatAttribute, bFound);
	}
	return 0.f;
}

float UACFGASStatisticsComponent::GetMaxValueForStatitstic(FGameplayTag stat) const
{
	FStatisticsConfig statConfig;
	if (UARSFunctionLibrary::TryGetStatisticFromSetByCallerTag(stat, statConfig) && GetOwnerAbilityComponent()) {
		bool bFound = false;
		return GetOwnerAbilityComponent()->GetGameplayAttributeValue(statConfig.MaxStatAttribute, bFound);
	}
	return -1.f;
}

float UACFGASStatisticsComponent::GetNormalizedValueForStatitstic(FGameplayTag statTag) const
{
	const float max = GetMaxValueForStatitstic(statTag);
	const float value = GetCurrentValueForStatitstic(statTag);
	return max != 0.f ? value / max : 0.f;
}

float UACFGASStatisticsComponent::GetCurrentPrimaryAttributeValue(FGameplayTag attributeTag) const
{
	return GetAttributeValue(attributeTag);
}

float UACFGASStatisticsComponent::GetCurrentAttributeValue(FGameplayTag attributeTag) const
{
	return GetAttributeValue(attributeTag);
}

float UACFGASStatisticsComponent::GetAttributeValue(FGameplayTag attributeTag) const
{
	FGameplayAttribute attribute;
	if (UARSFunctionLibrary::TryGetAttributeFromSetByCallerTag(attributeTag, attribute) && GetOwnerAbilityComponent()) {
		bool bFound = false;
		return GetOwnerAbilityComponent()->GetGameplayAttributeValue(attribute, bFound);
	}
	return -1.f;
}

bool UACFGASStatisticsComponent::HasValidStatistic(FGameplayTag stat) const
{
	FStatisticsConfig statConfig;
	return UARSFunctionLibrary::TryGetStatisticFromSetByCallerTag(stat, statConfig)
		&& statConfig.CurrentStatAttribute.IsValid()
		&& GetOwnerAbilityComponent() != nullptr;
}

bool UACFGASStatisticsComponent::HasValidAttribute(FGameplayTag param) const
{
	FGameplayAttribute attribute;
	return UARSFunctionLibrary::IsValidParameterTag(param)
		&& UARSFunctionLibrary::TryGetAttributeFromSetByCallerTag(param, attribute)
		&& attribute.IsValid()
		&& GetOwnerAbilityComponent() != nullptr;
}

bool UACFGASStatisticsComponent::HasValidPrimaryAttribute(FGameplayTag att) const
{
	FGameplayAttribute attribute;
	return UARSFunctionLibrary::IsValidAttributeTag(att)
		&& UARSFunctionLibrary::TryGetAttributeFromSetByCallerTag(att, attribute)
		&& attribute.IsValid()
		&& GetOwnerAbilityComponent() != nullptr;
}

void UACFGASStatisticsComponent::RefillStat(FGameplayTag Stat)
{
	if (UARSFunctionLibrary::IsValidStatisticTag(Stat)) {
		ModifyStatistic(Stat, GetMaxValueForStatitstic(Stat));
	}
}

void UACFGASStatisticsComponent::SetNewLevelAndReinitialize_Implementation(int32 newLevel)
{
	ForceSetLevel(newLevel);
	InitializeAttributeSet();
}

void UACFGASStatisticsComponent::BindHealthDelegate()
{
	if (!abilityComp) {
		abilityComp = GetOwner() ? GetOwner()->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
	}

	if (!abilityComp) {
		return;
	}

	const FGameplayAttribute healthAttribute = UARSFunctionLibrary::GetDefaultHealthAttribute();
	abilityComp->GetGameplayAttributeValueChangeDelegate(healthAttribute).AddUObject(this, &UACFGASStatisticsComponent::HandleHealthReachesZero);
}

void UACFGASStatisticsComponent::HandleHealthReachesZero(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue == 0.f) {
		OnDeath();
		OnHealthReachesZero.Broadcast();
	}
}

void UACFGASStatisticsComponent::OnDeath_Implementation()
{
}

void UACFGASStatisticsComponent::OnRevive_Implementation()
{
}
