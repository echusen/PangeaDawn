// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ACFEditorFunctionLibrary.h"
#include "ACFAbilitySet.h"
#include "Actions/ACFActionsSet.h"
#include "Actions/ACFActionAbility.h"
#include "UObject/UObjectGlobals.h"
#include <AssetRegistry/AssetRegistryModule.h>
#include <EditorAssetLibrary.h>
#include <ImageUtils.h>
#include <ObjectTools.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>
#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>

static const FString DefaultMigrationFolder = TEXT("/Game/Migrated/AbilitySets");

UObject* UACFEditorFunctionLibrary::GetObjectCDO(TSubclassOf<UObject> AssetClass)
{
	if (!AssetClass) {
		UE_LOG(LogTemp, Warning, TEXT("Invalid class passed to GetObjectCDO"));
		return nullptr;
	}
	return AssetClass->GetDefaultObject();
}

UObject* UACFEditorFunctionLibrary::GetAssetCDO(UObject* Asset)
{
	if (!Asset) {
		UE_LOG(LogTemp, Warning, TEXT("Invalid class passed to GetObjectCDO"));
		return nullptr;
	}
	UBlueprint* BP = Cast<UBlueprint>(Asset);
	if (!BP) {
		UE_LOG(LogTemp, Warning, TEXT("Invalid class passed to GetObjectCDO"));
		return nullptr;
	}
	return BP->GeneratedClass->GetDefaultObject();
}

