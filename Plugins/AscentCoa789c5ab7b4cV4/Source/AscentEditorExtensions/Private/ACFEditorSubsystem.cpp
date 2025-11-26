// Copyright (C) Developed by Pask & OlssonDev, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ACFEditorSubsystem.h"
#include "ACFAssetAction.h"
#include "ACFEditorSettings.h"
#include "ACFEditorTypes.h"
#include "AscentEditorExtensions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetTools/FAssetTypeActions_AssetCreatorBase.h"
#include "Factory/ACFActorFactory.h"
#include "Factory/ACFBaseFactory.h"
#include "Framework/Notifications/NotificationManager.h"
#include "KismetCompilerModule.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Widgets/Notifications/SNotificationList.h"
#include <AssetTypeActions/AssetTypeActions_DataAsset.h>
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/IPluginManager.h"
#include "ACFEditorStyle.h"

#define LOCTEXT_NAMESPACE "UAssetCreatorSubsystem"

void UACFEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Listen to when an asset has been deleted. So we can refresh the plugin in case the asset was a class of the plugin's API.
	FEditorDelegates::OnAssetsDeleted.AddUObject(this, &ThisClass::OnAssetsDeleted);

	// Listen to when an object has been modified. So we can refresh the categories.
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &ThisClass::OnAssetCreatorChanged);

	// We don't want to listen for new Assets before the editor is not opened.
	OnMapOpenedDelegate = FEditorDelegates::OnMapOpened.AddUObject(this, &ThisClass::OnEditorOpened);

	Super::Initialize(Collection);

	// Bind using AddLambda because OnAssetRenamed is a TS multicast delegate in UE5.6
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	/*
	const UACFEditorSettings* Settings = GetDefault<UACFEditorSettings>();
	if (Settings && Settings->AutoGenerateRedirectors()) {
		AssetRenamedHandle = AssetRegistryModule.Get().OnAssetRenamed().AddLambda(
			[this](const FAssetData& AssetData, const FString& OldObjectPath)
			{
				OnAssetRenamed(AssetData, OldObjectPath);
			}
		);
	}*/
}

void UACFEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
	const UACFEditorSettings* Settings = GetDefault<UACFEditorSettings>();
	/*
	if (Settings && Settings->AutoGenerateRedirectors()) {
		if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
			if (AssetRenamedHandle.IsValid())
			{
				AssetRegistryModule.Get().OnAssetRenamed().Remove(AssetRenamedHandle);
				AssetRenamedHandle.Reset();
			}
		}
	}*/
}

void UACFEditorSubsystem::OnEditorOpened(const FString& FileName, bool bAsTemplate)
{
	FEditorDelegates::OnMapOpened.Remove(OnMapOpenedDelegate);
	IAssetRegistry* AssetRegistry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();

	// Listen to when an asset has been added so we can keep those in memory if they're an UACFAssetAction or UACFBaseFactory.
	AssetRegistry->OnAssetAdded().AddUObject(this, &ThisClass::OnAssetCreated);

	// When we start the editor, just refresh the whole plugin.
	RefreshAll();
}

void UACFEditorSubsystem::OnAssetCreated(const FAssetData& AssetData)
{
	UClass* AssetAction = nullptr;
	if (GetClassFromBlueprint(AssetData, UACFAssetAction::StaticClass(), AssetAction)) {
		AssetActions.AddUnique(AssetAction);
		return;
	}

	UClass* Factory = nullptr;
	if (GetClassFromBlueprint(AssetData, UACFBaseFactory::StaticClass(), Factory)) {
		Factories.AddUnique(Factory);
		return;
	}
}

void UACFEditorSubsystem::OnAssetsDeleted(const TArray<UClass*>& AssetsToDelete)
{
	// Just refresh everything when an asset has been deleted.
	RefreshAll();
}

UACFAssetAction* UACFEditorSubsystem::GetAssetAction(UClass* AssetActionClass)
{
	if (AssetActionClass && AssetActionClass->IsChildOf(UACFAssetAction::StaticClass())) {
		UACFAssetAction* AssetAction = CastChecked<UACFAssetAction>(AssetActionClass->GetDefaultObject());
		return AssetAction->AssetActionSettings.IsValid() ? AssetAction : nullptr;
	}
	return nullptr;
}

