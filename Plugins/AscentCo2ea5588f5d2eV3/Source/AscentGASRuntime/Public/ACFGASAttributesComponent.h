// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFGASTypes.h"
#include "ARSLevelingComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "ACFGASAttributesComponent.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UDataTable;

/**
 * Component that manages the initialization and runtime handling of character attributes
 * using the Gameplay Ability System (GAS).
 * It supports applying starting effects, loading data from DataTables, and assigning perks.
 */
UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTGASRUNTIME_API UACFGASAttributesComponent : public UARSLevelingComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFGASAttributesComponent();

    /**
     * Uses a specified number of perks to increment the given attribute.
     * Will do nothing if the player doesn't have enough available perks.
     *
     * @param attribute The attribute to be increased.
     * @param numPerks The number of perks to assign to the attribute (default: 1).
     */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = ARS)
    void AssignPerkToAttribute(FGameplayAttribute attribute, int32 numPerks = 1);

    /**
     * Gets the DataTable row that defines this character's base attribute values.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FDataTableRowHandle GetCharacterRow() const { return CharacterRow; }

    /**
     * Sets the DataTable row handle used to initialize this character's attributes.
     * This is typically set from editor or during character creation.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetCharacterRow(FDataTableRowHandle val);

    /* this function should be called ONLY ON SERVER,*/
    UFUNCTION(BlueprintCallable, Category = ARS)
    virtual void InitializeAttributeSet();

    void InitAttributesFromLevelCurves();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    /**
     * Initializes the attributes for this character using the DataTable row.
     * Called on BeginPlay and/or when data is assigned dynamically.
     */
    virtual void InitAttributesValue();

    void InitAttributesFromDT();

    /**
     * Applies any permanent GameplayEffects (such as passive bonuses) to the character.
     */
    virtual void ApplyPermanentEffects();

    /*If this is set to true, InitializeAttributeSet is called automatically On BeginPlay serverside.
    If false you have to manually initialize this component when needed*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ACF|Init")
    bool bAutoInitialize = false;

    /**
     * GameplayEffects that should be applied permanently when the component is initialized.
     * Useful for passive stat boosts or initial modifiers.
     */
    UPROPERTY(EditAnywhere, Category = "ACF|Init")
    TArray<TSubclassOf<UGameplayEffect>> StartingEffects;

    /**
     * Handle to the row in the attribute initialization DataTable.
     * This defines the base attribute values for this character.
     * RowType is enforced to be ACFAttributeInits. 
     * NOTE: The Datatables are set in ACFAbilitySystemComp!
     */
    UPROPERTY(EditAnywhere, meta = (EditCondition = "LevelingType != ELevelingType::EGenerateNewStatsFromCurves", RowType = "/Script/AscentGASRuntime.ACFAttributeInits"), Category = "ACF|Leveling System")
    FDataTableRowHandle CharacterRow;

    /**
     * Handle to the row in the attribute initialization DataTable.
     * This defines the base attribute values for this character.
     * RowType is enforced to be ACFAttributeInits.
     */
    UPROPERTY(EditAnywhere, meta = (EditCondition = "LevelingType == ELevelingType::EGenerateNewStatsFromCurves"), Category = "ACF|Leveling System")
    UCurveTable* AttributesByLevelCurve;


    /**
     * Called when the component is loaded from a save or initialized at runtime.
     * Can be overridden in Blueprint to customize loading behavior.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ARS)
    void OnComponentLoaded();

    /**
     * Called when the component is saved (e.g. for persistence systems).
     * Can be overridden in Blueprint to customize save behavior.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ARS)
    void OnComponentSaved();

    UFUNCTION(BlueprintPure, Category = ACF)
    UAbilitySystemComponent* GetOwnerAbilityComponent() const;

private:
    void InitStats();

    UDataTable* GetAttributeToSave() const;
    bool bLoaded;

    TObjectPtr<UAbilitySystemComponent> abilityComp;

    UPROPERTY(Savegame)
    TArray<FAttributeSerializeNames> SaveableAttributes;

    TArray<FActiveGameplayEffectHandle> permanentEffects;
};
