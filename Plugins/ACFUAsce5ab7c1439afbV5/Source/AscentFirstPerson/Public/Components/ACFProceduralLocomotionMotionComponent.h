// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ACFProceduralMotionComponent.h"
#include "Presets/ACFProceduralPresetLocomotionMotion.h"
#include "ACFProceduralLocomotionMotionComponent.generated.h"

/** What the component knows about how the character is moving. Filled from the owner, or set by the game. */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralLocomotionInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion", meta=(ForceUnits="cm/s"))
	float GroundSpeed = 0.f;

	/** Signed. Negative is falling, which is what land impact is measured from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion", meta=(ForceUnits="cm/s"))
	float VerticalSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion")
	bool bGrounded = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion")
	bool bCrouched = false;
};

/** Playback cursor for one running event. Looked up by tag each frame, so editing the preset live is safe. */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralLocomotionEventPlayer
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Locomotion")
	FGameplayTag Tag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Locomotion")
	float Playback = 0.f;

	/** Counts down before playback starts. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Locomotion")
	float DelayRemaining = 0.f;

	/** Impact scale captured when it was triggered. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Locomotion")
	float Scale = 1.f;
};

/**
 * Movement cycles and one-shots, driven by math instead of animation assets.
 *
 * Idle, walk, run, sprint and in-air are states resolved from the character's speed; jump and land are
 * one-shots. Place it on the equipment for a per-weapon feel, or on the character for one feel across the
 * whole loadout — never both. See Docs/LOCOMOTION-MOTION.md.
 */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Locomotion Component"))
class ASCENTFIRSTPERSON_API UACFProceduralLocomotionMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralLocomotionMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
#if ACF_FP_DEBUG_CAPTURE
	virtual void GetDebugState(TArray<FACFProceduralMotionDebugValue>& OutValues) const override;
#endif
	//~ End of UACFProceduralMotionComponent

	//~ Begin UActorComponent
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End of UActorComponent

	/** Plays a one-shot from the preset's event list. Ignored when the tag is not in it. */
	UFUNCTION(BlueprintCallable, Category="Locomotion")
	void PlayLocomotionEvent(FGameplayTag EventTag, float ScaleMultiplier = 1.f);

	/**
	 * Plays locally and on every other client.
	 *
	 * Needs the component itself set to replicate. The automatic path does not: speed and movement mode are
	 * already replicated, so a remote proxy resolves the same states from the same inputs.
	 */
	UFUNCTION(BlueprintCallable, Category="Locomotion")
	void PlayLocomotionEventReplicated(FGameplayTag EventTag);

	UFUNCTION(Server, Reliable)
	void ServerPlayLocomotionEvent(FGameplayTag EventTag);

	UFUNCTION(BlueprintCallable, Category="Locomotion")
	void StopLocomotionEvent(FGameplayTag EventTag);

	UFUNCTION(BlueprintCallable, Category="Locomotion")
	void StopAllLocomotionEvents();

	/** Only has an effect when the preset's Driver is Manual. */
	UFUNCTION(BlueprintCallable, Category="Locomotion")
	void SetLocomotionState(FGameplayTag StateTag);

	/** Feeds the resolver when the preset's bAutoReadFromOwner is off. */
	UFUNCTION(BlueprintCallable, Category="Locomotion")
	void SetLocomotionInput(const FACFProceduralLocomotionInput& NewInput) { LocomotionInput = NewInput; }

	UFUNCTION(BlueprintPure, Category="Locomotion")
	const FACFProceduralLocomotionInput& GetLocomotionInput() const { return LocomotionInput; }

	UFUNCTION(BlueprintPure, Category="Locomotion")
	FGameplayTag GetActiveStateTag() const { return ActiveStateTag; }

	/** Cycle position in [0,1). */
	UFUNCTION(BlueprintPure, Category="Locomotion")
	float GetCyclePhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category="Locomotion")
	float GetCycleRate() const { return CurrentRate; }

	/** The blended resting pose, before scalars and masks. */
	UFUNCTION(BlueprintPure, Category="Locomotion")
	const FACFProceduralLocomotionPose& GetStatePose() const { return CurrentPose; }

	UFUNCTION(BlueprintPure, Category="Locomotion")
	const FACFProceduralLocomotionPose& GetCycleOutput() const { return CycleOutput; }

	UFUNCTION(BlueprintPure, Category="Locomotion")
	const FACFProceduralLocomotionPose& GetEventOutput() const { return EventOutput; }

#if WITH_EDITOR
	/** Forces a state during play, so a sprint pose can be checked without reaching sprint speed. */
	UFUNCTION(CallInEditor, Category="Locomotion|Preview")
	void PreviewState();

	UFUNCTION(CallInEditor, Category="Locomotion|Preview")
	void PreviewEvent();
#endif