bool UACFEditorSubsystem::GetClassFromBlueprint(const FAssetData AssetData, UClass* ClassToFind, UClass*& GeneratedClass)
{
	if (AssetData.IsValid() && ClassToFind) {
		FAssetDataTagMapSharedView::FFindTagResult Result = AssetData.TagsAndValues.FindTag(TEXT("NativeParentClass"));
		if (Result.IsSet()) {
			const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(Result.GetValue());
			if (UClass* ParentClass = FindObjectSafe<UClass>(nullptr, *ClassObjectPath, true)) {
				if (ParentClass->IsChildOf(ClassToFind)) {
					// TODO: Loading these assets could cause problems on projects with a large number of them.
					UBlueprint* BP = CastChecked<UBlueprint>(AssetData.GetAsset());
					// Can be not valid if a class has just recently been deleted.
					if (IsValid(BP->GeneratedClass)) {
						GeneratedClass = BP->GeneratedClass;
						return true;
					}
				}
			}
		}
	}

	return false;
}

void UACFEditorSubsystem::RefreshPlacementMode()
{
	if (GEditor) {
		IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

		for (auto RegisteredPlaceableItem : RegisteredPlaceableItems) {
			PlacementModeModule.UnregisterPlaceableItem(*RegisteredPlaceableItem);
		}
		RegisteredPlaceableItems.Empty();

		for (FPlacementCategoryInfo CategoryInfo : RegisteredPlaceableItemCategories) {
			PlacementModeModule.UnregisterPlacementCategory(CategoryInfo.UniqueHandle);
		}
		RegisteredPlaceableItemCategories.Empty();

		for (UActorFactory* ActorFactory : ActorFactories) {
			GEditor->ActorFactories.Remove(ActorFactory);
		}
		ActorFactories.Empty();

		const UACFEditorSettings* settings = GetEditorSettings();

		if (!settings) {
			return;
		}

		TArray<FPlaceCategoryConfig> placeCategory = settings->GetQuickPlacementActor();

		for (const auto category : placeCategory) {
			const FName CategoryName = category.CategoryName;
			FPlacementCategoryInfo PlacementCategoryInfo = FPlacementCategoryInfo(
				FText::FromName(CategoryName),
				CategoryName, "PM" + CategoryName.ToString(),
				25);

			FSlateIcon NewIcon(settings->GetStyleName(), category.IconName);

			PlacementCategoryInfo.DisplayIcon = NewIcon;
			PlacementModeModule.RegisterPlacementCategory(PlacementCategoryInfo);
			RegisteredPlaceableItemCategories.Add(PlacementCategoryInfo);
			for (auto placeable : category.Entries) {
				TSubclassOf<AActor> placeClass = placeable.AssetClass.Get();
				if (placeClass) {
					FPlaceableItem NewPlaceableItem;
					NewPlaceableItem.Factory = MakeActorFactoryFromClass(placeClass);
					NewPlaceableItem.AssetData = placeClass.Get();
					NewPlaceableItem.bAlwaysUseGenericThumbnail = true;

					//  GEditor->ActorFactories.Insert(NewPlaceableItem.Factory, 0);
					NewPlaceableItem.AssetTypeColorOverride = placeable.AssetColor;
					NewPlaceableItem.DisplayName = placeable.ClassNameOverride.IsEmpty() ? placeable.AssetClass->GetDisplayNameText() : placeable.ClassNameOverride;
					RegisteredPlaceableItems.Add(PlacementModeModule.RegisterPlaceableItem(PlacementCategoryInfo.UniqueHandle, MakeShared<FPlaceableItem>(NewPlaceableItem)));
				}
			}
		}

		PlacementModeModule.RegenerateItemsForCategory(FBuiltInPlacementCategories::AllClasses());
	}
}

void UACFEditorSubsystem::RefreshActions()
{
	RefreshPlacementMode();
	RefreshAssetTypeActions();
}

void UACFEditorSubsystem::RefreshAll()
{
	AssetActions = GatherAllAssetsOfClass(UACFAssetAction::StaticClass());
	Factories = GatherAllAssetsOfClass(UACFBaseFactory::StaticClass());

	RefreshActions();
}

