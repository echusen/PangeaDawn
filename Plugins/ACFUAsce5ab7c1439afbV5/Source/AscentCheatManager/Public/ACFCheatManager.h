// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/CheatManager.h"
#include <GameplayTagContainer.h>

#include "ACFCheatManager.generated.h"

struct FBaseItem;
class AACFCharacter;
class APawn;


/**
 *
 */
UCLASS()
class ASCENTCHEATMANAGER_API UACFCheatManager : public UCheatManager {
    GENERATED_BODY()

public:
    /** Discovers all available locations in the game world. */
    UFUNCTION(exec, Category = ACF)
    void DiscoverAllLocations();

    /** Grants the specified amount of experience points to the player.
     * @param Exps - The amount of experience to add.
     */
    UFUNCTION(exec, Category = ACF)
    void AddExp(float Exps);

    /** Adds the specified number of coins to the player's inventory.
     * @param Coins - The amount of coins to add.
     */
    UFUNCTION(exec, Category = ACF)
    void AddCurrency(float Coins);

    /** Advances the currently tracked quest by one step. */
    UFUNCTION(exec, Category = ACF)
    void ProceedInTrackedQuest();

    /** Instantly teleports the local pawn next to the currently tracked quest's target,
     * ignoring collision along the way, then waits for terrain to stream in before snapping to NavMesh.
     * @param Distance - How far from the target to land, in centimeters. */
    UFUNCTION(exec, Category = ACF)
    void TeleportToTarget(float Distance = 200.f);

    /** Starts all the quests defined in this cheat manager */
    UFUNCTION(exec, Category = ACF)
    void StartQuests();

    /** Adds the set of Items defined in this Cheat Manager to the local player's inventory. */
    UFUNCTION(exec, Category = ACF)
    void AddItemsToInventory();

    /** Adds one item by its display name. Quantity defaults to 1.
     *  Console: GiveItem <Display Name> [Quantity] */
    UFUNCTION(BlueprintCallable, Category = ACF, meta = (DisplayName = "Give Item By Name", Keywords = "cheat give item", AdvancedDisplay = "Quantity"))
    void GiveItemByName(const FString& ItemName, int32 Quantity = 1);

    /** Adds one item by Blueprint name. Quantity defaults to 1.
     *  Console: GiveItem -BP <Blueprint Name> [Quantity] */
    UFUNCTION(BlueprintCallable, Category = ACF, meta = (DisplayName = "Give Item By Blueprint", Keywords = "cheat give item blueprint", AdvancedDisplay = "Quantity"))
    void GiveItemByBlueprint(const FString& BlueprintName, int32 Quantity = 1);

    /** Console entry. The argument is the display name, unless -BP is set. */
    UFUNCTION(exec, BlueprintCallable, Category = ACF, meta = (DisplayName = "Give Item", Keywords = "cheat give item"))
    void GiveItem(const FString& Args);

    /** Same as GiveItem. Accepts a leading "item" word: Give item -BP Potion 2 */
    UFUNCTION(exec, BlueprintCallable, Category = ACF, meta = (DisplayName = "Give", Keywords = "cheat give item"))
    void Give(const FString& Args);


protected:

    /*Items to be added to the inventory when the AddItemToInventory cheat is triggered*/
    UPROPERTY(EditDefaultsOnly, Category = ACF)
    TArray<FBaseItem> ItemsToAdd;

    /*Quests to start when the StartQuests cheat is triggered*/
    UPROPERTY(EditDefaultsOnly, Category = ACF)
    TArray<FGameplayTag> QuestsToStart;

    /*Seconds to keep the pawn hovering after TeleportToTarget so World Partition / landscape can load.*/
    UPROPERTY(EditDefaultsOnly, Category = ACF, meta = (ClampMin = "0.0", ForceUnits = "s"))
    float TeleportTerrainLoadDuration = 2.f;

private:
    float CachedGravityScale = 1.f;

    TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;

    TWeakObjectPtr<class UCharacterMovementComponent> GravityDisabledMovement;

    TWeakObjectPtr<APawn> GravityDisabledPawn;

    float PendingNavSnapRadius = 1000.f;

    FTimerHandle GravityRestoreTimerHandle;

    static void TeleportPawnIgnoringPhysics(APawn* Pawn, const FVector& Dest, const FRotator& Rot);

    static bool SnapLocationToNavMesh(UObject* WorldContext, APawn* Pawn, FVector& InOutLocation, float SearchRadius);

    void DisablePawnGravityTemporarily(APawn* Pawn);

    void RestorePawnMovement();

    UFUNCTION()
    void FinishDelayedTeleport();

    UFUNCTION()
    void CheatMessage(const FString& Message, bool bWarning);

    UFUNCTION()
    int32 NormalizeGiveQuantity(int32 Quantity);

    UFUNCTION()
    bool IsGrantableItemClass(UClass* ItemClass) const;

    UFUNCTION()
    UClass* FindItemClassForGive(const FString& Query, bool bMatchBlueprintName);

    UFUNCTION()
    void GrantItemToPawn(UClass* ItemClass, int32 Quantity);
};
