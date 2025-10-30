// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFDamageType.h"
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"

#include "ACFGameMode.generated.h"

class UACMCollisionsMasterComponent;
class UACFRagdollMasterComponent;

/**
 * Base GameMode class for ACF-based games.
 * Defines game rules, player controller assignment, and authority-level behavior on the server.
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API AACFGameMode : public AGameMode {
    GENERATED_BODY()

public:
    AACFGameMode();

    /**
     * Returns the ragdoll manager component
     * @return Pointer to the ragdoll manager component
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE class UACFRagdollMasterComponent* GetRagdollManager() const { return RagdollManager; }

    /**
     * Returns the collisions manager component
     * @return Pointer to the collision manager component
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE class UACMCollisionsMasterComponent* GetCollisionsManager() const { return CollisionManager; }

    /**
     * Returns all currently connected ACF player controllers
     * @return Array of AACFPlayerController pointers
     */
    UFUNCTION(BlueprintPure, Category = ACF)
    TArray<class AACFPlayerController*> GetAllPlayerControllers();

protected:
    // Ragdoll manager component instance
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = ACF)
    TObjectPtr<UACFRagdollMasterComponent> RagdollManager;

    // Collisions manager component instance
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = ACF)
    TObjectPtr<UACMCollisionsMasterComponent> CollisionManager;
};
