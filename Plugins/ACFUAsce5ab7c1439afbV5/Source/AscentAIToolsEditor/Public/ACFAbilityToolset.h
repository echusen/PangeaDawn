// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFAIToolsTypes.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ACFAbilityToolset.generated.h"

/** A gameplay ability Blueprint, usually deriving from ACFActionAbility. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFAbilitySpec
{
	GENERATED_BODY()

	/** Blueprint asset path, e.g. "/Game/MyProject/Abilities/GA_HeavySlash". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString AssetPath;

	/** Parent class from ListAbilityClasses, e.g. "ACFActionAbility". Required to create; must match to update. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString ParentClass;

	/** Must be true to update an ability that already exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bOverwriteExisting = false;

	/**
	 * Property path -> value from DescribeAbilityClass, e.g. "animMontage" -> "/Game/Anims/AM_Slash.AM_Slash",
	 * "ActionConfig.CoolDownTime" -> "2". Only the listed properties change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TMap<FString, FString> Properties;
};

/** An entry of the ability set's Abilities list: a gameplay ability class granted with a trigger tag. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFAbilityGrantSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString TriggeringTag;

	/** Class name or path from ListAbilityClasses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	int32 AbilityLevel = 1;
};

/** An entry of the ability set's ACF Abilities list: an action configured inline in the set. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFActionAbilitySpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString TriggeringTag;

	/**
	 * Class name or path from ListActionClasses. Leave empty to edit the action already registered under
	 * TriggeringTag in place.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString ActionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	int32 AbilityLevel = 1;

	/** Property path -> value from DescribeAbilityClass for the action class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TMap<FString, FString> Properties;

	/** Nested instanced objects at paths relative to the action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFToolInlineObjectSpec> InlineObjects;

	/** Paths to clear before rebuilding nested objects and applying properties. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> ClearedProperties;
};

/**
 * An ACF Ability Set Data Asset. Tagged entries are matched by TriggeringTag; untagged ability grants
 * are matched by class and occurrence. Other entries are kept unless their tag is in RemoveTags.
 */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFAbilitySetSpec
{
	GENERATED_BODY()

	/** DataAsset path, e.g. "/Game/MyProject/Abilities/DA_AbilitySet_Bandit". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString AssetPath;

	/** Must be true to update an ability set that already exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bOverwriteExisting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFAbilityGrantSpec> Abilities;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFActionAbilitySpec> ActionAbilities;

	/** Trigger tags whose entries are removed from both lists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> RemoveTags;
};

/**
 * Create and edit ACF abilities. An ability is either a gameplay ability Blueprint (SaveAbility) or an action
 * configured inline in an Ability Set (SaveAbilitySet, ActionAbilities). Ability Sets are Data Assets that grant
 * both kinds to a character.
 */
UCLASS(BlueprintType, Hidden)
class ASCENTAITOOLSEDITOR_API UACFAbilityToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Gameplay ability classes usable as ParentClass or in FACFAbilitySetSpec.Abilities. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolClassInfo> ListAbilityClasses();

	/** ACF action classes usable inline in FACFAbilitySetSpec.ActionAbilities. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolClassInfo> ListActionClasses();

	/**
	 * Writable properties of an ability or action class, with their paths, types, enum values and defaults.
	 * @param Class Class name or path from ListAbilityClasses or ListActionClasses.
	 */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static TArray<FACFToolPropertyInfo> DescribeAbilityClass(const FString& Class);

	/**
	 * Reads an ability Blueprint back as a spec that SaveAbility accepts.
	 * @param AbilityPath Blueprint asset path.
	 */
	UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
	static FACFAbilitySpec ExportAbility(const FString& AbilityPath);

	/** Checks an ability spec without writing anything. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport ValidateAbility(const FACFAbilitySpec& Spec);

	/** Creates or updates the ability Blueprint, compiles and saves it. Fails without changes if the spec is invalid. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport SaveAbility(const FACFAbilitySpec& Spec);

	/**
	 * Reads an Ability Set back as a spec that SaveAbilitySet accepts.
	 * @param AbilitySetPath DataAsset path.
	 */
	UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
	static FACFAbilitySetSpec ExportAbilitySet(const FString& AbilitySetPath);

	/** Checks an ability set spec without writing anything. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport ValidateAbilitySet(const FACFAbilitySetSpec& Spec);

	/** Creates or updates the Ability Set and saves it. Fails without changes if the spec is invalid. */
	UFUNCTION(meta = (AICallable), Category = "ACF")
	static FACFToolReport SaveAbilitySet(const FACFAbilitySetSpec& Spec);
};
