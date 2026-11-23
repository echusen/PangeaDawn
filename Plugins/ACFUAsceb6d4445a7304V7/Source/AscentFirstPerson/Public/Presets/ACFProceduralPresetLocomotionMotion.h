// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Presets/ACFProceduralPreset.h"
#include "Curves/CurveFloat.h"
#include "GameplayTagContainer.h"
#include "ACFProceduralPresetLocomotionMotion.generated.h"

class UCurveVector;

UENUM(BlueprintType)
enum class EACFProceduralOscillatorShape : uint8
{
	/** sin(2*pi*p). Zero-mean, the default for everything. */
	Sine,
	/** Linear ping-pong. Same phase as Sine, sharper turnarounds. */
	Triangle,
	/** Slow ramp one way, fast return. Asymmetric, for a charge-and-release feel. */
	Sawtooth,
	/** Rises to a peak then decays. Unipolar, so it biases the axis toward the amplitude's sign. */
	Pulse,
	/** 1D noise on the unwrapped phase. Not periodic — for idle drift that must not repeat. */
	Perlin,
	/** CustomShape evaluated over [0,1]. */
	Custom
};

UENUM(BlueprintType)
enum class EACFProceduralLocomotionStance : uint8
{
	Any,
	Standing,
	Crouched
};

UENUM(BlueprintType)
enum class EACFProceduralLocomotionGround : uint8
{
	Any,
	Grounded,
	Airborne
};

UENUM(BlueprintType)
enum class EACFProceduralLocomotionDriver : uint8
{
	/** The speed ladder picks the state. Nothing to set up. */
	Automatic,
	/** Nothing is selected until SetLocomotionState is called. */
	Manual
};

UENUM(BlueprintType)
enum class EACFProceduralLocomotionEventSource : uint8
{
	/** Amplitude times a normalised shape curve over Duration. */
	Shape,
	/** A UCurveVector pair, as curve montages use. */
	Curves
};

/** A location and rotation pair, kept apart from FTransform so channels can be blended independently. */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralLocomotionPose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion")
	FRotator Rotation = FRotator::ZeroRotator;

	bool IsNearlyZero() const { return Location.IsNearlyZero() && Rotation.IsNearlyZero(); }

	FTransform ToTransform() const { return FTransform(Rotation.Quaternion(), Location); }

	FACFProceduralLocomotionPose operator*(float Scale) const { return { Location * Scale, Rotation * Scale }; }

	FACFProceduralLocomotionPose operator+(const FACFProceduralLocomotionPose& Other) const { return { Location + Other.Location, Rotation + Other.Rotation }; }

	FACFProceduralLocomotionPose operator-(const FACFProceduralLocomotionPose& Other) const { return { Location - Other.Location, Rotation - Other.Rotation }; }

	FACFProceduralLocomotionPose& operator+=(const FACFProceduralLocomotionPose& Other)
	{
		Location += Other.Location;
		Rotation += Other.Rotation;
		return *this;
	}
};

/**
 * Keeps an output continuous across a change that would otherwise step.
 *
 * On a discontinuity it captures whatever offset leaves this frame exactly where the last one ended, then
 * decays that offset away. Guarantees C0 continuity across anything — a shape swap, a phase reset, an
 * event being cut short — with no outgoing state to keep and no staleness to manage.
 */
struct FACFProceduralContinuityGuard
{
	/** Raw plus the correction. Pass bDiscontinuous on any frame the raw signal could have jumped. */
	FACFProceduralLocomotionPose Apply(const FACFProceduralLocomotionPose& Raw, bool bDiscontinuous, float DeltaSeconds, float DecaySpeed);

	void Reset();

	/** Size of the correction still being absorbed. Zero when nothing recently stepped. */
	float GetCorrectionSize() const { return Offset.Location.Size() + Offset.Rotation.Vector().Size(); }

private:
	FACFProceduralLocomotionPose Offset;

	FACFProceduralLocomotionPose Previous;

