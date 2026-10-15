// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFGASTypes.h"
#include "ARSLevelingComponent.h"
#include "ARSTypes.h"
#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayEffectTypes.h"
#include <GameplayTagContainer.h>

#include "ACFGASStatisticsComponent.generated.h"

class UGameplayEffect;
class UDataTable;
class UACFDifficultyManagerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthReachesZero);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttributeSetModified);

/**
 * Unified GAS statistics and attributes component used by ACF characters.
 */
UCLASS(Blueprintable, ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ADVANCEDRPGSYSTEM_API UACFGASStatisticsComponent : public UARSLevelingComponent {
	GENERATED_BODY()

public:
	UACFGASStatisticsComponent();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = ARS)
	void AssignPerkToAttribute(FGameplayAttribute attribute, int32 numPerks = 1);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = ARS)
	void AssignPerkToPrimaryAttribute(FGameplayTag attributeTag, int32 numPerks = 1);

	UFUNCTION(BlueprintPure, Category = ACF)
	FDataTableRowHandle GetCharacterRow() const { return CharacterRow; }

	UFUNCTION(BlueprintPure, Category = ACF)
	UCurveTable* GetAttributesByLevelCurve() const { return AttributesByLevelCurve; }

	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetAttributesByLevelCurve(UCurveTable* InAttributesByLevelCurve) { AttributesByLevelCurve = InAttributesByLevelCurve; }

	UFUNCTION(BlueprintPure, Category = "ACF|Attributes")
	bool IsAffectedByDifficultyLevel() const { return bAffectedByDifficultyLevel; }

	UFUNCTION(BlueprintCallable, Category = "ACF|Attributes")
	void SetAffectedByDifficultyLevel(bool bInAffectedByDifficultyLevel) { bAffectedByDifficultyLevel = bInAffectedByDifficultyLevel; }

	UFUNCTION(BlueprintCallable, Category = "ACF|Attributes")
	void RefreshDifficulty();

	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetCharacterRow(FDataTableRowHandle val);

	UFUNCTION(BlueprintCallable, Category = ARS)
	void InitializeAttributeSet();

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = ACF)
	FActiveGameplayEffectHandle AddAttributeSetModifier(const FAttributesSetModifier& modifier);

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = ACF)
	void RemoveAttributeSetModifier(const FActiveGameplayEffectHandle& modifier);

	UFUNCTION(Server, Reliable, Category = ACF)
	void ModifyStat(const FStatisticValue& StatMod);

	UFUNCTION(BlueprintCallable, Category = ACF)
	void ModifyStatistic(FGameplayTag Stat, float value);

	UFUNCTION(BlueprintCallable, Category = ACF)
	bool CheckCosts(const TArray<FStatisticValue>& Costs) const;

	UFUNCTION(BlueprintCallable, Category = ACF)
	bool CheckCost(const FStatisticValue& Cost) const;

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = ACF)
	void AddTimedAttributeSetModifier(const FAttributesSetModifier& modifier, float duration);

	UFUNCTION(BlueprintCallable, Category = ARS)
	void ConsumeStatistics(const TArray<FStatisticValue>& Costs);

	UFUNCTION(BlueprintCallable, Category = ARS)
	void RefillStat(FGameplayTag Stat);

	UFUNCTION(BlueprintCallable, Category = ARS)
	float GetCurrentValueForStatitstic(FGameplayTag stat) const;

	UFUNCTION(BlueprintCallable, Category = ARS)
	float GetMaxValueForStatitstic(FGameplayTag stat) const;

	UFUNCTION(BlueprintCallable, Category = ARS)
	float GetNormalizedValueForStatitstic(FGameplayTag statTag) const;

	UFUNCTION(BlueprintCallable, Category = ARS)
	float GetCurrentPrimaryAttributeValue(FGameplayTag attributeTag) const;

	UFUNCTION(BlueprintCallable, Category = ARS)
	float GetCurrentAttributeValue(FGameplayTag attributeTag) const;

	UFUNCTION(BlueprintCallable, Category = ARS)
	bool HasValidStatistic(FGameplayTag stat) const;

	UFUNCTION(BlueprintCallable, Category = ARS)
	bool HasValidAttribute(FGameplayTag param) const;

	UFUNCTION(BlueprintCallable, Category = ARS)
	bool HasValidPrimaryAttribute(FGameplayTag att) const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = ARS)
	void SetNewLevelAndReinitialize(int32 newLevel);

	UFUNCTION(BlueprintNativeEvent, Category = ARS)
	void OnDeath();

	UFUNCTION(BlueprintNativeEvent, Category = ARS)
	void OnRevive();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ARS)
	void OnComponentLoaded();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ARS)
	void OnComponentSaved();

	UFUNCTION(BlueprintPure, Category = ACF)
	UAbilitySystemComponent* GetOwnerAbilityComponent() const;

	UPROPERTY(BlueprintAssignable, Category = ARS)
	FOnHealthReachesZero OnHealthReachesZero;

	UPROPERTY(BlueprintAssignable, Category = ARS)
	FOnAttributeSetModified OnAttributeSetModified;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ACF|Init")
	bool bAutoInitialize = false;

	UPROPERTY(EditAnywhere, Category = "ACF|Init")
	TArray<TSubclassOf<UGameplayEffect>> StartingEffects;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "LevelingType != ELevelingType::EGenerateNewStatsFromCurves", RowType = "/Script/AscentGASRuntime.ACFAttributeInits"), Category = "ACF|Leveling System")
	FDataTableRowHandle CharacterRow;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "LevelingType == ELevelingType::EGenerateNewStatsFromCurves"), Category = "ACF|Leveling System")
	UCurveTable* AttributesByLevelCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ACF|Set By Caller GEs")
	TSubclassOf<UGameplayEffect> StatModifierGE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ACF|Attributes")
	bool bAffectedByDifficultyLevel = false;

	UPROPERTY()
	bool bIsInitialized = false;

	UPROPERTY()
	TArray<FAttributesSetModifier> activeModifiers;

	void OnDeath_Implementation();
	void OnRevive_Implementation();
	void OnComponentLoaded_Implementation();
	void OnComponentSaved_Implementation();
	void AssignPerkToAttribute_Implementation(FGameplayAttribute attribute, int32 numPerks);
	void AssignPerkToPrimaryAttribute_Implementation(FGameplayTag attributeTag, int32 numPerks);
	void SetNewLevelAndReinitialize_Implementation(int32 newLevel);

private:
	void InitAttributesValue();
	void InitAttributesFromDT();
	void InitAttributesFromLevelCurves();
	void ApplyPermanentEffects();
	void BindHealthDelegate();
	void HandleHealthReachesZero(const FOnAttributeChangeData& ChangeData);
	float GetAttributeValue(FGameplayTag attributeTag) const;

	UDataTable* GetAttributeToSave() const;
	const FACFDifficultyScaling* GetCachedDifficultyScaling() const;
	static float GetMultiplierForAttribute(const FACFDifficultyScaling& Scaling, const FGameplayAttribute& Attribute);

	UFUNCTION()
	void OnDifficultyLevelChanged(FGameplayTag NewDifficultyLevel);

	bool bLoaded = false;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> abilityComp;

	UPROPERTY(Savegame)
	TArray<FAttributeSerializeNames> SaveableAttributes;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> permanentEffects;

	UPROPERTY()
	TObjectPtr<UDataTable> allAttributes;

	UPROPERTY()
	mutable TWeakObjectPtr<UACFDifficultyManagerComponent> CachedDifficultyManager;
};
