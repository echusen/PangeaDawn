// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ACFCameraInputComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FACFCameraTurnRatesChanged, FVector2D, TurnRates);

UENUM(BlueprintType)
enum class EACFCameraTurnRateModifier : uint8
{
    Override,
    Multiplier
};

/** Local camera input settings. X is yaw, Y is pitch. Does not apply input or replicate. */
UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTCOMBATFRAMEWORK_API UACFCameraInputComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UACFCameraInputComponent();

    /** Effective rates, including the active modifier. */
    UFUNCTION(BlueprintPure, Category = "ACF|Camera")
    FVector2D GetTurnRates() const;

    UFUNCTION(BlueprintPure, Category = "ACF|Camera")
    FVector2D GetBaseTurnRates() const { return TurnRates; }

    UFUNCTION(BlueprintPure, Category = "ACF|Camera")
    float GetTurnRateX() const;

    UFUNCTION(BlueprintPure, Category = "ACF|Camera")
    float GetTurnRateY() const;

    UFUNCTION(BlueprintPure, Category = "ACF|Camera")
    float GetBaseTurnRateX() const { return static_cast<float>(TurnRates.X); }

    UFUNCTION(BlueprintPure, Category = "ACF|Camera")
    float GetBaseTurnRateY() const { return static_cast<float>(TurnRates.Y); }

    UFUNCTION(BlueprintCallable, Category = "ACF|Camera")
    void SetTurnRateX(float NewTurnRateX);

    UFUNCTION(BlueprintCallable, Category = "ACF|Camera")
    void SetTurnRateY(float NewTurnRateY);

    /** Updates base rates without clearing the modifier. Ignores non-finite inputs/results. */
    UFUNCTION(BlueprintCallable, Category = "ACF|Camera")
    void SetTurnRates(FVector2D NewTurnRates);

    /** Clears the modifier and restores the component template defaults (native default: 30 per axis). */
    UFUNCTION(BlueprintCallable, Category = "ACF|Camera")
    void ResetTurnRates();

    /** Replaces the single active modifier. Remains active until ClearModifier or ResetTurnRates.
     * Override values are rates; Multiplier values are factors (1 = unchanged).
     * Returns false for invalid mode, non-finite values or overflow, leaving state unchanged. */
    UFUNCTION(BlueprintCallable, Category = "ACF|Camera")
    bool SetTurnRateModifier(EACFCameraTurnRateModifier Mode, FVector2D Value);

    /** Removes the modifier without changing the current base rates. */
    UFUNCTION(BlueprintCallable, Category = "ACF|Camera")
    void ClearModifier();

    UFUNCTION(BlueprintPure, Category = "ACF|Camera")
    bool HasTurnRateModifier() const { return bHasModifier; }

    /** Fired once when effective rates change, including modifier application, removal and reset. */
    UPROPERTY(BlueprintAssignable, Category = "ACF|Camera")
    FACFCameraTurnRatesChanged OnTurnRatesChanged;

private:
    void BroadcastIfChanged(FVector2D Previous);

    UPROPERTY(Transient)
    bool bHasModifier = false;

    UPROPERTY(Transient)
    EACFCameraTurnRateModifier ModifierMode = EACFCameraTurnRateModifier::Multiplier;

    UPROPERTY(Transient)
    FVector2D Modifier = FVector2D(1.0, 1.0);

    // Configure these defaults to match the existing controller Blueprint before migrating input.
    UPROPERTY(EditDefaultsOnly, Category = "ACF|Camera")
    FVector2D TurnRates = FVector2D(30.0, 30.0);
};