	/** Nothing to be continuous with on the first frame. */
	bool bPrimed = false;
};

/** One axis of a cycle. Amplitude lives on the state, so these defaults rarely need touching. */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralOscillator
{
	GENERATED_BODY()

	FACFProceduralOscillator() = default;
	FACFProceduralOscillator(float InFrequencyMultiplier, float InPhaseOffset)
		: FrequencyMultiplier(InFrequencyMultiplier)
		, PhaseOffset(InPhaseOffset)
	{
	}

	/**
	 * Result in [-1,1], except Pulse which is unipolar.
	 *
	 * Guaranteed to end each cycle where it started: a shape that does not close would step the weapon by
	 * the difference once per cycle, which reads as a teleport.
	 */
	float Evaluate(float Phase, double UnwrappedPhase) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	EACFProceduralOscillatorShape Shape = EACFProceduralOscillatorShape::Sine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator", meta=(EditCondition="Shape==EACFProceduralOscillatorShape::Custom", EditConditionHides))
	FRuntimeFloatCurve CustomShape;

	/** How much of the decay happens within one cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator", meta=(EditCondition="Shape==EACFProceduralOscillatorShape::Pulse", EditConditionHides, ClampMin="0.1"))
	float PulseSharpness = 6.f;

	/** Cycles per state cycle. 2 is one per footfall, 1 is one per stride. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	float FrequencyMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PhaseOffset = 0.f;

private:
	/** The shape itself over one cycle, before the end-to-end drift is removed. */
	float EvaluateShapeAt(float T) const;
};

/**
 * The six axes of a cycle.
 *
 * Defaults are the classic weapon-bob figure-eight: vertical at twice the lateral rate, lateral a quarter
 * cycle ahead. Tune amplitude on the state first; come here only when the shape itself is wrong.
 */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralOscillatorSet
{
	GENERATED_BODY()

	FACFProceduralOscillatorSet();

	/** Applies the amplitudes and returns the composed offset for this phase. */
	FACFProceduralLocomotionPose Evaluate(float Phase, double UnwrappedPhase, const FVector& LocationAmplitude, const FRotator& RotationAmplitude) const;

	/** Sets every axis to the same shape, keeping frequencies and phases. */
	void SetShape(EACFProceduralOscillatorShape Shape);

	/**
	 * Whether both sets produce the same value at the same phase.
	 *
	 * Amplitude is not part of it — that interpolates. This is only asking whether swapping between the two
	 * would step the output, which most state changes do not: the shipped walk, run and sprint all share one
	 * set and differ only in amplitude and rate.
	 */
	bool MatchesShapeOf(const FACFProceduralOscillatorSet& Other) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	FACFProceduralOscillator X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	FACFProceduralOscillator Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	FACFProceduralOscillator Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	FACFProceduralOscillator Pitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	FACFProceduralOscillator Yaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oscillator")
	FACFProceduralOscillator Roll;
};

