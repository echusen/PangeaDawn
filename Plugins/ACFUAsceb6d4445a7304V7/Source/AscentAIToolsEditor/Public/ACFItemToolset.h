// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFAIToolsTypes.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ACFItemToolset.generated.h"

/** An ACF item Blueprint (weapon, armor, consumable, accessory, material, projectile...). */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFItemSpec
{
	GENERATED_BODY()

	/** Blueprint asset path, e.g. "/Game/MyProject/Items/Weapons/BP_Sword_Iron". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString AssetPath;

	/** Parent class from ListItemClasses, e.g. "ACFMeleeWeapon". Required to create; must match to update. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString ParentClass;

	/** Must be true to update an item that already exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bOverwriteExisting = false;

	/**
	 * Property path -> value from DescribeItemClass, e.g. "ItemInfo.Name" -> "Iron Sword",
	 * "ItemInfo.ItemSlots" -> "ItemSlot.MeleeWeapon.Sword", "WeaponMesh" -> "/Game/Meshes/SM_Sword.SM_Sword".
	 * Only the listed properties change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TMap<FString, FString> Properties;

	/** Item fragments (see ListItemFragmentClasses). When not empty they replace the current fragments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFToolObjectSpec> Fragments;

	/** Set to remove every fragment when Fragments is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bClearFragments = false;
};

/**
 * Create and edit ACF items. Items are Blueprints deriving from an ACF item class; their defaults hold the
 * item data. Typical flow: ListItemClasses -> DescribeItemClass -> ValidateItem -> SaveItem.
 */
UCLASS(BlueprintType, Hidden)
class ASCENTAITOOLSEDITOR_API UACFItemToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Item classes usable as ParentClass or as an item reference: native ACF item types and existing item Blueprints. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolClassInfo> ListItemClasses();

	/** Classes usable in FACFItemSpec.Fragments. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolClassInfo> ListItemFragmentClasses();

	/**
	 * Writable properties of an item or item fragment class, with their paths, types, enum values and defaults.
	 * @param Class Class name or path from ListItemClasses or ListItemFragmentClasses.
	 */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolPropertyInfo> DescribeItemClass(const FString& Class);

	/**
	 * Reads an item Blueprint back as a spec that SaveItem accepts, with only the values that differ from its parent.
	 * @param ItemPath Blueprint asset path.
	 */
	UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
	static FACFItemSpec ExportItem(const FString& ItemPath);

	/** Checks a spec without writing anything and reports every error at once. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport ValidateItem(const FACFItemSpec& Spec);

	/** Creates or updates the item Blueprint, compiles and saves it. Fails without changes if the spec is invalid. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport SaveItem(const FACFItemSpec& Spec);
};