void UACFEditorSubsystem::RefreshAssetTypeActions()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Unregister all of the AssetTypeActions first before we register new ones.
	for (int32 i = AssetTypeActions.Num(); i--;) {
		UnregisterAssetTypeAction(AssetTools, AssetTypeActions[i]);
	}

	AssetActionsInst.Empty();
	/*
   const UACFEditorSettings* settings = GetEditorSettings();

   const TArray<FMainCategoryConfig> contentConfig = settings->GetContentBrowserConfig();

   for (const auto mainCat : contentConfig) {
	   uint32 Category = AssetTools.RegisterAdvancedAssetCategory(mainCat.MainCategoryName,
		   FText::Format(LOCTEXT("AssetCreatorCategory", "{0}"),
			   FText::FromName(mainCat.MainCategoryName)));
	   for (const FAssetActionConfig& entry : mainCat.Entries) {
		   if (IsValid(entry.AssetClass.Get())) {

			   FAssetActionSettings newSettings(entry, mainCat.MainCategoryName, false, FText::FromString(""));
			   UACFAssetAction* newAction = MakeActorAction(newSettings);
			   UACFBaseFactory* factory = MakeFactory(newAction);

			   RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_AssetCreatorBase(newSettings, Category)));
		   }
	   }
	   for (const auto subCat : mainCat.SubCategories) {
		   for (const FAssetActionConfig& entry : subCat.Entries) {
			   if (IsValid(entry.AssetClass.Get())) {
				   FAssetActionSettings newSettings(entry, mainCat.MainCategoryName, true, FText::FromName(subCat.CategoryName));
				   UACFAssetAction* newAction = MakeActorAction(newSettings);
				   UACFBaseFactory* factory = MakeFactory(newAction);

				   RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_AssetCreatorBase(newSettings, Category)));
			   }
		   }
	   }
   }

   for (UClass* Class : AssetActions) {
	   if (UACFAssetAction* AssetAction = GetAssetAction(Class)) {
		   AssetActionsInst.AddUnique(AssetAction);
	   }
   }

   for (auto AssetAction : AssetActionsInst) {
	   if (AssetAction) {
		   uint32 Category = AssetTools.RegisterAdvancedAssetCategory(AssetAction->AssetActionSettings.CategoryName,
			   FText::Format(LOCTEXT("AssetCreatorCategory", "{0}"),
				   FText::FromName(AssetAction->AssetActionSettings.CategoryName)));

		   RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_AssetCreatorBase(AssetAction->AssetActionSettings, Category)));
	   }
   }
   */
   // Register all the valid AssetActions.
	for (UClass* Class : AssetActions) {
		if (UACFAssetAction* AssetAction = GetAssetAction(Class)) {
			uint32 Category = AssetTools.RegisterAdvancedAssetCategory(AssetAction->AssetActionSettings.CategoryName,
				FText::Format(LOCTEXT("AssetCreatorCategory", "{0}"),
					FText::FromName(AssetAction->AssetActionSettings.CategoryName)));

			if (AssetAction->AssetActionSettings.AssetClass->IsChildOf(UDataAsset::StaticClass())) {
				// Use data asset factory logic
				TSharedRef<FAssetTypeActions_ACFDataAsset> Action = MakeShareable(new FAssetTypeActions_ACFDataAsset(AssetAction->AssetActionSettings.AssetClass, AssetAction->AssetActionSettings, Category));
				RegisterAssetTypeAction(AssetTools, Action);
			}
			else {
				// Use blueprint factory logic
				RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_AssetCreatorBase(AssetAction->AssetActionSettings, Category)));
			}
		}
	}
}

void UACFEditorSubsystem::OnAssetCreatorChanged(UObject* Object, FPropertyChangedEvent& ChangedEvent)
{
	// Only refresh if the change had been made on one of our classes.
	UClass* Class = Object->GetClass();
	if (Class->IsChildOf(UACFAssetAction::StaticClass()) || Class->IsChildOf(UACFBaseFactory::StaticClass()) || Class == UACFEditorSettings::StaticClass()) {
		RefreshActions();
	}
}

void UACFEditorSubsystem::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}

void UACFEditorSubsystem::UnregisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.UnregisterAssetTypeActions(Action);
	AssetTypeActions.Remove(Action);
}

bool UACFEditorSubsystem::HasFactory(UClass* Class)
{
	if (Class) {
		UClass* BlueprintClass = nullptr;
		UClass* BlueprintGeneratedClass = nullptr;

		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetBlueprintTypesForClass(Class, BlueprintClass, BlueprintGeneratedClass);

		if (const UBlueprint* Blueprint = Cast<UBlueprint>(GetDefault<UObject>(BlueprintClass))) {
			if (!Blueprint->SupportedByDefaultBlueprintFactory()) {
				FNotificationInfo Info(NSLOCTEXT("AssetCreatorEditor", "FAscentEditorExtensions", "Selected class can't be used. The class doesn't support to be created in a default Blueprint Factory."));
				Info.ExpireDuration = 4.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
				return true;
			}
		}
	}

	return false;
}

UACFActorFactory* UACFEditorSubsystem::MakeActorFactory(UACFAssetAction* AssetAction)
{
	UACFActorFactory* NewFactory = nullptr;
	if (IsValid(AssetAction)) {
		NewFactory = NewObject<UACFActorFactory>(GetTransientPackage(), UACFActorFactory::StaticClass());
		NewFactory->AssetAction = AssetAction;
		NewFactory->NewActorClass = AssetAction->AssetActionSettings.AssetClass.Get();
		ActorFactories.Add(NewFactory);
	}
	return NewFactory;
}

