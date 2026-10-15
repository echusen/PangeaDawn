// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFCraftRecipeDataAsset.h"
#include "ACFItemTypes.h"
#include "Components/ACFEquipmentComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/ACFItem.h"
#include <Actors/ACFCharacter.h>

#include "ACFItemsManagerComponent.generated.h"

struct FInventoryItem;
class UACFVendorComponent;
class AACFCharacterPreviewStage;
class ACharacter;
class UTextureRenderTarget2D;
class USpotLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemCrafted, const FACFCraftingRecipe&, recipe);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemSold, const FInventoryItem&, item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemPurchased, const FInventoryItem&, item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemUpgraded, const FInventoryItem&, item);

UCLASS(ClassGroup = (ACF), Blueprintable, meta = (BlueprintSpawnableComponent))
class CRAFTINGSYSTEM_API UACFItemsManagerComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFItemsManagerComponent();

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    /**
     * Returns the Items DataTable used for item generation
     * @return The item database DataTable.
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    UDataTable* GetItemsDB() const { return ItemsDB; }

protected:
    // Called when the game starts
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (RowType = "ItemGenerationSlot"), Category = ACF)
    UDataTable* ItemsDB;

    //UPROPERTY(EditAnywhere, Category = ACF)
    //TSubclassOf<AACFCharacter> PreviewCharacterClass;

    UPROPERTY()
    TObjectPtr<AACFCharacter> PreviewCharacter;

    UPROPERTY(Transient)
    TObjectPtr<USpotLightComponent> ItemPreviewLight;

    /** Spawned instance of the item preview stage (spawned on demand via SpawnItemPreviewStage). */
    UPROPERTY(Transient)
    AACFCharacterPreviewStage* ItemPreviewStage = nullptr;

    /** Handle for the in-flight async load of ItemPreviewStageClass, if any. */
    TSharedPtr<FStreamableHandle> PreviewStreamableHandle;

    /** Async load completion callback for ItemPreviewStageClass - equivalent of UACFBaseGroupComponent::OnAIAssetsLoaded.
     * Spawns the preview stage now that its class is loaded, positions any characters queued while loading,
     * and broadcasts OnItemPreviewReady. */
    void OnItemPreviewAssetsLoaded();

    /** Spawns the preview stage actor now that ItemPreviewStageClass is guaranteed to be loaded. */
    AACFCharacterPreviewStage* Internal_SpawnLoadedPreviewStage();

