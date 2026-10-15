// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/ACFFirstPersonTypes.h"

/** Debug capture is compiled out of Shipping entirely, and cvar-gated everywhere else. */
#define ACF_FP_DEBUG_CAPTURE (!UE_BUILD_SHIPPING)

#if ACF_FP_DEBUG_CAPTURE

class UACFProceduralMotionComponent;
class USkeletalMeshComponent;

/** One op as it was applied, plus who emitted it. */
struct FACFProceduralMotionDebugOp
{
	FTransform Transform = FTransform::Identity;

	float Weight = 1.0f;

	int32 Stage = 0;

	EACFProceduralMotionOutputSpace Space = EACFProceduralMotionOutputSpace::Component;

	EACFProceduralMotionComposition Composition = EACFProceduralMotionComposition::Additive;

	bool bMaskLocation = false;

	bool bMaskRotation = false;

	/** Captured by value: the panel reads on the game thread, long after the worker wrote this. */
	FString ComponentName;

	TWeakObjectPtr<UACFProceduralMotionComponent> Component;
};

/** One named value a component chose to publish about itself. */
struct FACFProceduralMotionDebugValue
{
	FACFProceduralMotionDebugValue() = default;
	FACFProceduralMotionDebugValue(FName InName, FString&& InValue)
		: Name(InName)
		, Value(MoveTemp(InValue))
	{
	}

	FName Name;

	/** Preformatted: the component knows how its own state should read, the panel does not. */
	FString Value;
};

/**
 * One harvested component, and whatever internal state it publishes.
 *
 * Ops alone do not explain a stateful component — a locomotion cycle's active state and phase, a recoil
 * burst's shot count. GetDebugState is where that comes from.
 */
struct FACFProceduralMotionDebugComponent
{
	TWeakObjectPtr<UACFProceduralMotionComponent> Component;

	TArray<FACFProceduralMotionDebugValue> Values;
};

/** What one Procedural Motion node did on its last evaluation. */
struct FACFProceduralMotionDebugSnapshot
{
	TWeakObjectPtr<const USkeletalMeshComponent> Mesh;

	FString MeshName;

	FName BoneName;

	/** The composed bone transform the node emitted, in component space. */
	FTransform ResultCS = FTransform::Identity;

	float AimingState = 0.0f;

	bool bFirstPerson = false;

	bool bHasReferencePose = false;

	TArray<FACFProceduralMotionDebugOp> Ops;

	/** Every component harvested this frame, including ones that emitted nothing. */
	TArray<FACFProceduralMotionDebugComponent> Components;

	/** Every curve on the pose, for the mask and driver readout. */
	TArray<TPair<FName, float>> Curves;

	double TimeSeconds = 0.0;
};

/**
 * Latest snapshot per mesh, written by the anim node on a worker thread and read on the game thread.
 *
 * One capture, several front-ends: the editor Motion Inspector, `showdebug ACFFirstPerson`, and the
 * viewport gizmos all read from here. See Docs/EDITOR-TOOLING.md § 6.
 */
class ASCENTFIRSTPERSON_API FACFProceduralMotionDebugRegistry
{
public:
	static FACFProceduralMotionDebugRegistry& Get();

	/** False unless ACF.FirstPerson.Debug.Capture is on, so an idle build pays one branch per evaluation. */
	static bool IsCaptureEnabled();

	/** Registers the showdebug hook. Called from module startup. */
	static void Register();

	static void Unregister();

	void Publish(FACFProceduralMotionDebugSnapshot&& Snapshot);

	/** Snapshots seen within StaleSeconds, newest first. */
	void GetSnapshots(TArray<FACFProceduralMotionDebugSnapshot>& OutSnapshots) const;

	bool GetSnapshotForMesh(const USkeletalMeshComponent* Mesh, FACFProceduralMotionDebugSnapshot& OutSnapshot) const;

	/** A snapshot not refreshed within this long is dropped — the mesh stopped evaluating. */
	static constexpr double StaleSeconds = 1.0;

private:
	mutable FCriticalSection Mutex;

	TArray<FACFProceduralMotionDebugSnapshot> Snapshots;
};

#endif