UACFBaseFactory* UACFEditorSubsystem::MakeFactory(UACFAssetAction* AssetAction)
{
	UACFBaseFactory* NewFactory = nullptr;
	if (IsValid(AssetAction)) {
		NewFactory = NewObject<UACFBaseFactory>(GetTransientPackage(), UACFBaseFactory::StaticClass());
		NewFactory->ParentClass = AssetAction->AssetActionSettings.AssetClass;
		NewFactory->UseClassPicker = true;
		NewFactory->DefaultAssetName = "NewAsset";
		BaseFactories.Add(NewFactory);
	}
	return NewFactory;
}

UACFAssetAction* UACFEditorSubsystem::MakeActorAction(const FAssetActionSettings& assetSetting)
{
	UACFAssetAction* NewAction = nullptr;
	if (IsValid(assetSetting.AssetClass.Get())) {
		NewAction = NewObject<UACFAssetAction>(GetTransientPackage(), UACFAssetAction::StaticClass());
		NewAction->AssetActionSettings = assetSetting;
		AssetActionsInst.Add(NewAction);
	}
	return NewAction;
}

UACFActorFactory* UACFEditorSubsystem::MakeActorFactoryFromClass(TSubclassOf<AActor> AssetClass)
{
	UACFActorFactory* NewFactory = nullptr;
	if (IsValid(AssetClass)) {
		NewFactory = NewObject<UACFActorFactory>(GetTransientPackage(), UACFActorFactory::StaticClass());
		/*  NewFactory->AssetAction = AssetAction;*/
		NewFactory->NewActorClass = AssetClass.Get();
		ActorFactories.Add(NewFactory);
	}
	return NewFactory;
}

UACFEditorSettings* UACFEditorSubsystem::GetEditorSettings() const
{
	return GetMutableDefault<UACFEditorSettings>();
}



TArray<UClass*> UACFEditorSubsystem::GatherAllAssetsOfClass(UClass* Class)
{
	TArray<UClass*> Classes;

	// Search native classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt) {
		if (!ClassIt->IsNative() || !ClassIt->IsChildOf(Class)) {
			continue;
		}

		// Ignore classes that are Abstract, HideDropdown, and Deprecated.
		if (ClassIt->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown | CLASS_Deprecated | CLASS_NewerVersionExists)) {
			continue;
		}

		Classes.AddUnique(*ClassIt);
	}

	// Search Blueprint classes.
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FString> ContentPaths;
		ContentPaths.Add(TEXT("/Game"));
		AssetRegistry.ScanPathsSynchronous(ContentPaths);

		FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
#else
		Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
#endif
		Filter.bRecursiveClasses = true;
		Filter.bRecursivePaths = true;

		TArray<FAssetData> AssetList;
		AssetRegistry.GetAssets(Filter, AssetList);

		for (FAssetData& Asset : AssetList) {
			UClass* GeneratedClass = nullptr;
			;
			if (GetClassFromBlueprint(Asset, Class, GeneratedClass)) {
				Classes.AddUnique(GeneratedClass);
			}
		}
	}

	return Classes;
}

#undef LOCTEXT_NAMESPACE

void UACFEditorSubsystem::OnAssetRenamed(const FAssetData& AssetData, const FString& OldPath)
{
	const FString NewPath = AssetData.GetSoftObjectPath().ToString();
	WriteRedirectToConfig(OldPath, NewPath);
}

void UACFEditorSubsystem::WriteRedirectToConfig(const FString& OldPath, const FString& NewPath)
{
	// Build redirector line
	FString RedirectLine = FString::Printf(
		TEXT("(OldObjectName=\"%s\", NewObjectName=\"%s\")"),
		*OldPath, *NewPath);

	// Locate plugin Config directory
	FString PluginConfigFile;
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (Plugin.IsValid())
	{
		PluginConfigFile = Plugin->GetBaseDir() / TEXT("Config/DefaultEngine.ini");
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ACF Plugin not found, writing redirect to global Engine.ini"));
		PluginConfigFile = GEngineIni;
	}

	// Append redirect entry
	GConfig->SetArray(
		TEXT("/Script/Engine.Engine"),
		TEXT("ActiveObjectRedirects"),
		TArray<FString>{RedirectLine},
		PluginConfigFile
	);

	GConfig->Flush(false, PluginConfigFile);

	UE_LOG(LogTemp, Log, TEXT("Added redirect: %s -> %s"), *OldPath, *NewPath);
}