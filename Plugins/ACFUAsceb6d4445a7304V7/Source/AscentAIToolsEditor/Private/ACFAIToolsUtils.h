// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFAIToolsTypes.h"
#include "CoreMinimal.h"

namespace ACFAIToolsUtils
{
	/** Accepts "/Game/Dir/Name" or "/Game/Dir/Name.Name"; returns the package name and the object path. */
	bool SplitAssetPath(const FString& InPath, FString& OutPackage, FString& OutObjectPath, FString& OutError);

	/** Loads an asset if its package exists, without warnings when it does not. */
	UObject* FindExistingAsset(const FString& Package, const FString& ObjectPath);

	/** Native and Blueprint subclasses of Base, sorted by name. Blueprints are found unloaded through the registry. */
	TArray<FACFToolClassInfo> CollectClasses(UClass* Base);

	/** As above, keeping only classes whose native class (a Blueprint's native parent) passes NativeFilter. */
	TArray<FACFToolClassInfo> CollectClasses(UClass* Base, TFunctionRef<bool(const UClass* NativeClass)> NativeFilter);

	/** Resolves a class by name or path among Base and its subclasses; on failure lists what is installed. */
	UClass* ResolveClass(UClass* Base, const FString& NameOrPath, FString& OutError);

	/**
	 * Writable properties of Class, with struct members flattened into "Struct.Member" paths.
	 * bIncludeInline also lists inline object slots and the members of struct arrays that hold them, as "Array[i].Member".
	 */
	TArray<FACFToolPropertyInfo> DescribeProperties(const UClass* Class, const TSet<FName>& Skip, bool bIncludeInline = false);

	/**
	 * Writes each path -> value pair onto Target; failures are appended to OutErrors.
	 * Paths may index arrays ("Items[2].Count", growing the array) and go through existing inline objects.
	 */
	void ApplyProperties(UObject* Target, const TMap<FString, FString>& Properties, const TSet<FName>& Skip,
		const FString& ErrorPrefix, TArray<FString>& OutApplied, TArray<FString>& OutErrors);

	/** Empties the arrays, maps and sets at Paths; other properties are reset to zero/none. */
	void ClearProperties(UObject* Target, const TArray<FString>& Paths, const TSet<FName>& Skip,
		TArray<FString>& OutApplied, TArray<FString>& OutErrors);

	/** Creates (or, with an empty Class, edits) the inline object at each spec's path, in order. */
	void ApplyInlineObjects(UObject* Target, const TArray<FACFToolInlineObjectSpec>& Specs, const TSet<FName>& Skip,
		TArray<FString>& OutApplied, TArray<FString>& OutErrors);

	/**
	 * Paths of Source that differ from Defaults, in the format ApplyProperties accepts.
	 * With OutInlineObjects, inline objects are exported as path-addressed specs and the containers that must be
	 * emptied before rebuilding them are added to OutCleared; without it they are skipped.
	 */
	TMap<FString, FString> ExportChangedProperties(const UObject* Source, const UObject* Defaults, const TSet<FName>& Skip,
		TArray<FACFToolInlineObjectSpec>* OutInlineObjects = nullptr, TArray<FString>* OutCleared = nullptr);

	/** Creates the inline objects described by Specs as subobjects of Outer. */
	bool BuildInstancedObjects(UObject* Outer, UClass* Base, const TArray<FACFToolObjectSpec>& Specs,
		const FString& ErrorPrefix, TArray<UObject*>& OutObjects, TArray<FString>& OutApplied, TArray<FString>& OutErrors);

	FACFToolObjectSpec ExportInstancedObject(const UObject* Object);

	/** Replaces the contents of Owner's object array property. */
	bool SetObjectArray(UObject* Owner, FName PropertyName, const TArray<UObject*>& Objects, FString& OutError);

	/** Turns a list of errors into the report and, when bRaise, a script error for the caller. */
	FACFToolReport Finish(FACFToolReport Report, bool bRaise);

	/** Dry-runs Run(false) and raises its errors; otherwise runs Run(true), keeping the dry run's warnings. */
	FACFToolReport SaveChecked(TFunctionRef<FACFToolReport(bool bSave)> Run);

	/** Creates or updates a Blueprint asset whose class derives from RequiredBase. */
	struct FBlueprintRequest
	{
		FString AssetPath;
		FString ParentClass;
		UClass* RequiredBase = nullptr;
		bool bOverwriteExisting = false;
		const TMap<FString, FString>* Properties = nullptr;

		/** Optional instanced object array on the class defaults, e.g. an item's Fragments. */
		FName InstancedArrayProperty;
		UClass* InstancedBase = nullptr;
		const TArray<FACFToolObjectSpec>* InstancedSpecs = nullptr;
		bool bClearInstanced = false;
	};

	/** bSave false validates against a throwaway object; true writes, compiles and saves the Blueprint. */
	FACFToolReport RunBlueprintRequest(const FBlueprintRequest& Request, bool bSave, TFunctionRef<void(UObject* Defaults, FACFToolReport& Report)> ExtraChecks);

	/** Spec-shaped view of an existing Blueprint: parent class, changed defaults and instanced objects. */
	bool ExportBlueprint(const FString& AssetPath, UClass* RequiredBase, FName InstancedArrayProperty,
		FString& OutObjectPath, FString& OutParentClass, TMap<FString, FString>& OutProperties,
		TArray<FACFToolObjectSpec>& OutInstanced, FString& OutError);

	/**
	 * Creates or updates a DataAsset of Class. Apply writes the spec onto the target, which is a throwaway
	 * copy when bSave is false.
	 */
	FACFToolReport RunDataAssetRequest(const FString& AssetPath, UClass* Class, bool bOverwriteExisting, bool bSave,
		TFunctionRef<void(UObject* Target, FACFToolReport& Report)> Apply);
}
