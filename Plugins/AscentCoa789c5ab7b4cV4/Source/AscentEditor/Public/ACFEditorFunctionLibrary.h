// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFGASTypes.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include <Engine/Texture2D.h>

#include "ACFEditorFunctionLibrary.generated.h"

class UACFAbilitySet;
class UACFActionsSet;

/**
 *
 */
UCLASS()
class ASCENTEDITOR_API UACFEditorFunctionLibrary : public UBlueprintFunctionLibrary {
	GENERATED_BODY()

public:
	/** Retrieves all assets of a specific class from the content browser */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Editor")
	static UObject* GetObjectCDO(TSubclassOf<UObject> AssetClass);

	/** Retrieves all assets of a specific class from the content browser */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Editor")
	static UObject* GetAssetCDO(UObject* Asset);

	/** Retrieves all assets of a specific class from the content browser */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Editor")
	static bool SaveObjectPackage(UObject* Object);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Editor")
	static void AddAttributeInitToDataTable(UDataTable* DataTable, FName RowName, const FAttributeInit& RowData);

	static void AddRowToDataTable(UDataTable* DataTable, FName RowName, const FTableRowBase& RowData);



	/**
	 * Migrates all given ActionsSets into AbilitySets.
	 * Each resulting AbilitySet will be created in the folder /Game/Migrated/AbilitySets/
	 * and will include a 1:1 copy of all action data (including instanced UACFActionAbility).
	 *
	 * @param SourceSets The list of UACFActionsSet assets to migrate.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ACF|Migration")
	static bool ConvertActionsSetsToAbilitySets(UACFActionsSet* SourceSet, const FString& FolderPath = "/Game/Migrated");

	static UACFAbilitySet* CreateAbilitySetAsset(const FString& FolderPath, const FString& AssetName);


	/**
	 * List of files that will need to be copied completely
	 * This ensures all plugin settings are properly configured in the project.
	 *
	 * @param PluginName The name of the plugin (e.g., "AscentCombatFramework")
	 * @return True if the file was successfully copied, false otherwise
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Project Setup")
	static bool CopyPluginConfigToProject(const TArray<FString>& filesToCopy, const FString& PluginName = "AscentCombatFramework");

	/**
	 * Adds collision channel and profile settings from the plugin to the project's DefaultEngine.ini.
	 * This configures custom collision channels needed by ACF systems.
	 *
	 * @param PluginName The name of the plugin to read collision settings from
	 * @return True if collision settings were successfully added, false otherwise
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Project Setup")
	static bool AddCollisionSettingsToProject(const FString& PluginName = "AscentCombatFramework");

	/**
	 * Adds gameplay tag settings from the plugin to the project's DefaultGameplayTags.ini.
	 * This ensures all ACF gameplay tags are properly registered in the project.
	 *
	 * @param PluginName The name of the plugin to read tag settings from
	 * @return True if tag settings were successfully added, false otherwise
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Project Setup")
	static bool AddGameplayTagSettingsToProject(const FString& PluginName = "AscentCombatFramework");

	/**
	 * Adds default game user settings from the plugin to the project's DefaultGameUserSettings.ini.
	 * This configures default game options like graphics quality, controls, etc.
	 *
	 * @param PluginName The name of the plugin to read user settings from
	 * @return True if user settings were successfully added, false otherwise
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Project Setup")
	static bool AddGameUserSettingsToProject(const FString& PluginName = "AscentCombatFramework");

	/**
	 * Performs all ACF project setup steps in one call:
	 * - Copies DefaultPlugins.ini
	 * - Adds collision settings
	 * - Adds gameplay tag settings
	 * - Adds game user settings
	 *
	 * @param PluginName The name of the plugin to configure
	 * @return True if all setup steps completed successfully, false otherwise
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ACF|Project Setup")
	static bool SetupACFProjectConfiguration(const FString& PluginName = "AscentCombatFramework");

private:
	// Helper function to read an INI file from the plugin's Config folder
	static bool ReadPluginConfigFile(const FString& PluginName, const FString& ConfigFileName, TArray<FString>& OutLines);

	// Helper function to append or merge sections into a project config file
	static bool MergeConfigSection(const FString& ProjectConfigPath, const FString& SectionName, const TArray<FString>& SectionLines, bool bAppendIfExists = true);

	// Helper function to get the project's Config directory path
	static FString GetProjectConfigDirectory();

	// Helper function to get the plugin's Config directory path
	static FString GetPluginConfigDirectory(const FString& PluginName);
};