bool UACFEditorFunctionLibrary::SaveObjectPackage(UObject* Object)
{
	if (!Object) {
		UE_LOG(LogTemp, Warning, TEXT("Invalid object passed to SaveObjectPackage"));
		return false;
	}

	// Get the actual package, not just the outermost
	UPackage* Package = Object->GetPackage();
	if (!Package) {
		UE_LOG(LogTemp, Error, TEXT("Object has no valid package"));
		return false;
	}

	// Check if package is valid and not transient
	if (Package->HasAnyFlags(RF_Transient) || Package->GetName().StartsWith(TEXT("/Temp/"))) {
		UE_LOG(LogTemp, Error, TEXT("Cannot save transient or temporary package: %s"), *Package->GetName());
		return false;
	}

	// Generate unique filename to avoid conflicts
	FString PackageName = Package->GetName();
	FString FilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	// Handle duplicate names by adding suffix
	FString UniqueFilePath = FilePath;
	int32 Counter = 1;
	while (FPaths::FileExists(UniqueFilePath)) {
		FString BaseName = FPaths::GetBaseFilename(FilePath);
		FString Directory = FPaths::GetPath(FilePath);
		FString Extension = FPaths::GetExtension(FilePath, true);

		UniqueFilePath = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%d%s"), *BaseName, Counter, *Extension));
		Counter++;

		// Safety check to prevent infinite loop
		if (Counter > 1000) {
			UE_LOG(LogTemp, Error, TEXT("Too many duplicate files, aborting save"));
			return false;
		}
	}

	// Check if file is read-only
	if (FPaths::FileExists(UniqueFilePath) && IFileManager::Get().IsReadOnly(*UniqueFilePath)) {
		UE_LOG(LogTemp, Error, TEXT("Cannot save package: file is read-only -> %s"), *UniqueFilePath);
		return false;
	}

	// Mark package as dirty
	Package->MarkPackageDirty();

	// Setup save parameters correctly
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError | SAVE_FromAutosave;
	SaveArgs.bForceByteSwapping = false;
	SaveArgs.bWarnOfLongFilename = true;

	// Ensure the directory exists
	FString Directory = FPaths::GetPath(UniqueFilePath);
	if (!IFileManager::Get().DirectoryExists(*Directory)) {
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	// Attempt to save
	bool bSaveResult = UPackage::SavePackage(Package, Object, *UniqueFilePath, SaveArgs);

	if (bSaveResult) {
		UE_LOG(LogTemp, Log, TEXT("Successfully saved package to: %s"), *UniqueFilePath);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Failed to save package to: %s"), *UniqueFilePath);
	}

	return bSaveResult;
}

void UACFEditorFunctionLibrary::AddAttributeInitToDataTable(UDataTable* DataTable, FName RowName, const FAttributeInit& RowData)
{
	AddRowToDataTable(DataTable, RowName, RowData);
}

void UACFEditorFunctionLibrary::AddRowToDataTable(UDataTable* DataTable, FName RowName, const FTableRowBase& RowData)
{

	if (!DataTable || RowName.IsNone()) {
		return;
	}

	if (!DataTable->GetRowMap().Contains(RowName)) {
		DataTable->AddRow(RowName, RowData);

		DataTable->MarkPackageDirty();
		DataTable->PostEditChange();
		DataTable->Modify();

		FAssetRegistryModule::AssetCreated(DataTable);
	}
}

UACFAbilitySet* UACFEditorFunctionLibrary::CreateAbilitySetAsset(const FString& AssetName, const FString& FolderPath)
{
	FString PackagePath = FolderPath / AssetName;
	PackagePath = PackagePath.Replace(TEXT("//"), TEXT("/"));

	if (!FPackageName::IsValidLongPackageName(PackagePath)) {
		UE_LOG(LogTemp, Error, TEXT("Invalid package path: %s"), *PackagePath);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) {
		UE_LOG(LogTemp, Error, TEXT("Failed to create package: %s"), *PackagePath);
		return nullptr;
	}

	UACFAbilitySet* NewAsset = NewObject<UACFAbilitySet>(Package, UACFAbilitySet::StaticClass(), *AssetName, RF_Public | RF_Standalone);
	if (!NewAsset) {
		UE_LOG(LogTemp, Error, TEXT("Failed to instantiate asset: %s"), *AssetName);
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->MarkPackageDirty();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs args;
	args.SaveFlags = EObjectFlags::RF_Public | RF_Standalone;
	const bool bSuccess = UPackage::SavePackage(Package, NewAsset, *PackageFilename, args);

	if (!bSuccess) {
		UE_LOG(LogTemp, Warning, TEXT("Asset was created but failed to save: %s"), *PackagePath);
	}

	return NewAsset;
}

bool UACFEditorFunctionLibrary::ConvertActionsSetsToAbilitySets(UACFActionsSet* SourceSet, const FString& FolderPath)
{

	if (!SourceSet) {
		UE_LOG(LogTemp, Warning, TEXT("Null ActionSet found, skipping."));
		return false;
	}

	const FString SourceName = SourceSet->GetName();
	FString NewAssetName = FString::Printf(TEXT("AS_%s"), *SourceName);
	NewAssetName = NewAssetName.Replace(TEXT("_Default_"), TEXT(""), ESearchCase::IgnoreCase);
	NewAssetName = NewAssetName.Replace(TEXT("_C"), TEXT(""), ESearchCase::IgnoreCase);
	NewAssetName = NewAssetName.Replace(TEXT("_AS"), TEXT(""), ESearchCase::IgnoreCase);
	UACFAbilitySet* NewSet = UACFEditorFunctionLibrary::CreateAbilitySetAsset(NewAssetName, FolderPath);

	if (!NewSet) {
		UE_LOG(LogTemp, Error, TEXT("Failed to create ability set for %s"), *SourceName);
		return false;
	}

	for (const FActionState& ActionState : SourceSet->GetActionsRef()) {
		if (!IsValid(ActionState.Action)) {
			continue;
		}

		FActionAbilityConfig NewConfig;
		NewConfig.TriggeringTag = ActionState.TagName;
		NewConfig.AbilityLevel = 1;

		UObject* DuplicatedObject = StaticDuplicateObject(ActionState.Action, NewSet);
		UACFActionAbility* DuplicatedAction = Cast<UACFActionAbility>(DuplicatedObject);
		NewConfig.Action = DuplicatedAction;
		NewConfig.Action->SetAnimMontage(ActionState.MontageAction);
		NewSet->ActionAbilities.Add(NewConfig);
	}

	NewSet->MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("Created AbilitySet %s with %d actions"), *NewAssetName, NewSet->ActionAbilities.Num());
	return UACFEditorFunctionLibrary::SaveObjectPackage(NewSet);
}

FString UACFEditorFunctionLibrary::GetProjectConfigDirectory()
{
	// Get the project's Config directory path
	return FPaths::ProjectConfigDir();
}

FString UACFEditorFunctionLibrary::GetPluginConfigDirectory(const FString& PluginName)
{
	// Get the plugin's Config directory path
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Plugin '%s' not found"), *PluginName);
		return FString();
	}

	return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"));
}

