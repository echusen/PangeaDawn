// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ATSTargetingFunctionLibrary.generated.h"


class UATSTargetPointComponent;

/**
 *
 */
UCLASS()
class ASCENTTARGETINGSYSTEM_API UATSTargetingFunctionLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()

public:
    /**
     * Retrieves the actor currently targeted by the specified pawn.
     * @param pawn - The pawn whose targeted actor needs to be retrieved.
     * @return The targeted AActor, or nullptr if no target is found.
     */
    UFUNCTION(BlueprintPure, Category = "ATS")
    static AActor* GetTargetedActor(const APawn* pawn);

    /**
     * Retrieves the current target point component of the specified pawn.
     * @param pawn - The pawn whose target point component needs to be retrieved.
     * @return The UATSTargetPointComponent representing the current target point, or nullptr if none exists.
     */
    UFUNCTION(BlueprintPure, Category = "ATS")
    static UATSTargetPointComponent* GetCurrentTargetPoint(const APawn* pawn);

    /**
     * World location a look-at / aim should point at this frame, with the optional eye-height
     * correction already applied. Game Thread only: it reads the target actor's transform.
     * @param pawn The pawn whose targeting component to query (usually the mesh owner).
     * @param OutLocation World location of the look-at point.
     * @return False when there is no targeting component or no target.
     */
    UFUNCTION(BlueprintPure, Category = ATS, meta = (DisplayName = "Get Look At Location", Keywords = "look at target aim lock on head"))
    static bool GetLookAtLocation(const APawn* pawn, FVector& OutLocation);
};
