// Copyright (C) Developed by Pask & OlssonDev, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/ACFActorFactory.h"
#include "IAssetTools.h"
#include "IPlacementModeModule.h"
#include "UObject/Object.h"
#include "ACFEditorTypes.h"


#include "ACFEditorSubsystem.generated.h"

class UACFEditorSettings;
class UACFAssetAction;
class UAssetCreator_ActorFactory;
class UACFBaseFactory;

UCLASS()
class ASCENTEDITOREXTENSIONS_API UACFEditorSubsystem : public UEditorSubsystem {
	GENERATED_BODY()

	// USubsystem interface implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;
	// End of implementation

public:
	// Check if class already has a factory
	static bool HasFactory(UClass* Class);

	// Gather all derived classes from the given class, including blueprints.
	TArray<UClass*> GatherAllAssetsOfClass(UClass* Class);

	// Callback fired when an asset is renamed or moved 
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);


private:
	void OnEditorOpened(const FString& FileName, bool bAsTemplate);

	// Refreshes both place actors menu and the categories in the content browser
	void RefreshActions();

	// Gets all the assets and refreshes the actions
	void RefreshAll();
	void RefreshAssetTypeActions();

	void RefreshPlacementMode();

	// When a Asset Creator has changed its properties.
	void OnAssetCreatorChanged(UObject* Object, FPropertyChangedEvent& ChangedEvent);

	void OnAssetCreated(const FAssetData& AssetData);
	void OnAssetsDeleted(const TArray<UClass*>& AssetsToDelete);

	UACFAssetAction* GetAssetAction(UClass* AssetActionClass);

	// Find the class from a Blueprint asset.
	bool GetClassFromBlueprint(const FAssetData Class, UClass* ClassToFind, UClass*& GeneratedClass);

	// Register category in content browser
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	// Unregister category in content browser
	void UnregisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	// Make a Actor Factory from a Asset Action
	UACFActorFactory* MakeActorFactory(UACFAssetAction* AssetAction);

	UACFBaseFactory* MakeFactory(UACFAssetAction* AssetAction);
	UACFAssetAction* MakeActorAction(const FAssetActionSettings& assetSetting);
	UACFActorFactory* MakeActorFactoryFromClass(TSubclassOf<AActor> AssetClass);
	UACFEditorSettings* GetEditorSettings() const;
	void WriteRedirectToConfig(const FString& OldPath, const FString& NewPath);

	/** Handle for the OnAssetRenamed binding so we can unbind safely */
	FDelegateHandle AssetRenamedHandle;
private:
	// Keep track of actor factories we have created to remove them when we refresh the actions.
	UPROPERTY(Transient)
	TArray<UActorFactory*> ActorFactories;

	// Keep all Factories of UACFBaseFactory loaded.
	UPROPERTY(Transient)
	TArray<UClass*> Factories;

	// Keep all asset actions loaded and cached. So we don't need to get them all the time.
	UPROPERTY(Transient)
	TArray<UClass*> AssetActions;

	UPROPERTY(Transient)
	TArray<UACFAssetAction*> AssetActionsInst;

	UPROPERTY(Transient)
	TArray<UACFBaseFactory*> BaseFactories;

	// Keep track of all AssetTypeActions so we can unregister them on refresh.
	TArray<TSharedRef<IAssetTypeActions>> AssetTypeActions;

	// Keep track of all Placeable Items so we can unregister them on refresh.
	TArray<TOptional<FPlacementModeID>> RegisteredPlaceableItems;

	// Keep track of all Placeable Items categories so we can unregister them on refresh.
	TArray<FPlacementCategoryInfo> RegisteredPlaceableItemCategories;

private:
	FDelegateHandle OnMapOpenedDelegate;

	friend UACFEditorSettings;
};