bool UACFEditorFunctionLibrary::ReadPluginConfigFile(const FString& PluginName, const FString& ConfigFileName, TArray<FString>& OutLines)
{
	FString PluginConfigDir = GetPluginConfigDirectory(PluginName);
	if (PluginConfigDir.IsEmpty())
	{
		return false;
	}

	FString ConfigFilePath = FPaths::Combine(PluginConfigDir, ConfigFileName);

	// Check if file exists
	if (!FPaths::FileExists(ConfigFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Config file not found: %s"), *ConfigFilePath);
		return false;
	}

	// Read the entire file
	if (!FFileHelper::LoadFileToStringArray(OutLines, *ConfigFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read config file: %s"), *ConfigFilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Successfully read config file: %s (%d lines)"), *ConfigFilePath, OutLines.Num());
	return true;
}

bool UACFEditorFunctionLibrary::MergeConfigSection(const FString& ProjectConfigPath, const FString& SectionName, const TArray<FString>& SectionLines, bool bAppendIfExists)
{
	// Read existing project config file
	TArray<FString> ExistingLines;
	bool bFileExists = FPaths::FileExists(ProjectConfigPath);

	if (bFileExists)
	{
		if (!FFileHelper::LoadFileToStringArray(ExistingLines, *ProjectConfigPath))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to read existing config file: %s"), *ProjectConfigPath);
			return false;
		}
	}

	// Find if section already exists
	int32 SectionStartIndex = INDEX_NONE;
	int32 SectionEndIndex = INDEX_NONE;
	FString SectionHeader = FString::Printf(TEXT("[%s]"), *SectionName);

	for (int32 i = 0; i < ExistingLines.Num(); i++)
	{
		FString Line = ExistingLines[i].TrimStartAndEnd();

		if (Line.Equals(SectionHeader, ESearchCase::IgnoreCase))
		{
			SectionStartIndex = i;
		}
		else if (SectionStartIndex != INDEX_NONE && Line.StartsWith(TEXT("[")))
		{
			// Found the next section
			SectionEndIndex = i;
			break;
		}
	}

	// If section doesn't exist or we want to append
	if (SectionStartIndex == INDEX_NONE || bAppendIfExists)
	{
		// Add section at the end if it doesn't exist
		if (SectionStartIndex == INDEX_NONE)
		{
			if (ExistingLines.Num() > 0)
			{
				ExistingLines.Add(TEXT("")); // Add blank line before new section
			}
			ExistingLines.Add(SectionHeader);
		}

		// Add all section lines
		for (const FString& Line : SectionLines)
		{
			ExistingLines.Add(Line);
		}
	}

	// Write the modified content back
	if (!FFileHelper::SaveStringArrayToFile(ExistingLines, *ProjectConfigPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write config file: %s"), *ProjectConfigPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Successfully merged section [%s] into: %s"), *SectionName, *ProjectConfigPath);
	return true;
}

bool UACFEditorFunctionLibrary::CopyPluginConfigToProject(const TArray<FString>& filesToCopy, const FString& PluginName)
{
	for (const FString& file : filesToCopy) {

		TArray<FString> PluginConfigLines;
		if (!ReadPluginConfigFile(PluginName, file, PluginConfigLines))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to read DefaultPlugins.ini from plugin '%s'"), *PluginName);
			return false;
		}

		// Get project config path
		const FString ProjectConfigPath = FPaths::Combine(GetProjectConfigDirectory(), file);

		// Simply overwrite the entire file
		if (!FFileHelper::SaveStringArrayToFile(PluginConfigLines, *ProjectConfigPath))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to write DefaultPlugins.ini to project Config folder"));
			return false;
		}

	}

	UE_LOG(LogTemp, Log, TEXT("Successfully copied files from plugin '%s' to project"), *PluginName);
	return true;
}

