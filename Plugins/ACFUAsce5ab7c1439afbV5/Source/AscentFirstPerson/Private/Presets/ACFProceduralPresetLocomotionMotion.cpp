// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Presets/ACFProceduralPresetLocomotionMotion.h"

#include "Curves/CurveVector.h"
#include "Types/ACFFirstPersonTags.h"

namespace ACFProceduralLocomotion
{
	/** Adds a cubic key with automatic tangents. */
	static void AddShapeKey(FRichCurve& Curve, float Time, float Value)
	{
		const FKeyHandle Handle = Curve.AddKey(Time, Value);
		Curve.SetKeyInterpMode(Handle, RCIM_Cubic);
		Curve.SetKeyTangentMode(Handle, RCTM_Auto);
	}

	/** Hit, overshoot, settle. The shape almost every impact wants before it is tuned. */
	static void BuildDefaultImpactShape(FRuntimeFloatCurve& Shape)
	{
		FRichCurve* Curve = Shape.GetRichCurve();
		if (!Curve || Curve->GetNumKeys() > 0)
		{
			return;
		}

		AddShapeKey(*Curve, 0.00f, 0.00f);
		AddShapeKey(*Curve, 0.18f, 1.00f);
		AddShapeKey(*Curve, 0.55f, -0.25f);
		AddShapeKey(*Curve, 1.00f, 0.00f);
	}

	/** Curve montages map a vector curve as X to roll, Y to pitch, Z to yaw. Matched so curves interchange. */
	static FRotator VectorCurveToRotator(const FVector& Value)
	{
		return FRotator(Value.Y, Value.Z, Value.X);
	}

	/** Fraction of a Sawtooth cycle spent returning. Without a return it would step by 2 every cycle. */
	constexpr float SawtoothReturnRatio = 0.2f;

	/** FMath::PerlinNoise1D repeats every 256, so folding the input there is exact, not an approximation. */
	constexpr double PerlinPeriod = 256.0;

	/** Frac that stays in [0,1) for a negative argument, which a negative frequency multiplier produces. */
	static float WrapPhase(float Value)
	{
		return Value - FMath::FloorToFloat(Value);
	}
}

// ---------------------------------------------------------------------------------------------------
// Continuity guard

FACFProceduralLocomotionPose FACFProceduralContinuityGuard::Apply(const FACFProceduralLocomotionPose& Raw, bool bDiscontinuous, float DeltaSeconds, float DecaySpeed)
{
	if (bDiscontinuous && bPrimed)
	{
		// Exactly the offset that lands this frame on last frame's output. Assignment, not accumulation, so
		// the correction can never be larger than the step it is hiding.
		Offset = Previous - Raw;
	}
	else if (DecaySpeed > 0.f)
	{
		Offset.Location = FMath::VInterpTo(Offset.Location, FVector::ZeroVector, DeltaSeconds, DecaySpeed);
		Offset.Rotation = FMath::RInterpTo(Offset.Rotation, FRotator::ZeroRotator, DeltaSeconds, DecaySpeed);
	}
	else
	{
		Offset = FACFProceduralLocomotionPose();
	}

	Previous = Raw + Offset;
	bPrimed = true;

	return Previous;
}

void FACFProceduralContinuityGuard::Reset()
{
	Offset = FACFProceduralLocomotionPose();
	Previous = FACFProceduralLocomotionPose();
	bPrimed = false;
}

// ---------------------------------------------------------------------------------------------------
// Oscillator