protected:
	//~ Begin UACFProceduralMotionComponent
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs) override;
	//~ End of UACFProceduralMotionComponent

	/** Everything that integrates over time. Runs at most once per frame — see FINDINGS #18. */
	void Advance(float DeltaSeconds, const UACFProceduralPresetLocomotionMotion& Preset);

	void RefreshInputFromOwner();

	void DetectMovementEvents(const UACFProceduralPresetLocomotionMotion& Preset);

	void RefreshState(float DeltaSeconds, const UACFProceduralPresetLocomotionMotion& Preset);

	/** 0 when the state's suppress curve is unset or reads 0, 1 when it fully hands over to the fallback. */
	float GetSuppressAlpha(const FACFProceduralLocomotionState& State) const;

	void RefreshEvents(float DeltaSeconds, const UACFProceduralPresetLocomotionMotion& Preset);

	UFUNCTION()
	void OnRep_ReplicatedEvent();

public:
	/** Per-weapon variation on top of a shared preset. Scale first, override the preset only if that fails. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Locomotion", meta=(ClampMin="0.0"))
	float LocationAmplitudeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Locomotion", meta=(ClampMin="0.0"))
	float RotationAmplitudeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Locomotion", meta=(ClampMin="0.0"))
	float PoseScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Locomotion", meta=(ClampMin="0.0"))
	float CycleRateScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Locomotion", meta=(ClampMin="0.0"))
	float EventScale = 1.f;

	/**
	 * Replaces a state's pose without copying its whole preset.
	 *
	 * Preset data inheritance resolves whole presets, so a weapon needing one different pose would otherwise
	 * have to duplicate the shared state set. This is the way out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Locomotion")
	TMap<FGameplayTag, FACFProceduralLocomotionPose> StatePoseOverrides;

	/** Stage for the resting pose. Defaults ahead of ADS so aiming overrides a sprint pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Procedural Motion")
	int32 PoseStage = 1;

	/** If set, overrides the state tag independent of driver mode or locomotion input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Procedural Motion")
	FGameplayTag OverrideStateTag;

#if WITH_EDITORONLY_DATA
	/** Held state while Preview State is toggled on. Wins over the resolver, whichever driver is set. */
	UPROPERTY(EditAnywhere, Category="Settings")
	FGameplayTag PreviewStateTag;

	UPROPERTY(EditAnywhere, Category="Settings")
	FGameplayTag PreviewEventTag;

	UPROPERTY(VisibleInstanceOnly, Transient, Category="Settings")
	bool bPreviewStateActive = false;
#endif

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Locomotion")
	FACFProceduralLocomotionInput LocomotionInput;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Locomotion")
	FGameplayTag ActiveStateTag;

	/** Set by SetLocomotionState. Only read while the preset's Driver is Manual. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Locomotion")
	FGameplayTag RequestedStateTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Locomotion")
	TArray<FACFProceduralLocomotionEventPlayer> ActiveEvents;

	/** Bumped on every replicated play so an identical tag twice in a row still triggers OnRep. */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedEvent)
	FGameplayTag ReplicatedEventTag;

	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedEvent)
	uint8 ReplicatedEventCounter = 0;

private:
	/** Cycle position in [0,1). */
	float Phase = 0.f;

	/** Keeps growing. Perlin needs a phase that does not repeat, and double keeps it precise for good. */
	double UnwrappedPhase = 0.0;

	/** Absorbs the step when the oscillator set swaps or the phase is reset. */
	FACFProceduralContinuityGuard CycleGuard;

	/** Absorbs the step when an event is restarted or dropped part way through. */
	FACFProceduralContinuityGuard EventGuard;

	/** Set by anything that cuts an event short, and consumed on the next refresh. */
	bool bEventDiscontinuity = false;

	/**
	 * Smoothed toward the active state rather than cross-faded from the previous one.
	 *
	 * There is no "from" state to go stale, so walk to run and straight back is handled the same as any
	 * other change, and speed response varies these continuously anyway.
	 */
	float CurrentRate = 0.f;

	FACFProceduralLocomotionPose CurrentPose;

	FVector CurrentLocationAmplitude = FVector::ZeroVector;

	FRotator CurrentRotationAmplitude = FRotator::ZeroRotator;

	FACFProceduralLocomotionPose CycleOutput;

	FACFProceduralLocomotionPose EventOutput;

	int32 ActiveStateIndex = INDEX_NONE;

	/** Last state that actually resolved, so an unmatched situation fades out instead of cutting off. */
	int32 CycleShapeStateIndex = INDEX_NONE;

	/** Most negative vertical speed seen since leaving the ground, for land impact scaling. */
	float FallSpeed = 0.f;

	bool bWasGrounded = true;

	/** Guards the integration against a dual-mesh character updating this component twice a frame. */
	uint64 LastAdvanceFrame = 0;
};
