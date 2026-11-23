// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/SkinnedMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Game/ACFTypes.h"
#include <AttributeSet.h>
#include <Curves/CurveFloat.h>
#include <GameplayTagContainer.h>

#include "ACFDeveloperSettings.generated.h"

UCLASS(config = Plugins, Defaultconfig, meta = (DisplayName = "Ascent Combat Settings"))
class ASCENTCOMBATFRAMEWORK_API UACFDeveloperSettings : public UDeveloperSettings {
    GENERATED_BODY()

public:
    UACFDeveloperSettings();

    /*Default COMBAT TAGS */

    UPROPERTY(EditAnywhere, config, meta = (Categories = "Actions"), Category = "ACF|Default Tags")
    FGameplayTag DefaultHitState;

    UPROPERTY(EditAnywhere, config, meta = (Categories = "Actions"), Category = "ACF|Default Tags")
    FGameplayTag DefaultDeathState;

    /** When true, the game is running as a demo build (e.g. for packaging). Use UACFFunctionLibrary::IsDemoVersion() to read at runtime. */
    UPROPERTY(EditAnywhere, config, Category = "ACF|Packaging")
    bool bIsDemoVersion = false;

    /** When true, every skeletal mesh initialized from a Character Data Asset gets the animation tick
     * settings below, instead of the ones set on the mesh component in the Blueprint.
     * Runtime retargeting: the hidden source mesh gets these settings and is treated as rendered whenever its
     * visible target is, while the target keeps AlwaysTickPoseAndRefreshBones so weapon sockets stay valid off-screen.
     * Dedicated servers always tick the full pose regardless of this. */
    UPROPERTY(EditAnywhere, config, Category = "ACF|Animation Performance")
    bool bOverrideMeshAnimTickSettings = true;

    /** Applied to meshes initialized from a Character Data Asset when bOverrideMeshAnimTickSettings is true.
     * OnlyTickMontagesAndRefreshBonesWhenPlayingMontages keeps attacks, notifies, root motion and weapon
     * traces working off-screen while skipping the locomotion pose. Never use OnlyTickPoseWhenRendered
     * for characters that must fight off-screen: montages would freeze. */
    UPROPERTY(EditAnywhere, config, Category = "ACF|Animation Performance", meta = (EditCondition = "bOverrideMeshAnimTickSettings"))
    EVisibilityBasedAnimTickOption DefaultMeshAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;

    /** Enables Update Rate Optimizations (animation evaluated every N frames by distance / off-screen, interpolated)
     * on meshes initialized from a Character Data Asset when bOverrideMeshAnimTickSettings is true. */
    UPROPERTY(EditAnywhere, config, Category = "ACF|Animation Performance", meta = (EditCondition = "bOverrideMeshAnimTickSettings"))
    bool bDefaultEnableUpdateRateOptimizations = true;
};