float FACFProceduralOscillator::EvaluateShapeAt(float T) const
{
	switch (Shape)
	{
	case EACFProceduralOscillatorShape::Sine:
	{
		return FMath::Sin(UE_TWO_PI * T);
	}

	case EACFProceduralOscillatorShape::Triangle:
	{
		// Same phase as Sine: 0 rising through 1 at a quarter, back through -1 at three quarters.
		return 4.f * FMath::Abs(ACFProceduralLocomotion::WrapPhase(T + 0.75f) - 0.5f) - 1.f;
	}

	case EACFProceduralOscillatorShape::Sawtooth:
	{
		// Ramps up over most of the cycle then returns fast. A true sawtooth steps by 2 at the wrap, which
		// on a weapon transform is a teleport, so the return ramp is not optional.
		constexpr float RampEnd = 1.f - ACFProceduralLocomotion::SawtoothReturnRatio;

		return T < RampEnd
			? -1.f + 2.f * (T / RampEnd)
			: 1.f - 2.f * ((T - RampEnd) / ACFProceduralLocomotion::SawtoothReturnRatio);
	}

	case EACFProceduralOscillatorShape::Pulse:
	{
		// Rises from zero to a peak of 1, then decays. Starting at 1 — the obvious way to write an impact —
		// steps the weapon on the first frame of every cycle.
		const float PeakTime = 1.f / FMath::Max(PulseSharpness, 0.1f);
		const float X = T / PeakTime;

		return X * FMath::Exp(1.f - X);
	}

	case EACFProceduralOscillatorShape::Custom:
	{
		const FRichCurve* Curve = CustomShape.GetRichCurveConst();
		return Curve ? Curve->Eval(T) : 0.f;
	}

	default:
	{
		return 0.f;
	}
	}
}

float FACFProceduralOscillator::Evaluate(float Phase, double UnwrappedPhase) const
{
	if (Shape == EACFProceduralOscillatorShape::Perlin)
	{
		// Unwrapped, or the noise would repeat exactly like the periodic shapes it exists to avoid. Folded at
		// the noise function's own period, so it is seamless for any frequency multiplier and stays precise
		// however long the session runs.
		const double Input = FMath::Fmod(UnwrappedPhase * FrequencyMultiplier + PhaseOffset, ACFProceduralLocomotion::PerlinPeriod);
		return FMath::PerlinNoise1D(static_cast<float>(Input));
	}

	const float T = ACFProceduralLocomotion::WrapPhase(Phase * FrequencyMultiplier + PhaseOffset);

	// Every shape has to end a cycle where it started, or the weapon steps by the difference once per cycle.
	// Subtracting the end-to-end drift guarantees that, and costs nothing on a shape that already closes —
	// which is every built-in one. It is the customer's own curve this is really protecting against.
	const float Drift = EvaluateShapeAt(1.f) - EvaluateShapeAt(0.f);

	return EvaluateShapeAt(T) - T * Drift;
}

// ---------------------------------------------------------------------------------------------------
// Oscillator set

FACFProceduralOscillatorSet::FACFProceduralOscillatorSet()
	// The classic bob figure-eight: vertical twice per stride, lateral once and a quarter cycle ahead.
	: X(2.f, 0.f)
	, Y(1.f, 0.25f)
	, Z(2.f, 0.f)
	, Pitch(2.f, 0.f)
	, Yaw(1.f, 0.25f)
	, Roll(1.f, 0.25f)
{
}

FACFProceduralLocomotionPose FACFProceduralOscillatorSet::Evaluate(float Phase, double UnwrappedPhase, const FVector& LocationAmplitude, const FRotator& RotationAmplitude) const
{
	FACFProceduralLocomotionPose Out;

	Out.Location.X = X.Evaluate(Phase, UnwrappedPhase) * LocationAmplitude.X;
	Out.Location.Y = Y.Evaluate(Phase, UnwrappedPhase) * LocationAmplitude.Y;
	Out.Location.Z = Z.Evaluate(Phase, UnwrappedPhase) * LocationAmplitude.Z;

	Out.Rotation.Pitch = Pitch.Evaluate(Phase, UnwrappedPhase) * RotationAmplitude.Pitch;
	Out.Rotation.Yaw = Yaw.Evaluate(Phase, UnwrappedPhase) * RotationAmplitude.Yaw;
	Out.Rotation.Roll = Roll.Evaluate(Phase, UnwrappedPhase) * RotationAmplitude.Roll;

	return Out;
}

void FACFProceduralOscillatorSet::SetShape(EACFProceduralOscillatorShape InShape)
{
	X.Shape = Y.Shape = Z.Shape = InShape;
	Pitch.Shape = Yaw.Shape = Roll.Shape = InShape;
}