/**
 * One movement state: the pose the weapon settles into, and the cycle running on top of it.
 *
 * Pose is a delta on top of the Offset component's base pose, never an absolute placement, so a weapon's
 * grip is only ever tuned once. See Docs/LOCOMOTION-MOTION.md § 14.
 */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralLocomotionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State")
	bool bEnabled = true;

	/** Lowest ground speed that enters this state. The highest threshold at or below the current speed wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Selection", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float SpeedThreshold = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Selection")
	EACFProceduralLocomotionGround Ground = EACFProceduralLocomotionGround::Grounded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Selection")
	EACFProceduralLocomotionStance Stance = EACFProceduralLocomotionStance::Any;

	/**
	 * Anim curve that hands this state over to the next one down the ladder. Unset never suppresses.
	 *
	 * An alpha, not a switch: 0 is normal, 1 behaves exactly as if the state were disabled, and anything
	 * between mixes this state's pose, amplitude and rate with the fallback's. Suppressing Sprint during a
	 * reload therefore leaves a sprinting character in Run and an idle one in Idle, with no rule anywhere
	 * that knows what a reload is. Authored on the weapon's own montage, so it is per weapon for free.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Selection")
	FName SuppressCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Pose")
	FACFProceduralLocomotionPose Pose;

	/** Multiplies the preset's base cycle duration. Sprint 0.5, run 0.65, walk 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Cycle", meta=(ClampMin="0.0"))
	float CycleDurationScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Cycle")
	FVector LocationAmplitude = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Cycle")
	FRotator RotationAmplitude = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Cycle", AdvancedDisplay)
	FACFProceduralOscillatorSet Oscillators;

	/** Speed the amplitudes above were authored against. 0 disables both scalings below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Speed Response", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float ReferenceSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Speed Response")
	bool bScaleRateBySpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Speed Response")
	bool bScaleAmplitudeBySpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Speed Response")
	FVector2D SpeedScaleClamp = FVector2D(0.25f, 2.f);

	/** 0 falls back to the preset's StateBlendSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Blending", meta=(ClampMin="0.0"))
	float BlendInSpeed = 0.f;

	/** Restart the cycle from the top when this state is entered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="State|Blending")
	bool bResetPhaseOnEnter = false;

	/** Speed factor for this state, clamped. 1 when speed response is off. */
	float GetSpeedFactor(float GroundSpeed) const;
};

/** A one-shot triggered by an event rather than a condition — jump, land, or anything the game plays. */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFProceduralLocomotionEvent
{
	GENERATED_BODY()

	FACFProceduralLocomotionEvent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event")
	EACFProceduralLocomotionEventSource Source = EACFProceduralLocomotionEventSource::Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Shape", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Shape", EditConditionHides))
	FVector LocationAmplitude = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Shape", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Shape", EditConditionHides))
	FRotator RotationAmplitude = FRotator::ZeroRotator;

	/** Normalised time to scalar. Defaults to a hit, an overshoot and a settle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Shape", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Shape", EditConditionHides))
	FRuntimeFloatCurve Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Shape", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Shape", EditConditionHides, ClampMin="0.01", ForceUnits="s"))
	float Duration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Curves", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Curves", EditConditionHides))
	TObjectPtr<UCurveVector> LocationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Curves", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Curves", EditConditionHides))
	TObjectPtr<UCurveVector> RotationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Curves", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Curves", EditConditionHides))
	FVector LocationCurveScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Curves", meta=(EditCondition="Source==EACFProceduralLocomotionEventSource::Curves", EditConditionHides))
	FVector RotationCurveScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Playback", meta=(ClampMin="0.01"))
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Playback", meta=(ClampMin="0.0", ForceUnits="s"))
	float Delay = 0.f;

	/** Ramps the head in, so a curve that does not start at zero cannot snap the weapon when it triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Playback", meta=(ClampMin="0.0", ForceUnits="s"))
	float BlendInTime = 0.05f;

	/** Fades the tail out over the last stretch of playback, so a curve that does not end at zero still can. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Playback", meta=(ClampMin="0.0", ForceUnits="s"))
	float BlendOutTime = 0.1f;

	/** When false, re-triggering while it plays is ignored instead of restarting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Playback")
	bool bRestartIfPlaying = true;

	/** Scale the whole event by how hard the character hit the ground. Land wants this; jump does not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Impact")
	bool bScaleByImpactSpeed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Impact", meta=(EditCondition="bScaleByImpactSpeed", EditConditionHides, ForceUnits="cm/s"))
	FVector2D ImpactSpeedRange = FVector2D(200.f, 1200.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event|Impact", meta=(EditCondition="bScaleByImpactSpeed", EditConditionHides))
	FVector2D ImpactScaleRange = FVector2D(0.3f, 1.5f);

	/** Playback length in seconds, before PlayRate. */
	float GetLength() const;

	/** Offset at this playback time, before the caller's own scaling. */
	FACFProceduralLocomotionPose Evaluate(float Playback) const;

	/** Maps an impact speed onto ImpactScaleRange, or 1 when impact scaling is off. */
	float GetImpactScale(float ImpactSpeed) const;
};

