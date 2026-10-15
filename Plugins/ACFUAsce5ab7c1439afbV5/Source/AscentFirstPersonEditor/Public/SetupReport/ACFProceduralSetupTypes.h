// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class FProperty;
class UActorComponent;
class UAnimBlueprint;
class UACFProceduralMotionComponent;
class UACFProceduralPresetData;
class USkeleton;

/** Strips blueprint template decoration, so "Offset_GEN_VARIABLE" reads as "Offset". */
ASCENTFIRSTPERSONEDITOR_API FString GetACFProceduralFriendlyName(const UObject* Object);

enum class EACFProceduralSetupSeverity : uint8
{
	/** Worth knowing. Never means anything is wrong. */
	Info,
	/** Legal, but almost certainly not what was intended. */
	Warning,
	/** Provably broken in any setup. */
	Error
};

/** One `UACFProceduralPresetData` property found by reflection, and what it currently points at. */
struct FACFProceduralPresetBinding
{
	/** Actor, component or CDO the property lives on. */
	TWeakObjectPtr<UObject> Owner;

	FProperty* Property = nullptr;

	TWeakObjectPtr<UACFProceduralPresetData> PresetData;

	/** "MyWeapon (Recoil Component)" — for display. */
	FText GetDisplayName() const;
};

/**
 * Everything a check can reach from whatever the user picked.
 *
 * Built by Build(), which resolves as much as it can and leaves the rest empty. A check must tolerate any
 * field being empty — that is the "discover, never require" rule in practice.
 */
struct FACFProceduralSetupContext
{
	/** Resolves everything reachable from InTarget. Safe to call with null. */
	void Build(UObject* InTarget);

	bool IsValid() const { return Target.IsValid(); }

	/** What the user picked: a skeleton, skeletal mesh, anim blueprint, actor blueprint, actor or preset data. */
	TWeakObjectPtr<UObject> Target;

	TWeakObjectPtr<USkeleton> Skeleton;

	TArray<TWeakObjectPtr<UAnimBlueprint>> AnimBlueprints;

	/** Component templates for a blueprint target, live components for an actor target. */
	TArray<TWeakObjectPtr<UACFProceduralMotionComponent>> MotionComponents;

	TArray<TWeakObjectPtr<UACFProceduralPresetData>> PresetDataAssets;

	/** Every reflected `UACFProceduralPresetData` property on the target and its components. */
	TArray<FACFProceduralPresetBinding> PresetBindings;

	/** True when Target is a live actor rather than an asset. */
	bool bIsLiveActor = false;
};

/** One row in the report. */
struct FACFProceduralSetupResult
{
	FName CheckId;

	EACFProceduralSetupSeverity Severity = EACFProceduralSetupSeverity::Info;

	FText Title;

	FText Detail;

	/** Selected or opened when the row is clicked. */
	TWeakObjectPtr<UObject> Subject;

	/** Appended to the plugin's DocsURL when set. */
	FString DocsAnchor;

	/** Shown as a button when bound. */
	FSimpleDelegate Fix;

	FText FixLabel;
};