bool FACFProceduralOscillatorSet::MatchesShapeOf(const FACFProceduralOscillatorSet& Other) const
{
	auto AxisMatches = [](const FACFProceduralOscillator& A, const FACFProceduralOscillator& B)
	{
		return A.Shape == B.Shape
			&& FMath::IsNearlyEqual(A.FrequencyMultiplier, B.FrequencyMultiplier)
			&& FMath::IsNearlyEqual(A.PhaseOffset, B.PhaseOffset)
			&& FMath::IsNearlyEqual(A.PulseSharpness, B.PulseSharpness);
	};

	return AxisMatches(X, Other.X)
		&& AxisMatches(Y, Other.Y)
		&& AxisMatches(Z, Other.Z)
		&& AxisMatches(Pitch, Other.Pitch)
		&& AxisMatches(Yaw, Other.Yaw)
		&& AxisMatches(Roll, Other.Roll);
}

// ---------------------------------------------------------------------------------------------------
// State

float FACFProceduralLocomotionState::GetSpeedFactor(float GroundSpeed) const
{
	if (ReferenceSpeed <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	return FMath::Clamp(GroundSpeed / ReferenceSpeed, SpeedScaleClamp.X, SpeedScaleClamp.Y);
}

// ---------------------------------------------------------------------------------------------------
// Event

FACFProceduralLocomotionEvent::FACFProceduralLocomotionEvent()
{
	ACFProceduralLocomotion::BuildDefaultImpactShape(Shape);
}

float FACFProceduralLocomotionEvent::GetLength() const
{
	if (Source == EACFProceduralLocomotionEventSource::Shape)
	{
		return Duration;
	}

	float Length = 0.f;

	if (::IsValid(LocationCurve))
	{
		float Min = 0.f, Max = 0.f;
		LocationCurve->GetTimeRange(Min, Max);
		Length = FMath::Max(Length, Max);
	}

	if (::IsValid(RotationCurve))
	{
		float Min = 0.f, Max = 0.f;
		RotationCurve->GetTimeRange(Min, Max);
		Length = FMath::Max(Length, Max);
	}

	return Length;
}

FACFProceduralLocomotionPose FACFProceduralLocomotionEvent::Evaluate(float Playback) const
{
	const float Length = GetLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return FACFProceduralLocomotionPose();
	}

	FACFProceduralLocomotionPose Out;

	if (Source == EACFProceduralLocomotionEventSource::Shape)
	{
		const FRichCurve* Curve = Shape.GetRichCurveConst();
		const float Scalar = Curve ? Curve->Eval(FMath::Clamp(Playback / Length, 0.f, 1.f)) : 0.f;

		Out.Location = LocationAmplitude * Scalar;
		Out.Rotation = RotationAmplitude * Scalar;
	}
	else
	{
		if (::IsValid(LocationCurve))
		{
			Out.Location = LocationCurve->GetVectorValue(Playback) * LocationCurveScale;
		}

		if (::IsValid(RotationCurve))
		{
			Out.Rotation = ACFProceduralLocomotion::VectorCurveToRotator(RotationCurve->GetVectorValue(Playback) * RotationCurveScale);
		}
	}

	// A curve that does not start or land on zero would step the weapon when it triggers or when it ends, so
	// both ends are ramped regardless of what was authored.
	float Fade = 1.f;

	if (BlendInTime > KINDA_SMALL_NUMBER)
	{
		Fade = FMath::Min(Fade, FMath::Clamp(Playback / BlendInTime, 0.f, 1.f));
	}

	if (BlendOutTime > KINDA_SMALL_NUMBER)
	{
		Fade = FMath::Min(Fade, FMath::Clamp((Length - Playback) / BlendOutTime, 0.f, 1.f));
	}

	return Out * Fade;
}

float FACFProceduralLocomotionEvent::GetImpactScale(float ImpactSpeed) const
{
	if (!bScaleByImpactSpeed)
	{
		return 1.f;
	}

	const float Alpha = FMath::GetRangePct(ImpactSpeedRange, ImpactSpeed);
	return FMath::Lerp(ImpactScaleRange.X, ImpactScaleRange.Y, FMath::Clamp(Alpha, 0.f, 1.f));
}

// ---------------------------------------------------------------------------------------------------
// Preset