bool UACFEditorFunctionLibrary::AddCollisionSettingsToProject(const FString& PluginName)
{
	// Read collision settings from plugin's DefaultEngine.ini
	TArray<FString> PluginEngineConfig;
	if (!ReadPluginConfigFile(PluginName, TEXT("DefaultEngine.ini"), PluginEngineConfig))
	{
		UE_LOG(LogTemp, Warning, TEXT("No DefaultEngine.ini found in plugin '%s', skipping collision settings"), *PluginName);
		return false;
	}

	// Extract collision-related sections
	TArray<FString> CollisionProfileSettings;
	TArray<FString> CollisionChannelSettings;
	bool bInCollisionProfiles = false;
	bool bInCollisionChannels = false;

	for (const FString& Line : PluginEngineConfig)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();

		// Check for section headers
		if (TrimmedLine.Equals(TEXT("[/Script/Engine.CollisionProfile]"), ESearchCase::IgnoreCase))
		{
			bInCollisionProfiles = true;
			bInCollisionChannels = false;
			continue;
		}
		else if (TrimmedLine.Equals(TEXT("[/Script/Engine.PhysicsSettings]"), ESearchCase::IgnoreCase))
		{
			bInCollisionChannels = true;
			bInCollisionProfiles = false;
			continue;
		}
		else if (TrimmedLine.StartsWith(TEXT("[")))
		{
			// New section started
			bInCollisionProfiles = false;
			bInCollisionChannels = false;
			continue;
		}

		// Add lines to appropriate arrays
		if (bInCollisionProfiles)
		{
			CollisionProfileSettings.Add(Line);
		}
		else if (bInCollisionChannels)
		{
			CollisionChannelSettings.Add(Line);
		}
	}

	// Get project's DefaultEngine.ini path
	FString ProjectEnginePath = FPaths::Combine(GetProjectConfigDirectory(), TEXT("DefaultEngine.ini"));

	// Merge collision profile settings
	bool bSuccess = true;
	if (CollisionProfileSettings.Num() > 0)
	{
		bSuccess &= MergeConfigSection(ProjectEnginePath, TEXT("/Script/Engine.CollisionProfile"), CollisionProfileSettings, true);
	}

	// Merge collision channel settings
	if (CollisionChannelSettings.Num() > 0)
	{
		bSuccess &= MergeConfigSection(ProjectEnginePath, TEXT("/Script/Engine.PhysicsSettings"), CollisionChannelSettings, true);
	}

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully added collision settings to project from plugin '%s'"), *PluginName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Partially failed to add collision settings to project from plugin '%s'"), *PluginName);
	}

	return bSuccess;
}

bool UACFEditorFunctionLibrary::AddGameplayTagSettingsToProject(const FString& PluginName)
{
	// Read gameplay tag settings from plugin's DefaultGameplayTags.ini
	TArray<FString> PluginTagsConfig;
	if (!ReadPluginConfigFile(PluginName, TEXT("DefaultGameplayTags.ini"), PluginTagsConfig))
	{
		UE_LOG(LogTemp, Warning, TEXT("No DefaultGameplayTags.ini found in plugin '%s', skipping gameplay tag settings"), *PluginName);
		return false;
	}

	// Extract gameplay tag sections
	TMap<FString, TArray<FString>> TagSections;
	FString CurrentSection;

	for (const FString& Line : PluginTagsConfig)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();

		// Check if this is a section header
		if (TrimmedLine.StartsWith(TEXT("[")) && TrimmedLine.EndsWith(TEXT("]")))
		{
			// Extract section name (remove brackets)
			CurrentSection = TrimmedLine.Mid(1, TrimmedLine.Len() - 2);
			TagSections.Add(CurrentSection, TArray<FString>());
		}
		else if (!CurrentSection.IsEmpty())
		{
			// Add line to current section
			TagSections[CurrentSection].Add(Line);
		}
	}

	// Get project's DefaultGameplayTags.ini path
	FString ProjectTagsPath = FPaths::Combine(GetProjectConfigDirectory(), TEXT("DefaultGameplayTags.ini"));

	// Merge all tag sections
	bool bSuccess = true;
	for (const auto& Section : TagSections)
	{
		if (Section.Value.Num() > 0)
		{
			bSuccess &= MergeConfigSection(ProjectTagsPath, Section.Key, Section.Value, true);
		}
	}

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully added gameplay tag settings to project from plugin '%s'"), *PluginName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Partially failed to add gameplay tag settings to project from plugin '%s'"), *PluginName);
	}

	return bSuccess;
}