/**
 * Movement cycles and one-shots for one equipment or character.
 *
 * The shape lives here and is meant to be shared through UACFProceduralPresetData::Parents; per-weapon
 * variation belongs on the component as scalars. See Docs/LOCOMOTION-MOTION.md § 7.
 */
UCLASS(meta=(DisplayName="Preset: Locomotion Motion"))
class ASCENTFIRSTPERSON_API UACFProceduralPresetLocomotionMotion : public UACFProceduralPreset
{
	GENERATED_BODY()

public:
	UACFProceduralPresetLocomotionMotion();

	/**
	 * First enabled state matching the inputs, highest threshold winning and stance breaking ties.
	 *
	 * CurrentIndex keeps its state selectable until the speed falls StateHysteresis below its threshold.
	 * Without that, velocity noise while walking at exactly the run threshold flips between the two every
	 * frame.
	 */
	int32 ResolveStateIndex(float GroundSpeed, bool bGrounded, bool bCrouched, int32 CurrentIndex, TFunctionRef<bool(const FACFProceduralLocomotionState&)> IsSuppressed) const;

	int32 FindStateIndexByTag(const FGameplayTag& Tag) const;

	const FACFProceduralLocomotionEvent* FindEvent(const FGameplayTag& Tag) const;

public:
	/** Seconds for one full stride — both feet — at CycleDurationScale 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion", meta=(ClampMin="0.01", ForceUnits="s"))
	float BaseCycleDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion")
	TArray<FACFProceduralLocomotionState> States;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion")
	TArray<FACFProceduralLocomotionEvent> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Blending", meta=(ClampMin="0.0"))
	float StateBlendSpeed = 10.f;

	/** How far below its threshold the speed must fall before the active state gives way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Blending", meta=(ClampMin="0.0", ForceUnits="cm/s"))
	float StateHysteresis = 15.f;

	/**
	 * How fast a continuity correction is absorbed.
	 *
	 * Some changes cannot be interpolated — swapping to a state whose oscillators run at a different
	 * frequency, resetting the phase, cutting an event short. The correction that hides the step decays at
	 * this rate. Too low and the motion lags after a change; too high and the step comes back.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Blending", meta=(ClampMin="0.0"))
	float ContinuityDecaySpeed = 12.f;

	/** Cycle amplitude multiplier while fully aimed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Aiming", meta=(ClampMin="0.0"))
	float AimingScale = 0.35f;

	/** Pose multiplier while fully aimed. ADS normally owns the pose, so this is usually low. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Aiming", meta=(ClampMin="0.0"))
	float AimingPoseScale = 0.2f;

	/** Masks the pose, cycle and event output together. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Masks", meta=(GetOptions="GetCurveNameOptions"))
	FName MaskLocomotionCurve = TEXT("MaskLocomotion");

	/** Extra mask on the cycle only. Unset by default, so it does nothing until it is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Masks", meta=(GetOptions="GetCurveNameOptions"))
	FName MaskLocomotionCycleCurve = NAME_None;

	/** Automatic runs the speed ladder; Manual waits for SetLocomotionState. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Input")
	EACFProceduralLocomotionDriver Driver = EACFProceduralLocomotionDriver::Automatic;

	/** Read speed, ground and stance off the animated character. Off means the game calls SetLocomotionInput. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Input")
	bool bAutoReadFromOwner = true;

	/** Fire the jump and land tags below from the character's own ground transitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Input")
	bool bAutoDetectEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Input", meta=(EditCondition="bAutoDetectEvents", EditConditionHides))
	FGameplayTag JumpEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Input", meta=(EditCondition="bAutoDetectEvents", EditConditionHides))
	FGameplayTag LandEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locomotion|Events", meta=(ClampMin="1", ClampMax="16"))
	int32 MaxConcurrentEvents = 4;
};