UACFProceduralPresetLocomotionMotion::UACFProceduralPresetLocomotionMotion()
{
	JumpEventTag = ACFFirstPersonTags::Locomotion_Event_Jump;
	LandEventTag = ACFFirstPersonTags::Locomotion_Event_Land;

	// These defaults are the out-of-box experience: GetPresetOrDefault falls back to this CDO, so a
	// component dropped on a weapon with no preset data at all runs on exactly what is authored here.

	{
		FACFProceduralLocomotionState& Idle = States.AddDefaulted_GetRef();
		Idle.Tag = ACFFirstPersonTags::Locomotion_State_Idle;
		Idle.SpeedThreshold = 0.f;
		Idle.CycleDurationScale = 4.f;
		Idle.LocationAmplitude = FVector(0.f, 0.1f, 0.2f);
		Idle.RotationAmplitude = FRotator(0.25f, 0.35f, 0.2f);
		Idle.ReferenceSpeed = 0.f;

		// Breathing is one cycle per stride, not one per footfall, and the drift must not repeat.
		Idle.Oscillators.X = FACFProceduralOscillator(1.f, 0.f);
		Idle.Oscillators.Y = FACFProceduralOscillator(1.f, 0.25f);
		Idle.Oscillators.Z = FACFProceduralOscillator(1.f, 0.f);
		Idle.Oscillators.Pitch = FACFProceduralOscillator(1.f, 0.15f);
		Idle.Oscillators.Yaw = FACFProceduralOscillator(0.3f, 0.f);
		Idle.Oscillators.Yaw.Shape = EACFProceduralOscillatorShape::Perlin;
		Idle.Oscillators.Roll = FACFProceduralOscillator(0.25f, 11.f);
		Idle.Oscillators.Roll.Shape = EACFProceduralOscillatorShape::Perlin;
	}

	{
		FACFProceduralLocomotionState& Walk = States.AddDefaulted_GetRef();
		Walk.Tag = ACFFirstPersonTags::Locomotion_State_Walk;
		Walk.SpeedThreshold = 10.f;
		Walk.CycleDurationScale = 1.f;
		Walk.LocationAmplitude = FVector(0.35f, 0.8f, 0.6f);
		Walk.RotationAmplitude = FRotator(0.6f, 0.8f, 1.2f);
		Walk.ReferenceSpeed = 200.f;
	}

	{
		FACFProceduralLocomotionState& Run = States.AddDefaulted_GetRef();
		Run.Tag = ACFFirstPersonTags::Locomotion_State_Run;
		Run.SpeedThreshold = 300.f;
		Run.CycleDurationScale = 0.85f;
		Run.LocationAmplitude = FVector(0.1f, 0.5f, 0.35f);
		Run.RotationAmplitude = FRotator(0.5f, 0.5f, 0.5f);
		Run.ReferenceSpeed = 300.f;
		Run.bScaleRateBySpeed = false;
		Run.bScaleAmplitudeBySpeed = false;
		Run.Pose.Location = FVector(0.f, 0.f, 0.f);
		Run.Pose.Rotation = FRotator(0.f, 0.f, 0.f);
		
		Run.Oscillators.Roll = FACFProceduralOscillator(2.f, 0.25f);
	}

	{
		FACFProceduralLocomotionState& Sprint = States.AddDefaulted_GetRef();
		Sprint.Tag = ACFFirstPersonTags::Locomotion_State_Sprint;
		Sprint.SuppressCurve = TEXT("SuppressSprint");
		Sprint.SpeedThreshold = 550.f;
		Sprint.CycleDurationScale = 0.6f;
		Sprint.LocationAmplitude = FVector(1.5f, 1.f, 0.5f);
		Sprint.RotationAmplitude = FRotator(3.f, 10.f, 0.f);
		Sprint.ReferenceSpeed = 550.f;
		Sprint.bScaleRateBySpeed = false;
		Sprint.bScaleAmplitudeBySpeed = false;
		Sprint.Pose.Location = FVector(5.f, -5.f, -5.f);
		Sprint.Pose.Rotation = FRotator(0.f, -45.f, 0.f);
		Sprint.BlendInSpeed = 3.f;
		
		Sprint.Oscillators.X = FACFProceduralOscillator(1.f, 0.f);
		Sprint.Oscillators.Yaw = FACFProceduralOscillator(0.5f, 0.25f);
		Sprint.Oscillators.Yaw.Shape = EACFProceduralOscillatorShape::Perlin;
	}

	{
		FACFProceduralLocomotionState& InAir = States.AddDefaulted_GetRef();
		InAir.Tag = ACFFirstPersonTags::Locomotion_State_InAir;
		InAir.Ground = EACFProceduralLocomotionGround::Airborne;
		InAir.SpeedThreshold = 0.f;
		InAir.LocationAmplitude = FVector::ZeroVector;
		InAir.RotationAmplitude = FRotator::ZeroRotator;
		InAir.ReferenceSpeed = 0.f;
		InAir.Pose.Location = FVector(0.f, 0.f, 2.f);
		InAir.Pose.Rotation = FRotator(0.f, 0.f, -5.f);
		InAir.BlendInSpeed = 8.f;
	}

	{
		FACFProceduralLocomotionEvent& Jump = Events.AddDefaulted_GetRef();
		Jump.Tag = ACFFirstPersonTags::Locomotion_Event_Jump;
		Jump.LocationAmplitude = FVector(0.f, 0.f, -1.8f);
		Jump.RotationAmplitude = FRotator(5.f, 0.f, 0.f);
		Jump.Duration = 0.45f;
	}

	{
		FACFProceduralLocomotionEvent& Land = Events.AddDefaulted_GetRef();
		Land.Tag = ACFFirstPersonTags::Locomotion_Event_Land;
		Land.LocationAmplitude = FVector(0.f, 0.f, -3.2f);
		Land.RotationAmplitude = FRotator(8.f, 0.f, -3.f);
		Land.Duration = 0.5f;
		Land.bScaleByImpactSpeed = true;
	}
}

