// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFAIToolsTypes.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ACFCharacterToolset.generated.h"

/** One item the character spawns with. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFStartingItemSpec
{
	GENERATED_BODY()

	/** Item class name or path from UACFItemToolset.ListItemClasses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bAutoEquip = true;

	/** Chance in [0, 100] that the item drops when the character dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	float DropChancePercentage = 0.f;
};

/** An ACF Character Data Asset: stats, appearance, team, equipment, abilities and fragments (AI, combat behaviour...). */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFCharacterSpec
{
	GENERATED_BODY()

	/** DataAsset path, e.g. "/Game/MyProject/Characters/DA_Character_Bandit". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString AssetPath;

	/** Optional UACFCharacterDataAsset subclass for new assets; empty uses the base class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString DataAssetClass;

	/** Must be true to update a Data Asset that already exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bOverwriteExisting = false;

	/**
	 * Property path -> value from DescribeCharacterData, e.g. "ChatacterName" -> "Bandit", "Team" -> "Teams.Enemies",
	 * "DefaultAbilitySet" -> "/Game/Abilities/DA_AbilitySet_Bandit.DA_AbilitySet_Bandit". Only the listed properties change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TMap<FString, FString> Properties;

	/** When not empty, replaces the starting items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFStartingItemSpec> StartingItems;

	/** Set to remove every starting item when StartingItems is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bClearStartingItems = false;

	/** Character fragments (see ListCharacterFragmentClasses). When not empty they replace the current fragments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFToolObjectSpec> Fragments;

	/** Set to remove every fragment when Fragments is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bClearFragments = false;
};

/**
 * Create and edit ACF characters as Character Data Assets. Enemies, NPCs and variants are Data Assets applied to an
 * existing ACF character Blueprint; do not create a character Blueprint for them.
 * Typical flow: DescribeCharacterData -> ListCharacterFragmentClasses -> ValidateCharacter -> SaveCharacter.
 */
UCLASS(BlueprintType, Hidden)
class ASCENTAITOOLSEDITOR_API UACFCharacterToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Writable Character Data Asset properties, with their paths, types, enum values and defaults. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolPropertyInfo> DescribeCharacterData();

	/** Classes usable in FACFCharacterSpec.Fragments, such as AI controller, combat behaviour and AI state fragments. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolClassInfo> ListCharacterFragmentClasses();

	/**
	 * Writable properties of a character fragment class.
	 * @param Class Class name or path from ListCharacterFragmentClasses.
	 */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolPropertyInfo> DescribeCharacterFragment(const FString& Class);

	/**
	 * Reads a Character Data Asset back as a spec that SaveCharacter accepts, with only the values that differ from the defaults.
	 * @param CharacterPath DataAsset path.
	 */
	UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
	static FACFCharacterSpec ExportCharacter(const FString& CharacterPath);

	/** Checks a spec without writing anything and reports every error at once. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport ValidateCharacter(const FACFCharacterSpec& Spec);

	/** Creates or updates the Character Data Asset and saves it. Fails without changes if the spec is invalid. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport SaveCharacter(const FACFCharacterSpec& Spec);
};
