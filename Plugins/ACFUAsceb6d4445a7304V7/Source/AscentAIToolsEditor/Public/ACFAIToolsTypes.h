// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ACFAIToolsTypes.generated.h"


/**
 * An inline object placed at a property path of the asset, e.g. "CombatStatesConfig[0].Conditions[1]" or
 * "DailyTasks[2].RoutineTask". Paths may go through earlier inline objects.
 */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFToolInlineObjectSpec
{
	GENERATED_BODY()

	/** Path of the inline object slot; array indices grow the array when needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Path;

	/** Class name or path from ListInlineObjectClasses. Empty edits the object already in the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Class;

	/** Property path -> value on the inline object itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TMap<FString, FString> Properties;

	/** Paths relative to this inline object, cleared after construction and before descendant objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> ClearedProperties;
};

/** An instanced object with property paths relative to that object. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFToolObjectSpec
{
	GENERATED_BODY()

	/** Class name or path, as returned by the matching List*Classes tool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Class;

	/** Property path -> value, e.g. "ActionConfig.CoolDownTime" -> "1.5". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TMap<FString, FString> Properties;

	/** Inline object slots addressed relative to this object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FACFToolInlineObjectSpec> InlineObjects;

	/** Property paths to clear before applying inline objects and scalar properties. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> ClearedProperties;
};

/** Result of a Validate* or Save* tool. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFToolReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bValid = false;

	/** Object path of the asset that was (or would be) written. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString AssetPath;

	/** True when the asset does not exist yet and will be created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	bool bCreatesAsset = false;

	/** Property paths that were (or would be) written. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> AppliedProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> Errors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	TArray<FString> Warnings;
};

/** A class that can be used as a parent, an inline object or a reference. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFToolClassInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Name;

	/** Native "/Script/Module.Class" or Blueprint "/Game/Path/BP_Name.BP_Name_C". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString ClassPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Description;
};

/** One writable property, addressed by its path. */
USTRUCT(BlueprintType)
struct ASCENTAITOOLSEDITOR_API FACFToolPropertyInfo
{
	GENERATED_BODY()

	/** Property path to use as a key in Properties, e.g. "ItemInfo.Name". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Name;

	/** C++ type, plus the allowed values for enums. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Type;

	/** Current default in the value format Properties expects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString DefaultValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
	FString Description;
};
