// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFActionTypes.h"
#include "ACFItemTypes.h"
#include "ARSTypes.h"
#include "CoreMinimal.h"
#include "Items/ACFItem.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ACFItemSystemFunctionLibrary.generated.h"

class UACFCurrencyComponent;
class UACFEquipmentComponent;
class UACFItemsManagerComponent;

/**
 *
 */
UCLASS()
class INVENTORYSYSTEM_API UACFItemSystemFunctionLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = ACFLibrary)
    static AACFWorldItem* SpawnWorldItemNearLocation(UObject* WorldContextObject, const TArray<FBaseItem>& ContainedItem, const FVector& location, float acceptanceRadius = 100.f);

    UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = ACFLibrary)
    static AACFWorldItem* SpawnCurrencyItemNearLocation(UObject* WorldContextObject, float currencyAmount, const FVector& location, float acceptanceRadius = 100.f);

    /* FILTERS */
    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static void FilterByItemType(const TArray<FInventoryItem>& inItems, EItemType inType, TArray<FInventoryItem>& outItems);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static void FilterByItemSlot(const TArray<FInventoryItem>& inItems, FGameplayTag inSlot, TArray<FInventoryItem>& outItems);

    UFUNCTION(BlueprintPure, Category = ACFLibrary, meta = (DeterminesOutputType = "FragmentClass"))
    static UACFItemFragment* FindFragmentByClass(const UACFItem* inItem, TSubclassOf<UACFItemFragment> FragmentClass);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool GetItemData(const TSubclassOf<class UACFItem>& item, FItemDescriptor& outData);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool GetEquippableAttributeSetModifier(const TSubclassOf<class UACFItem>& itemClass, FAttributesSetModifier& outModifier);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool GetConsumableTimedAttributeSetModifier(const TSubclassOf<class UACFItem>& itemClass, TArray<FTimedAttributeSetModifier>& outModifiers);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool GetConsumableStatModifier(const TSubclassOf<class UACFItem>& itemClass, TArray<FStatisticValue>& outModifiers);

    /*GAS GETTERS*/
    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool GetEquippableGameplayEffects(const TSubclassOf<class UACFItem>& itemClass, TArray<FGEModifier>& outModifiers);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool GetEquippableGEType(const TSubclassOf<class UACFItem>& itemClass, EGEType& outType);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool GetConsumableGameplayEffects(const TSubclassOf<class UACFItem>& itemClass, TArray<FGEModifier>& outModifiers);
    /**/

    UFUNCTION(BlueprintCallable, Category = ACFLibrary)
    static FBaseItem MakeBaseItemFromInventory(const FInventoryItem& inItem);

    UFUNCTION(BlueprintCallable, Category = ACFLibrary)
    static FTransform GetShootTransform(APawn* SourcePawn, EShootTargetType targetType, FVector& outSourceLoc);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static float GetPawnCurrency(const APawn* pawn);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static UACFEquipmentComponent* GetPawnEquipment(const APawn* pawn);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static UACFCurrencyComponent* GetPawnCurrencyComponent(const APawn* pawn);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static bool CanUseConsumableItem(const APawn* pawn, const TSubclassOf<class UACFConsumable>& itemClass);

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static FGameplayTag GetDesiredUseAction(const TSubclassOf<class UACFConsumable>& itemClass);

    /* DEFAULTS */
    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static TSubclassOf<class AACFWorldItem> GetDefaultWorldItemClass();

    UFUNCTION(BlueprintPure, Category = ACFLibrary)
    static FText GetDefaultCurrencyName();

    UFUNCTION(BlueprintCallable, Category = ACFLibrary)
    static FGameplayTag GetItemTypeTagRoot();

    UFUNCTION(BlueprintCallable, Category = ACFLibrary)
    static FGameplayTag GetItemSlotTagRoot();

    UFUNCTION(BlueprintCallable, Category = ACFLibrary)
    static bool IsValidItemTypeTag(FGameplayTag TagToCheck);

    UFUNCTION(BlueprintCallable, Category = ACFLibrary)
    static bool IsValidItemSlotTag(FGameplayTag TagToCheck);

    static AACFWorldItem* SpawnWorldItem(UObject* WorldContextObject, const FVector& location, float acceptanceRadius = 100.f);
};
