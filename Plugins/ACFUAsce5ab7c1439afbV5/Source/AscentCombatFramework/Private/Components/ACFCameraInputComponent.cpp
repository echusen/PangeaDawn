// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFCameraInputComponent.h"

UACFCameraInputComponent::UACFCameraInputComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UACFCameraInputComponent::SetTurnRateX(float NewTurnRateX)
{
    SetTurnRates(FVector2D(NewTurnRateX, TurnRates.Y));
}

void UACFCameraInputComponent::SetTurnRateY(float NewTurnRateY)
{
    SetTurnRates(FVector2D(TurnRates.X, NewTurnRateY));
}

void UACFCameraInputComponent::SetTurnRates(FVector2D NewTurnRates)
{
    if (!FMath::IsFinite(NewTurnRates.X) || !FMath::IsFinite(NewTurnRates.Y)
        || TurnRates == NewTurnRates) {
        return;
    }

    if (bHasModifier && ModifierMode == EACFCameraTurnRateModifier::Multiplier
        && (!FMath::IsFinite(NewTurnRates.X * Modifier.X) || !FMath::IsFinite(NewTurnRates.Y * Modifier.Y))) {
        return;
    }

    const FVector2D Previous = GetTurnRates();
    TurnRates = NewTurnRates;
    BroadcastIfChanged(Previous);
}

float UACFCameraInputComponent::GetTurnRateX() const
{
    return static_cast<float>(GetTurnRates().X);
}

float UACFCameraInputComponent::GetTurnRateY() const
{
    return static_cast<float>(GetTurnRates().Y);
}

FVector2D UACFCameraInputComponent::GetTurnRates() const
{
    return !bHasModifier ? TurnRates : (ModifierMode == EACFCameraTurnRateModifier::Override ? Modifier : TurnRates * Modifier);
}

bool UACFCameraInputComponent::SetTurnRateModifier(EACFCameraTurnRateModifier Mode, FVector2D Value)
{
    if ((Mode != EACFCameraTurnRateModifier::Override && Mode != EACFCameraTurnRateModifier::Multiplier)
        || !FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y)) {
        return false;
    }
    if (Mode == EACFCameraTurnRateModifier::Multiplier
        && (!FMath::IsFinite(TurnRates.X * Value.X) || !FMath::IsFinite(TurnRates.Y * Value.Y))) {
        return false;
    }

    const FVector2D Previous = GetTurnRates();
    ModifierMode = Mode;
    Modifier = Value;
    bHasModifier = true;
    BroadcastIfChanged(Previous);
    return true;
}

void UACFCameraInputComponent::ClearModifier()
{
    const FVector2D Previous = GetTurnRates();
    bHasModifier = false;
    ModifierMode = EACFCameraTurnRateModifier::Multiplier;
    Modifier = FVector2D(1.0, 1.0);
    BroadcastIfChanged(Previous);
}

void UACFCameraInputComponent::ResetTurnRates()
{
    // Use the archetype rather than only the class CDO to preserve Blueprint component defaults.
    const UACFCameraInputComponent* Defaults = Cast<UACFCameraInputComponent>(GetArchetype());
    if (!Defaults) {
        Defaults = GetDefault<UACFCameraInputComponent>();
    }
    const FVector2D Previous = GetTurnRates();
    TurnRates = Defaults->TurnRates;
    bHasModifier = false;
    ModifierMode = EACFCameraTurnRateModifier::Multiplier;
    Modifier = FVector2D(1.0, 1.0);
    BroadcastIfChanged(Previous);
}

void UACFCameraInputComponent::BroadcastIfChanged(FVector2D Previous)
{
    const FVector2D Current = GetTurnRates();
    if (Previous != Current) {
        OnTurnRatesChanged.Broadcast(Current);
    }
}