bool UACFEditorFunctionLibrary::AddGameUserSettingsToProject(const FString& PluginName)
{
	// Read game user settings from plugin's DefaultGameUserSettings.ini
	TArray<FString> PluginUserSettingsConfig;
	if (!ReadPluginConfigFile(PluginName, TEXT("DefaultGameUserSettings.ini"), PluginUserSettingsConfig))
	{
		UE_LOG(LogTemp, Warning, TEXT("No DefaultGameUserSettings.ini found in plugin '%s', skipping game user settings"), *PluginName);
		return false;
	}

	// Extract ACF-specific settings sections
	TMap<FString, TArray<FString>> UserSettingsSections;
	FString CurrentSection;

	for (const FString& Line : PluginUserSettingsConfig)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();

		// Check if this is a section header
		if (TrimmedLine.StartsWith(TEXT("[")) && TrimmedLine.EndsWith(TEXT("]")))
		{
			// Extract section name (remove brackets)
			CurrentSection = TrimmedLine.Mid(1, TrimmedLine.Len() - 2);
			UserSettingsSections.Add(CurrentSection, TArray<FString>());
		}
		else if (!CurrentSection.IsEmpty())
		{
			// Add line to current section
			UserSettingsSections[CurrentSection].Add(Line);
		}
	}

	// Get project's DefaultGameUserSettings.ini path
	FString ProjectUserSettingsPath = FPaths::Combine(GetProjectConfigDirectory(), TEXT("DefaultGameUserSettings.ini"));

	// Merge all user settings sections
	bool bSuccess = true;
	for (const auto& Section : UserSettingsSections)
	{
		if (Section.Value.Num() > 0)
		{
			bSuccess &= MergeConfigSection(ProjectUserSettingsPath, Section.Key, Section.Value, true);
		}
	}

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully added game user settings to project from plugin '%s'"), *PluginName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Partially failed to add game user settings to project from plugin '%s'"), *PluginName);
	}

	return bSuccess;
}

bool UACFEditorFunctionLibrary::SetupACFProjectConfiguration(const FString& PluginName)
{
	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("Starting ACF Project Configuration Setup"));
	UE_LOG(LogTemp, Log, TEXT("Plugin: %s"), *PluginName);
	UE_LOG(LogTemp, Log, TEXT("========================================"));

	bool bSuccess = true;
	int32 SuccessCount = 0;
	int32 TotalSteps = 4;

	// Step 1: Copy DefaultPlugins.ini
	UE_LOG(LogTemp, Log, TEXT("[1/%d] Copying DefaultPlugins.ini..."), TotalSteps);
	const TArray<FString> FilesToCopy = { TEXT("DefaultPlugins.ini"),  TEXT("DefaultGameplayTags.ini"),  TEXT("DefaultGameUserSettings.ini"), TEXT("DefaultInput.ini")};
	if (CopyPluginConfigToProject(FilesToCopy, PluginName))
	{
		SuccessCount++;
		UE_LOG(LogTemp, Log, TEXT("✓ DefaultPlugins.ini copied successfully"));
	}
	else
	{
		bSuccess = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Failed to copy DefaultPlugins.ini"));
	}

	// Step 2: Add collision settings
	UE_LOG(LogTemp, Log, TEXT("[2/%d] Adding collision settings..."), TotalSteps);
	if (AddCollisionSettingsToProject(PluginName))
	{
		SuccessCount++;
		UE_LOG(LogTemp, Log, TEXT("✓ Collision settings added successfully"));
	}
	else
	{
		bSuccess = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Failed to add collision settings"));
	}

	// Step 3: Add gameplay tag settings
	UE_LOG(LogTemp, Log, TEXT("[3/%d] Adding gameplay tag settings..."), TotalSteps);
	if (AddGameplayTagSettingsToProject(PluginName))
	{
		SuccessCount++;
		UE_LOG(LogTemp, Log, TEXT("✓ Gameplay tag settings added successfully"));
	}
	else
	{
		bSuccess = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Failed to add gameplay tag settings"));
	}

	// Step 4: Add game user settings
	UE_LOG(LogTemp, Log, TEXT("[4/%d] Adding game user settings..."), TotalSteps);
	if (AddGameUserSettingsToProject(PluginName))
	{
		SuccessCount++;
		UE_LOG(LogTemp, Log, TEXT("✓ Game user settings added successfully"));
	}
	else
	{
		bSuccess = false;
		UE_LOG(LogTemp, Error, TEXT("✗ Failed to add game user settings"));
	}

	// Summary
	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("ACF Project Configuration Complete"));
	UE_LOG(LogTemp, Log, TEXT("Success: %d/%d steps"), SuccessCount, TotalSteps);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Status: ✓ ALL CONFIGURATIONS APPLIED"));
		UE_LOG(LogTemp, Log, TEXT("NOTE: You may need to restart the editor for all changes to take effect."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Status: ⚠ PARTIAL CONFIGURATION"));
		UE_LOG(LogTemp, Warning, TEXT("Some configurations failed. Check the log above for details."));
	}

	UE_LOG(LogTemp, Log, TEXT("========================================"));

	return bSuccess;
}