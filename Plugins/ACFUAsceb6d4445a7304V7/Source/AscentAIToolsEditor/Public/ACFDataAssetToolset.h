// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFAIToolsTypes.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ACFDataAssetToolset.generated.h"

/** Any ACF Data Asset without a dedicated tool: combat behaviours, AI routines, crafting and building recipes, gatherables... */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFDataAssetSpec
{
	GENERATED_BODY()

	/** DataAsset path, e.g. "/Game/MyProject/AI/DA_CombatBehavior_Archer". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString AssetPath;

	/** Class from ListDataAssetClasses. Required to create an asset; empty keeps the class of an existing one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString DataAssetClass;

	/** Must be true to update a Data Asset that already exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bOverwriteExisting = false;

	/** Property paths to empty before anything else is written, e.g. "CombatStatesConfig" to rebuild that array. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> ClearProperties;

	/**
	 * Property path -> value from DescribeClass. Array elements are addressed by index and the array grows to reach
	 * them, e.g. "RequiredItems[0].ItemClass" -> "/Game/Items/BP_IronOre.BP_IronOre_C", "RequiredItems[0].Count" -> "3".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TMap<FString, FString> Properties;

	/**
	 * Inline objects, applied in order after Properties, e.g. Path "CombatStatesConfig[0].Conditions[0]" with
	 * Class "ACFDistanceActionCondition", or Path "DailyTasks[0].RoutineTask" with an ACFTask subclass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFToolInlineObjectSpec> InlineObjects;
};

/**
 * Create and edit ACF Data Assets that have no dedicated toolset, by reflection.
 * Characters use ACFCharacterToolset and ability sets use ACFAbilityToolset.
 * Before creating, look for an existing asset to reuse. To copy an asset, ExportDataAsset it and save the spec under a new AssetPath.
 * Typical flow: ListDataAssetClasses -> DescribeClass -> ListInlineObjectClasses -> ValidateDataAsset -> SaveDataAsset.
 */
UCLASS(BlueprintType, Hidden)
class ASCENTAITOOLSEDITOR_API UACFDataAssetToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * ACF Data Asset classes, native and Blueprint, that SaveDataAsset can create.
	 * @param Filter Optional case-insensitive part of the class name, e.g. "Recipe". Pass an empty string to list all.
	 */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolClassInfo> ListDataAssetClasses(const FString& Filter);

	/**
	 * Concrete classes that can fill an inline object slot.
	 * @param BaseClass The "subclass of" class that DescribeClass reports for the slot, e.g. "ACFActionCondition".
	 */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolClassInfo> ListInlineObjectClasses(const FString& BaseClass);

	/**
	 * Writable properties of a Data Asset class or of an inline object class, with paths, types, enum values and
	 * defaults. Inline object slots are listed with the class they accept.
	 * @param Class Class name or path from ListDataAssetClasses or ListInlineObjectClasses.
	 */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolPropertyInfo> DescribeClass(const FString& Class);

	/**
	 * Reads a Data Asset back as a spec that SaveDataAsset accepts: changed values, inline objects, and the arrays
	 * to clear so that saving it rebuilds them exactly.
	 * @param AssetPath DataAsset path.
	 */
	UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
	static FACFDataAssetSpec ExportDataAsset(const FString& AssetPath);

	/** Checks a spec without writing anything and reports every error at once. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport ValidateDataAsset(const FACFDataAssetSpec& Spec);

	/** Creates or updates the Data Asset and saves it. Fails without changes if the spec is invalid. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport SaveDataAsset(const FACFDataAssetSpec& Spec);
};
