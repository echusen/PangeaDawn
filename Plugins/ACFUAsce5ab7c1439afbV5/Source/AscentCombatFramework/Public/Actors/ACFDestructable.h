// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Actors/ACFActor.h"

#include "ACFDestructable.generated.h"

class UACFDestructableComponent;
class UGeometryCollectionComponent;

/**
 * Convenience ACF actor for destructible props: a Geometry Collection root plus a
 * UACFDestructableComponent that owns all the destruction logic and settings.
 * Any other actor can become destructible by adding the component itself.
 */
UCLASS(Blueprintable, BlueprintType)
class ASCENTCOMBATFRAMEWORK_API AACFDestructable : public AACFActor {
    GENERATED_BODY()

public:
    AACFDestructable();

    UFUNCTION(BlueprintPure, Category = ACF)
    FORCEINLINE UACFDestructableComponent* GetDestructableComponent() const { return DestructableComp; }

protected:
    virtual void BeginPlay() override;

    /** Chaos visual / collision body. Attach a fractured Geometry Collection asset in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ACF|Destructable")
    TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComp;

    /** Owns all the destruction logic and settings. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ACF|Destructable")
    TObjectPtr<UACFDestructableComponent> DestructableComp;

    UFUNCTION()
    void HandleDestructionTriggered();
};
