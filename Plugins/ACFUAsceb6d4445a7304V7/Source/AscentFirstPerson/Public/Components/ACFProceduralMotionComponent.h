// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/ACFFirstPersonTypes.h"
#include "Debug/ACFProceduralMotionDebug.h"
#include "Animation/AnimNodeBase.h"
#include "BoneIndices.h"
#include "ACFProceduralMotionComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UACFProceduralPreset;
class UACFProceduralPresetData;

/**
 * Base for every procedural motion source.
 *
 * Subclasses emit FACFProceduralMotionOutput ops that FAnimNode_ACFProceduralMotion composes onto the weapon
 * holder bone, ordered by Stage. See Docs/ARCHITECTURE.md for the stage table and composition rules.
 */
UCLASS(Blueprintable, Abstract)
class ASCENTFIRSTPERSON_API UACFProceduralMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACFProceduralMotionComponent(const FObjectInitializer& ObjectInitializer);

	/** Game thread. Called by the anim node in PreUpdate. Subclasses override ComputeOutputs instead. */
	void UpdateOutput(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input);

	/** Worker thread. Samples the incoming pose before the stack is applied. Pose reads only, no UObject mutation. */
	virtual void BeforeEvaluate(float DeltaSeconds, FComponentSpacePoseContext& Context, FCompactPoseBoneIndex& BoneIndex);

	/** Worker thread. Appends pose-derived ops after BeforeEvaluate. Subclasses override ComputePoseOutputs. */
	void UpdatePoseOutputs(float DeltaSeconds, const FACFProceduralMotionPoseContext& Context);

	/**
	 * Whether this component reads FACFProceduralMotionPoseContext::ReferenceBoneLocal.
	 *
	 * The node only evaluates its Reference Pose branch when something asks for it, so returning true
	 * without needing it costs a full extra pose evaluation per frame.
	 */
	virtual bool NeedsReferencePose() const { return false; }

	const TArray<FACFProceduralMotionOutput>& GetCachedOutputs() const { return CachedOps; }

	/** Returns 0 for a null instance or an unset name. */
	static float ReadCurve(const UAnimInstance* AnimInstance, const FName& CurveName, const float DefaultValue = 0.0f);

	/**
	 * The preset class this component reads, or null if it reads none.
	 *
	 * Declares the component-to-preset pairing that otherwise only exists inside the .cpp, so tooling can
	 * spot a preset data asset that is missing the preset a component needs.
	 */
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const { return nullptr; }

#if ACF_FP_DEBUG_CAPTURE
	/**
	 * Internal state worth showing next to this component's ops in the Motion Inspector and showdebug.
	 *
	 * Only called while ACF.FirstPerson.Debug.Capture is on, and compiled out of Shipping. Runs on the animation worker
	 * thread alongside BeforeEvaluate, so it has the same rules: read this component's own state, format it,
	 * mutate nothing.
	 */
	virtual void GetDebugState(TArray<FACFProceduralMotionDebugValue>& OutValues) const {}
#endif

	/** PresetData, or the owner's when it is unset and bInheritPresetDataFromOwner is on. May be null. */
	UFUNCTION(BlueprintPure, Category="Settings")
	UACFProceduralPresetData* GetEffectivePresetData() const { return PresetData ? PresetData : InheritedPresetData; }

	/** Re-resolves the inherited preset data. Only needed if the owner's preset data changes at runtime. */
	UFUNCTION(BlueprintCallable, Category="Settings")
	void RefreshInheritedPresetData();

	/** False only while bRequirePreset is on and the effective preset data lacks this component's preset. */
	UFUNCTION(BlueprintPure, Category="Settings")
	bool HasRequiredPreset() const;

protected:
	//~ Begin UActorComponent
	virtual void OnRegister() override;
	//~ End of UActorComponent

	/** Emit zero or more ops. Preferred entry point. */
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs);

	UFUNCTION(BlueprintNativeEvent, meta=(DisplayName="Compute Outputs"))
	void BP_ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs);
	virtual void BP_ComputeOutputs_Implementation(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs);

	/** Emit a single op. Return false to emit none. */
	virtual bool ComputeOutput(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, FACFProceduralMotionOutput& Out);

	UFUNCTION(BlueprintNativeEvent, meta=(DisplayName="Compute Output"))
	bool BP_ComputeOutput(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, FACFProceduralMotionOutput& Out);
	virtual bool BP_ComputeOutput_Implementation(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, FACFProceduralMotionOutput& Out) { return false; }

	/**
	 * Emit ops that need the incoming pose (bone positions, parent transforms).
	 *
	 * Runs on the animation worker thread immediately before the stack is applied, so anything read here is
	 * current-frame rather than a frame stale. Pose reads only.
	 */
	virtual void ComputePoseOutputs(float DeltaSeconds, const FACFProceduralMotionPoseContext& Context, TArray<FACFProceduralMotionOutput>& Outs);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	TObjectPtr<UACFProceduralPresetData> PresetData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", AdvancedDisplay)
	bool bShowInConfigurator = true;

	/**
	 * Fall back to a preset data asset found on the owning actor when PresetData is unset.
	 *
	 * On by default since 1.3, so preset data set once on an actor reaches every motion component on it.
	 * Turn off to keep a component on its preset class defaults while its owner carries preset data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", AdvancedDisplay)
	bool bInheritPresetDataFromOwner = true;

	/**
	 * Emit nothing unless the effective preset data carries this component's preset, instead of running on the
	 * preset's class defaults. For components that live on the character whatever it holds, where a missing
	 * preset means the held item wants no such motion. The first person component sets it on its owner's.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", AdvancedDisplay)
	bool bRequirePreset = false;

	/** Composition order. Lower stages apply first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Procedural Motion")
	int32 Stage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Procedural Motion")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Procedural Motion")
	bool bApplyInFirstPerson = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Procedural Motion")
	bool bApplyInThirdPerson = true;

protected:
	/** Character owning the mesh being animated, not necessarily this component's owner. */
	UPROPERTY(BlueprintReadOnly, Category="Procedural Motion")
	TObjectPtr<ACharacter> AnimOwnerCharacter;

	UPROPERTY(BlueprintReadOnly, Category="Procedural Motion")
	TObjectPtr<const UAnimInstance> CachedAnimInstance;

private:
	/** Resolved once on registration, so GetEffectivePresetData stays a trivial read on the hot path. */
	UPROPERTY(Transient)
	TObjectPtr<UACFProceduralPresetData> InheritedPresetData;

	TArray<FACFProceduralMotionOutput> CachedOps;

	/** Result of the enabled and first/third person gates from the last UpdateOutput. */
	bool bAcceptingOutputs = false;
};