int32 UACFProceduralPresetLocomotionMotion::ResolveStateIndex(float GroundSpeed, bool bGrounded, bool bCrouched, int32 CurrentIndex, TFunctionRef<bool(const FACFProceduralLocomotionState&)> IsSuppressed) const
{
	int32 Best = INDEX_NONE;
	float BestThreshold = -1.f;
	int32 BestSpecificity = -1;

	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		const FACFProceduralLocomotionState& State = States[Index];
		if (!State.bEnabled)
		{
			continue;
		}

		// Same effect as bEnabled, driven per frame by an animation instead of by the asset. Skipping the
		// state here rather than clamping afterwards is what makes the fallback the *next state down*
		// rather than a fixed one.
		if (IsSuppressed(State))
		{
			continue;
		}

		if ((State.Ground == EACFProceduralLocomotionGround::Grounded && !bGrounded)
			|| (State.Ground == EACFProceduralLocomotionGround::Airborne && bGrounded))
		{
			continue;
		}

		if ((State.Stance == EACFProceduralLocomotionStance::Standing && bCrouched)
			|| (State.Stance == EACFProceduralLocomotionStance::Crouched && !bCrouched))
		{
			continue;
		}

		// The active state holds on a little below its own threshold. Ranking still uses the real threshold,
		// so lowering the gate cannot reorder anything — it only decides whether this state stays in the run.
		const float EntryThreshold = (Index == CurrentIndex) ? State.SpeedThreshold - StateHysteresis : State.SpeedThreshold;

		if (EntryThreshold > GroundSpeed)
		{
			continue;
		}

		// Threshold first, so a crouch-specific idle can never beat a walk you are fast enough for.
		// Specificity only settles an exact tie, where the narrower filter is the one that was authored
		// for this situation.
		const int32 Specificity = (State.Stance != EACFProceduralLocomotionStance::Any ? 2 : 0)
			+ (State.Ground != EACFProceduralLocomotionGround::Any ? 1 : 0);

		if (State.SpeedThreshold > BestThreshold
			|| (State.SpeedThreshold == BestThreshold && Specificity > BestSpecificity))
		{
			Best = Index;
			BestThreshold = State.SpeedThreshold;
			BestSpecificity = Specificity;
		}
	}

	return Best;
}

int32 UACFProceduralPresetLocomotionMotion::FindStateIndexByTag(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return INDEX_NONE;
	}

	return States.IndexOfByPredicate([&Tag](const FACFProceduralLocomotionState& State)
	{
		return State.Tag.MatchesTagExact(Tag);
	});
}

const FACFProceduralLocomotionEvent* UACFProceduralPresetLocomotionMotion::FindEvent(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return nullptr;
	}

	return Events.FindByPredicate([&Tag](const FACFProceduralLocomotionEvent& Event)
	{
		return Event.Tag.MatchesTagExact(Tag);
	});
}
