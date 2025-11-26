// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include <GameplayTagContainer.h>
#include "ACFGASTypes.h"
#include "Components/ACFInventoryComponent.h"
#include "ACFAbilitySet.h"
#include "ACFCombatBehaviorDataAsset.h"

#include "ACFCharacterDataAsset.generated.h"

struct FStartingItem;


/**
 *
 */
UCLASS()
class AIFRAMEWORK_API UACFCharacterDataAsset : public UPrimaryDataAsset {
	GENERATED_BODY()

public:

	UACFCharacterDataAsset();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FText ChatacterName = FText::FromString("AI Character");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	UTexture2D* CharacterPortrait;

	// All mesh components data (main mesh, helmets, armor pieces, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Character Meshes",
		meta = (TitleProperty = "ComponentTag"))
	TArray<FSkeletalMeshComponentData> MeshComponents;


#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

private:
	// Update material array based on the selected skeletal mesh
	void UpdateMaterialArray(FSkeletalMeshComponentData& MeshData);
#endif

public:
	/*The Team this character belongs to*/
	UPROPERTY(EditAnywhere, meta = (Categories = "Teams"), BlueprintReadWrite, Category = "ACF")
	FGameplayTag Team;

	/*Indicates if this character can gain exp and level up and how the stats increase is handles:
	EGenerateNewStatsFromCurves  After leveling up, new stats are generated from AttributesByLevelCurves
	EAssignPerksManually	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ACF|Leveling System")
	ELevelingType LevelingType = ELevelingType::ECantLevelUp;

	/**
	 * Handle to the row in the attribute initialization DataTable.
	 * This defines the base attribute values for this character.
	 * RowType is enforced to be ACFAttributeInits.
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

	/*For Each level the amount of Exp necessary to Lvl Up*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "LevelingType != ELevelingType::ECantLevelUp"), Category = "ACF|Leveling System")
	class UCurveFloat* ExpForNextLevelCurve;

	/*Exp to be given when this character dies it has not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "LevelingType == ELevelingType::ECantLevelUp"), Category = "ACF|Leveling System")
	float ExpToGiveOnDeath;

	/*For Each level the amount of Exp given to the killer. Used Only if this character can level up*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "LevelingType != ELevelingType::ECantLevelUp"), Category = "ACF|Leveling System")
	class UCurveFloat* ExpToGiveOnDeathByCurrentLevel;

	/* The character's starting items*/
	UPROPERTY(EditAnywhere, meta = (TitleProperty = "ItemClass"), BlueprintReadWrite, Category = "ACF|Equipment")
	TArray<FStartingItem> StartingItems;

	/*The Actions set to be used for this character*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ACF|Abilities")
	UACFAbilitySet* DefaultAbilitySet;

	/* The abilityset to be used when a specific moveset is applied*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "ACF|Abilities")
	TMap<FGameplayTag, UACFAbilitySet*> MovesetAbilities;

	   /** All fragments contained in this instance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "ACF")
    TArray<UACFCharacterFragment*> Fragments;

    /**
     * Return the first fragment matching the class (or subclass) passed.
     * Fully Blueprintable and extensible.
     */
    UFUNCTION(BlueprintCallable, Category = "Fragments", meta = (DeterminesOutputType = "FragmentClass"))
    UACFCharacterFragment* GetFragmentByClass(TSubclassOf<UACFCharacterFragment> FragmentClass) const;

	/* Configuration of Combat abilities for this AI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|AI")
	UACFCombatBehaviorDataAsset* CombatBehaviour;
};