public:
    /*------------------- SERVER SIDE -----------------------------------*/

    /**
     * Server RPC to buy an item from a vendor
     * @param item - The item to buy.
     * @param instigator - The pawn performing the action.
     * @param vendorComp - The vendor component handling the transaction.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void BuyItems(const FInventoryItem& item, APawn* instigator, class UACFVendorComponent* vendorComp);

    /**
     * Server RPC to sell items to a vendor
     * @param itemTobeSold - The item to sell.
     * @param instigator - The pawn performing the action.
     * @param count - Quantity of items to sell.
     * @param vendorComp - The vendor component receiving the item.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void SellItemsToVendor(const FInventoryItem& itemTobeSold, APawn* instigator, int32 count, class UACFVendorComponent* vendorComp);

    /**
     * Server RPC to craft an item using a recipe
     * @param ItemToCraft - The recipe of the item to craft.
     * @param instigator - The pawn performing the crafting.
     * @param craftingComp - The crafting component handling the logic.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void CraftItem(const FACFCraftingRecipe& ItemToCraft, APawn* instigator, class UACFCraftingComponent* craftingComp);

    /**
     * Server RPC to upgrade an existing item
     * @param itemToUpgrade - The item to upgrade.
     * @param instigator - The pawn performing the upgrade.
     * @param craftingComp - The crafting component applying the upgrade.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void UpgradeItem(const FInventoryItem& itemToUpgrade, APawn* instigator, class UACFCraftingComponent* craftingComp);

    /**
     * Server RPC to drop an item from the inventory
     * @param targetComponent - The equipment component from which the item will be dropped.
     * @param itemToDrop - The inventory item to drop.
     * @param count - The number of items to drop. Defaults to 1.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void DropItem(UACFEquipmentComponent* targetComponent, const FInventoryItem& itemToDrop, int32 count = 1);

    /**
     * Server RPC to use an item directly from the inventory
     * @param targetComponent - The equipment component that will use the item.
     * @param itemToUse - The inventory item to use.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void UseInventoryItem(UACFEquipmentComponent* targetComponent, const FInventoryItem& itemToUse);

    /**
     * Server RPC to equip an inventory item into a specific slot
     * @param targetComponent - The equipment component where the item will be equipped.
     * @param itemToUse - The inventory item to equip.
     * @param slot - The gameplay tag identifying the equipment slot.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void EquipItemFromInventoryInSlot(UACFEquipmentComponent* targetComponent, const FInventoryItem& itemToUse, const FGameplayTag& slot);

    /**
     * Server RPC to construct a buildable object
     * @param instigator - The pawn initiating the construction.
     * @param buildComp - The buildable component managing the construction.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
    void ConstructBuildable(APawn* instigator, class UACFBuildableComponent* buildComp);

    /* Generates an array of FBaseItem matching the provided rules by selecting them from the provided ItemsDB
    returns true only if we are able to find matching items for ALL the provided rules*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GenerateItemsFromRules(const TArray<FACFItemGenerationRule>& generationRules, TArray<FBaseItem>& outItems);

    /* Generates an  FBaseItem matching the provide rule by selecting it from the provided ItemsDB
    returns true if at least one item is found*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool GenerateItemFromRule(const FACFItemGenerationRule& generationRules, FBaseItem& outItems);

    /* Returns true if the provided itemSlot matches the provided slot rules*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    bool DoesSlotMatchesRule(const FACFItemGenerationRule& generationRules, const FItemGenerationSlot& item);

    void BuyItemsFromVendor(UACFVendorComponent* vendorComp, const FInventoryItem& item, UACFEquipmentComponent* equipComp);

    /*------------------- ITEM PREVIEW -----------------------------------*/

    /** Class of the preview stage actor to spawn for showing the owning character wearing newly bought/crafted/upgraded gear.
     * Soft reference: loaded asynchronously (see OnItemPreviewAssetsLoaded) so opening a shop/crafting/equipment panel never hitches on it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Preview")
    TSoftClassPtr<AACFCharacterPreviewStage> ItemPreviewStageClass;

    /** Transform used to spawn the item preview stage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Preview")
    FTransform ItemPreviewStageTransform = FTransform::Identity;
    /** Radius used when positioning the previewed character(s) on the stage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Preview", meta = (ClampMin = "0.0"))
    float ItemPreviewSphereRadius = 50.f;

    /** Spacing multiplier used when positioning multiple previewed characters on the stage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Preview")
    float ItemPreviewSpacingMultiplier = 1.0f;

    /** Render target the item preview can be drawn into (e.g. to display the previewed character in UI). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview")
    TObjectPtr<UTextureRenderTarget2D> ItemPreviewRenderTarget;

    /** Spawn a local spotlight on the preview copy only, never on the gameplay character. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting")
    bool bEnableItemPreviewLight = true;

    /** Offset in character-root space (cm): X is forward, Z is up, relative to capsule center. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting")
    FVector ItemPreviewLightOffset = FVector(200.f, 0.f, 150.f);

    /** Point the cone at this position in character-root space (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting")
    FVector ItemPreviewLightTargetOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting", meta = (ClampMin = "0.0", Units = "lm"))
    float ItemPreviewLightIntensity = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting")
    FLinearColor ItemPreviewLightColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting", meta = (ClampMin = "1.0", Units = "cm"))
    float ItemPreviewLightAttenuationRadius = 600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
    float ItemPreviewLightInnerConeAngle = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Preview|Lighting", meta = (ClampMin = "1.0", ClampMax = "89.0", Units = "deg"))
    float ItemPreviewLightOuterConeAngle = 60.f;

    /** Creates the light on demand, or updates the existing one. Returns null before the preview exists. */
    UFUNCTION(BlueprintCallable, Category = "ACF|Preview|Lighting")
    USpotLightComponent* SpawnItemPreviewLight();

    /** Reapply editable lighting settings after changing them at runtime. */
    UFUNCTION(BlueprintCallable, Category = "ACF|Preview|Lighting")
    void UpdateItemPreviewLightSettings();

    /** Enable/disable the light; remembers the choice if the preview is still loading. */
    UFUNCTION(BlueprintCallable, Category = "ACF|Preview|Lighting")
    void SetItemPreviewLightEnabled(bool bEnabled);

    /**
     * Spawns the item preview stage actor from ItemPreviewStageClass at ItemPreviewStageTransform.
     * Safe to call multiple times - returns the existing instance if already spawned.
     * ItemPreviewStageClass is a soft reference, so if it isn't loaded yet this kicks off an async load
     * and returns nullptr; OnItemPreviewAssetsLoaded finishes the spawn once loading completes and
     * broadcasts OnItemPreviewReady. Use IsItemPreviewReady()/bind OnItemPreviewReady to know when it's safe
     * to show the preview panel, mirroring UACFPartyComponent::IsPartyReady()/OnPartyChanged.
     * Intended to be called client-side by shop/crafting/equipment UI before showing the preview panel.
     * @return The spawned (or already-existing) preview stage instance, or nullptr if still loading or ItemPreviewStageClass is unset.
     */
    UFUNCTION(BlueprintCallable, Category = "ACF|Preview")
    AACFCharacterPreviewStage* SpawnItemPreviewStage();

    /**
     * Positions the given character(s) on the item preview stage, spawning the stage first if needed.
     * Call this (from UI) after equipping/previewing gear on the character to lay them out on the stage.
     * If the stage class is still loading, the characters are queued and positioned automatically once
     * OnItemPreviewAssetsLoaded fires.
     * @return The preview stage instance used, or nullptr if still loading or on failure (e.g. ItemPreviewStageClass unset).
     */
    UFUNCTION(BlueprintCallable, Category = "ACF|Preview")
    void MoveCharacterToItemPreviewStage();

    /**
     * Refreshes/re-applies the previously-computed preview transform for the given character
     * without recalculating layout. Call this after equipment changes when the character count
     * on the stage hasn't changed. No-ops (with a warning) if the stage hasn't been spawned yet.
     */
    UFUNCTION(BlueprintCallable, Category = "ACF|Preview")
    void RefreshItemPreviewStage();

    /** Returns the currently-spawned item preview stage, or nullptr if not yet spawned. */
    UFUNCTION(BlueprintPure, Category = "ACF|Preview")
    AACFCharacterPreviewStage* GetItemPreviewStage() const { return ItemPreviewStage; }

    /** Returns the currently-spawned preview preview character, or nullptr if not yet spawned. */
    UFUNCTION(BlueprintPure, Category = "ACF|Preview")
    AACFCharacter* GetPreviewCharacter() const { return PreviewCharacter; }

    // void ApplyAppearanceFromSkeletalMeshes(const TArray<USkeletalMeshComponent*>& SkeletalComponents, const TObjectPtr<AACFCharacter> Character);
	
    UFUNCTION(BlueprintCallable, Category = "ACF|Preview")
    void ApplyAppearance(const FEquipment& Equipment);


    /**
     * Event triggered when an item is crafted successfully
     */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnItemCrafted OnItemCrafted;

    /**
     * Event triggered when an item is sold to a vendor
     */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnItemSold OnItemSold;

    /**
     * Event triggered when an item is purchased from a vendor
     */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnItemPurchased OnItemPurchased;

    /**
     * Event triggered when an item is successfully upgraded
     */
    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnItemUpgraded OnItemUpgraded;

};
