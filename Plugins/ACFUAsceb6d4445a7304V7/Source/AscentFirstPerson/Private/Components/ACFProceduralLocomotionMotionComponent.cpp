// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralLocomotionMotionComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

#include "Presets/ACFProceduralPreset.h"

namespace ACFProceduralLocomotion
{
	/** Upward speed that separates a jump from walking off a ledge. */
	constexpr float JumpDetectSpeed = 10.f;

	/** Past this much suppression the fallback state owns the cycle shape, the tag and the debug readout. */
	constexpr float SuppressShapeThreshold = 0.5f;

	/** Everything a state contributes, so a partly suppressed state can be mixed with its fallback. */
	struct FStateTargets
	{
		FACFProceduralLocomotionPose Pose;
		FVector LocationAmplitude = FVector::ZeroVector;
		FRotator RotationAmplitude = FRotator::ZeroRotator;
		float Rate = 0.f;
		float BlendSpeed = 0.f;
	};

	/** Nothing matching leaves the rate alone, so the clock keeps running and the cycle fades instead of cutting. */
	static FStateTargets BuildStateTargets(
		const UACFProceduralPresetLocomotionMotion& Preset,
		int32 Index,
		float GroundSpeed,
		const TMap<FGameplayTag, FACFProceduralLocomotionPose>& PoseOverrides,
		float FallbackRate)
	{
		FStateTargets Targets;
		Targets.Rate = FallbackRate;
		Targets.BlendSpeed = Preset.StateBlendSpeed;

		if (!Preset.States.IsValidIndex(Index))
		{
			return Targets;
		}

		const FACFProceduralLocomotionState& State = Preset.States[Index];
		const float SpeedFactor = State.GetSpeedFactor(GroundSpeed);

		const FACFProceduralLocomotionPose* Override = PoseOverrides.Find(State.Tag);
		Targets.Pose = Override ? *Override : State.Pose;

		Targets.LocationAmplitude = State.LocationAmplitude * (State.bScaleAmplitudeBySpeed ? SpeedFactor : 1.f);
		Targets.RotationAmplitude = State.RotationAmplitude * (State.bScaleAmplitudeBySpeed ? SpeedFactor : 1.f);

		const float CycleDuration = Preset.BaseCycleDuration * State.CycleDurationScale;
		Targets.Rate = CycleDuration > KINDA_SMALL_NUMBER ? 1.f / CycleDuration : 0.f;

		if (State.bScaleRateBySpeed)
		{
			Targets.Rate *= SpeedFactor;
		}

		if (State.BlendInSpeed > 0.f)
		{
			Targets.BlendSpeed = State.BlendInSpeed;
		}

		return Targets;
	}

	static FStateTargets BlendStateTargets(const FStateTargets& From, const FStateTargets& To, float Alpha)
	{
		FStateTargets Result;
		Result.Pose.Location = FMath::Lerp(From.Pose.Location, To.Pose.Location, Alpha);
		Result.Pose.Rotation = FMath::Lerp(From.Pose.Rotation, To.Pose.Rotation, Alpha);
		Result.LocationAmplitude = FMath::Lerp(From.LocationAmplitude, To.LocationAmplitude, Alpha);
		Result.RotationAmplitude = FMath::Lerp(From.RotationAmplitude, To.RotationAmplitude, Alpha);
		Result.Rate = FMath::Lerp(From.Rate, To.Rate, Alpha);
		Result.BlendSpeed = FMath::Lerp(From.BlendSpeed, To.BlendSpeed, Alpha);

		return Result;
	}
}

UACFProceduralLocomotionMotionComponent::UACFProceduralLocomotionMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stage = 3;
}

TSubclassOf<UACFProceduralPreset> UACFProceduralLocomotionMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetLocomotionMotion::StaticClass();
}

void UACFProceduralLocomotionMotionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SkipOwnerParameters;
	SkipOwnerParameters.bIsPushBased = true;
	SkipOwnerParameters.Condition = COND_SkipOwner;

	DOREPLIFETIME_WITH_PARAMS_FAST(UACFProceduralLocomotionMotionComponent, ReplicatedEventTag, SkipOwnerParameters);
	DOREPLIFETIME_WITH_PARAMS_FAST(UACFProceduralLocomotionMotionComponent, ReplicatedEventCounter, SkipOwnerParameters);
}

void UACFProceduralLocomotionMotionComponent::ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs)
{
	const UACFProceduralPresetLocomotionMotion* Preset = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetLocomotionMotion>(GetEffectivePresetData());
	if (!Preset)
	{
		return;
	}

	// PreUpdate runs once per anim node, and a dual-mesh character has two of them naming the same weapon.
	// Integrate on the first call of the frame; the second reads what it produced. See FINDINGS #18.
	if (LastAdvanceFrame != GFrameCounter)
	{
		LastAdvanceFrame = GFrameCounter;
		Advance(DeltaSeconds, *Preset);
	}

	const float MaskAlpha = 1.f - FMath::Clamp(ReadCurve(AnimInstance, Preset->MaskLocomotionCurve), 0.f, 1.f);
	if (MaskAlpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Scalars are baked into the transform rather than folded into Weight: the node clamps Weight to [0,1],
	// so a scale above 1 would silently do nothing there.

	{
		const float PoseAlpha = PoseScale * FMath::Lerp(1.f, Preset->AimingPoseScale, Input.AimingState);

		FACFProceduralMotionOutput& Out = Outs.AddDefaulted_GetRef();
		Out.Stage = PoseStage;
		Out.Composition = EACFProceduralMotionComposition::Additive;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = (CurrentPose * PoseAlpha).ToTransform();
		Out.Weight = MaskAlpha;
	}

	{
		const float CycleMask = MaskAlpha * (1.f - FMath::Clamp(ReadCurve(AnimInstance, Preset->MaskLocomotionCycleCurve), 0.f, 1.f));
		const float AimAlpha = FMath::Lerp(1.f, Preset->AimingScale, Input.AimingState);

		FACFProceduralLocomotionPose Cycle;
		Cycle.Location = CycleOutput.Location * (LocationAmplitudeScale * AimAlpha);
		Cycle.Rotation = CycleOutput.Rotation * (RotationAmplitudeScale * AimAlpha);

		FACFProceduralMotionOutput& Out = Outs.AddDefaulted_GetRef();
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::Additive;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = Cycle.ToTransform();
		Out.Weight = CycleMask;
	}

	{
		FACFProceduralMotionOutput& Out = Outs.AddDefaulted_GetRef();
		Out.Stage = Stage;
		Out.Composition = EACFProceduralMotionComposition::Additive;
		Out.Space = EACFProceduralMotionOutputSpace::Component;
		Out.Transform = (EventOutput * EventScale).ToTransform();
		Out.Weight = MaskAlpha;
	}
}

void UACFProceduralLocomotionMotionComponent::Advance(float DeltaSeconds, const UACFProceduralPresetLocomotionMotion& Preset)
{
	if (Preset.bAutoReadFromOwner)
	{
		RefreshInputFromOwner();
	}

	DetectMovementEvents(Preset);
	RefreshState(DeltaSeconds, Preset);
	RefreshEvents(DeltaSeconds, Preset);
}

void UACFProceduralLocomotionMotionComponent::RefreshInputFromOwner()
{
	// Not the component's owner: a component on a weapon has to read the character driving the animation.
	const ACharacter* Character = AnimOwnerCharacter;
	if (!Character)
	{
		return;
	}

	const FVector Velocity = Character->GetVelocity();

	LocomotionInput.GroundSpeed = Velocity.Size2D();
	LocomotionInput.VerticalSpeed = Velocity.Z;
	LocomotionInput.bCrouched = Character->bIsCrouched;

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	LocomotionInput.bGrounded = Movement ? Movement->IsMovingOnGround() : true;
}

void UACFProceduralLocomotionMotionComponent::DetectMovementEvents(const UACFProceduralPresetLocomotionMotion& Preset)
{
	if (!Preset.bAutoDetectEvents)
	{
		// Still tracked, or switching detection on mid-play would fire off a stale edge.
		bWasGrounded = LocomotionInput.bGrounded;
		FallSpeed = 0.f;
		return;
	}

	if (LocomotionInput.bGrounded)
	{
		if (!bWasGrounded)
		{
			const FACFProceduralLocomotionEvent* Land = Preset.FindEvent(Preset.LandEventTag);
			PlayLocomotionEvent(Preset.LandEventTag, Land ? Land->GetImpactScale(FMath::Abs(FallSpeed)) : 1.f);
		}

		FallSpeed = 0.f;
	}
	else
	{
		if (bWasGrounded && LocomotionInput.VerticalSpeed > ACFProceduralLocomotion::JumpDetectSpeed)
		{
			PlayLocomotionEvent(Preset.JumpEventTag, 1.f);
		}

		// The hardest part of the fall, not the speed at the instant of contact, which a step-up can soften.
		FallSpeed = FMath::Min(FallSpeed, LocomotionInput.VerticalSpeed);
	}

	bWasGrounded = LocomotionInput.bGrounded;
}

float UACFProceduralLocomotionMotionComponent::GetSuppressAlpha(const FACFProceduralLocomotionState& State) const
{
	if (State.SuppressCurve.IsNone())
	{
		return 0.f;
	}

	return FMath::Clamp(ReadCurve(CachedAnimInstance, State.SuppressCurve), 0.f, 1.f);
}

void UACFProceduralLocomotionMotionComponent::RefreshState(float DeltaSeconds, const UACFProceduralPresetLocomotionMotion& Preset)
{
	int32 TargetIndex = INDEX_NONE;

	// An explicit request is honoured whatever the curves say. Asking for a state and silently getting a
	// different one would be worse than the problem suppression solves.
	bool bExplicitRequest = true;

#if WITH_EDITORONLY_DATA
	if (bPreviewStateActive && PreviewStateTag.IsValid())
	{
		TargetIndex = Preset.FindStateIndexByTag(PreviewStateTag);
	}
	else
#endif
	if (OverrideStateTag.IsValid())
	{
		TargetIndex = Preset.FindStateIndexByTag(OverrideStateTag);
	}
	else if (Preset.Driver == EACFProceduralLocomotionDriver::Manual)
	{
		TargetIndex = Preset.FindStateIndexByTag(RequestedStateTag);
	}
	else
	{
		bExplicitRequest = false;
	}

	// How far the chosen state is being suppressed, and where selection lands once it is out of the way.
	float SuppressAlpha = 0.f;
	int32 FallbackIndex = INDEX_NONE;

	if (!bExplicitRequest)
	{
		// Resolve as though nothing were suppressed, so a partly suppressed state is still the one being
		// mixed *from*. Fully suppressing it then lands exactly where the boolean version did.
		TargetIndex = Preset.ResolveStateIndex(
			LocomotionInput.GroundSpeed,
			LocomotionInput.bGrounded,
			LocomotionInput.bCrouched,
			ActiveStateIndex,
			[](const FACFProceduralLocomotionState&) { return false; });

		SuppressAlpha = Preset.States.IsValidIndex(TargetIndex) ? GetSuppressAlpha(Preset.States[TargetIndex]) : 0.f;

		if (SuppressAlpha > 0.f)
		{
			// Any suppression at all takes a state out of the running for being someone else's fallback,
			// so chained suppression walks down the ladder rather than landing on a half-off state.
			FallbackIndex = Preset.ResolveStateIndex(
				LocomotionInput.GroundSpeed,
				LocomotionInput.bGrounded,
				LocomotionInput.bCrouched,
				ActiveStateIndex,
				[this](const FACFProceduralLocomotionState& State) { return GetSuppressAlpha(State) > 0.f; });
		}
	}

	// Amplitude, pose and rate all interpolate, but the oscillator set itself cannot: a state whose axes run
	// at a different frequency or shape produces a completely different value at the same phase. Idle at one
	// cycle per stride against run at two is a full-amplitude step, which is what braking hard used to feel
	// like. The guard below absorbs it.
	bool bCycleDiscontinuity = false;
	const int32 PreviousShapeIndex = CycleShapeStateIndex;

	// Past halfway the fallback owns the identity of the state — its shape, its tag, its debug row. The
	// shapes cannot be interpolated (see above), so one of the two has to own them outright.
	const bool bFallbackDominates = SuppressAlpha >= ACFProceduralLocomotion::SuppressShapeThreshold
		&& Preset.States.IsValidIndex(FallbackIndex);

	const int32 DominantIndex = bFallbackDominates ? FallbackIndex : TargetIndex;

	if (DominantIndex != ActiveStateIndex)
	{
		ActiveStateIndex = DominantIndex;
		ActiveStateTag = Preset.States.IsValidIndex(DominantIndex) ? Preset.States[DominantIndex].Tag : FGameplayTag();

		if (Preset.States.IsValidIndex(DominantIndex) && Preset.States[DominantIndex].bResetPhaseOnEnter)
		{
			Phase = 0.f;
			UnwrappedPhase = 0.0;
			bCycleDiscontinuity = true;
		}
	}

	// Held separately so that when nothing matches, the amplitudes fade out through the shape that was
	// running rather than the cycle cutting off mid-stride. Deleting the in-air state and jumping is the
	// case that makes this visible.
	if (Preset.States.IsValidIndex(ActiveStateIndex))
	{
		CycleShapeStateIndex = ActiveStateIndex;
	}

	// Nothing matching means nothing was authored for this situation, so ease everything out rather than
	// snapping to zero — but keep the clock running, so re-entering a state does not restart mid-stride.
	ACFProceduralLocomotion::FStateTargets Targets = ACFProceduralLocomotion::BuildStateTargets(
		Preset, TargetIndex, LocomotionInput.GroundSpeed, StatePoseOverrides, CurrentRate);

	// Partial suppression mixes the state with where selection would land without it, so "half suppressed"
	// means halfway to the fallback rather than half of nothing. At 1 this is exactly the fallback, which is
	// what the earlier boolean form did.
	if (SuppressAlpha > 0.f)
	{
		const ACFProceduralLocomotion::FStateTargets FallbackTargets = ACFProceduralLocomotion::BuildStateTargets(
			Preset, FallbackIndex, LocomotionInput.GroundSpeed, StatePoseOverrides, CurrentRate);

		Targets = ACFProceduralLocomotion::BlendStateTargets(Targets, FallbackTargets, SuppressAlpha);
	}

	const FACFProceduralLocomotionPose TargetPose = Targets.Pose;
	const FVector TargetLocationAmplitude = Targets.LocationAmplitude;
	const FRotator TargetRotationAmplitude = Targets.RotationAmplitude;
	const float TargetRate = Targets.Rate;
	const float BlendSpeed = Targets.BlendSpeed;

	// Parameters are smoothed toward the active state, never cross-faded from the previous one. There is no
	// "from" state to go stale, so walk to run and straight back needs no special handling, and the speed
	// response already varies these continuously.
	if (BlendSpeed > 0.f)
	{
		CurrentPose.Location = FMath::VInterpTo(CurrentPose.Location, TargetPose.Location, DeltaSeconds, BlendSpeed);
		CurrentPose.Rotation = FMath::RInterpTo(CurrentPose.Rotation, TargetPose.Rotation, DeltaSeconds, BlendSpeed);
		CurrentLocationAmplitude = FMath::VInterpTo(CurrentLocationAmplitude, TargetLocationAmplitude, DeltaSeconds, BlendSpeed);
		CurrentRotationAmplitude = FMath::RInterpTo(CurrentRotationAmplitude, TargetRotationAmplitude, DeltaSeconds, BlendSpeed);
		CurrentRate = FMath::FInterpTo(CurrentRate, TargetRate, DeltaSeconds, BlendSpeed);
	}
	else
	{
		CurrentPose = TargetPose;
		CurrentLocationAmplitude = TargetLocationAmplitude;
		CurrentRotationAmplitude = TargetRotationAmplitude;
		CurrentRate = TargetRate;
	}

	const float PhaseDelta = DeltaSeconds * CurrentRate * CycleRateScale;

	Phase += PhaseDelta;
	Phase -= FMath::FloorToFloat(Phase);

	UnwrappedPhase += PhaseDelta;

	// Only when the sets genuinely differ. Most state changes vary amplitude and rate alone — the shipped
	// walk, run and sprint share one set — and correcting those would be worse than useless: a capture holds
	// the output at last frame's value, so capturing every frame would freeze the cycle outright.
	if (CycleShapeStateIndex != PreviousShapeIndex
		&& Preset.States.IsValidIndex(CycleShapeStateIndex)
		&& Preset.States.IsValidIndex(PreviousShapeIndex))
	{
		bCycleDiscontinuity |= !Preset.States[CycleShapeStateIndex].Oscillators.MatchesShapeOf(Preset.States[PreviousShapeIndex].Oscillators);
	}

	const FACFProceduralLocomotionPose RawCycle = Preset.States.IsValidIndex(CycleShapeStateIndex)
		? Preset.States[CycleShapeStateIndex].Oscillators.Evaluate(Phase, UnwrappedPhase, CurrentLocationAmplitude, CurrentRotationAmplitude)
		: FACFProceduralLocomotionPose();

	CycleOutput = CycleGuard.Apply(RawCycle, bCycleDiscontinuity, DeltaSeconds, Preset.ContinuityDecaySpeed);
}

void UACFProceduralLocomotionMotionComponent::RefreshEvents(float DeltaSeconds, const UACFProceduralPresetLocomotionMotion& Preset)
{
	FACFProceduralLocomotionPose Sum;

	for (int32 Index = ActiveEvents.Num() - 1; Index >= 0; --Index)
	{
		FACFProceduralLocomotionEventPlayer& Player = ActiveEvents[Index];

		// By tag, not by index: the preset can be edited while play-in-editor is running.
		const FACFProceduralLocomotionEvent* Event = Preset.FindEvent(Player.Tag);
		if (!Event)
		{
			// Removed from the preset mid-playback, so its contribution vanishes with no blend out.
			ActiveEvents.RemoveAt(Index);
			bEventDiscontinuity = true;
			continue;
		}

		if (Player.DelayRemaining > 0.f)
		{
			Player.DelayRemaining -= DeltaSeconds;
			continue;
		}

		const float Length = Event->GetLength();
		if (Length <= KINDA_SMALL_NUMBER || Player.Playback >= Length)
		{
			// BlendOutTime normally has this at zero already, in which case the guard sees no step and does
			// nothing. It is only doing work when someone set the blend out to zero.
			ActiveEvents.RemoveAt(Index);
			bEventDiscontinuity = true;
			continue;
		}

		Sum += Event->Evaluate(Player.Playback) * Player.Scale;

		Player.Playback += DeltaSeconds * Event->PlayRate;
	}

	EventOutput = EventGuard.Apply(Sum, bEventDiscontinuity, DeltaSeconds, Preset.ContinuityDecaySpeed);
	bEventDiscontinuity = false;
}

void UACFProceduralLocomotionMotionComponent::PlayLocomotionEvent(FGameplayTag EventTag, float ScaleMultiplier)
{
	const UACFProceduralPresetLocomotionMotion* Preset = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetLocomotionMotion>(GetEffectivePresetData());
	if (!Preset)
	{
		return;
	}

	const FACFProceduralLocomotionEvent* Event = Preset->FindEvent(EventTag);
	if (!Event)
	{
		return;
	}

	for (FACFProceduralLocomotionEventPlayer& Existing : ActiveEvents)
	{
		if (!Existing.Tag.MatchesTagExact(EventTag))
		{
			continue;
		}

		if (Event->bRestartIfPlaying)
		{
			// Jumping the cursor back to zero drops whatever it was outputting mid-curve.
			Existing.Playback = 0.f;
			Existing.DelayRemaining = Event->Delay;
			Existing.Scale = ScaleMultiplier;
			bEventDiscontinuity = true;
		}

		return;
	}

	if (ActiveEvents.Num() >= FMath::Max(1, Preset->MaxConcurrentEvents))
	{
		// Drop the most advanced one. Whatever just happened matters more than something already fading.
		int32 Oldest = 0;
		for (int32 Index = 1; Index < ActiveEvents.Num(); ++Index)
		{
			if (ActiveEvents[Index].Playback > ActiveEvents[Oldest].Playback)
			{
				Oldest = Index;
			}
		}

		ActiveEvents.RemoveAt(Oldest);
		bEventDiscontinuity = true;
	}

	FACFProceduralLocomotionEventPlayer& Player = ActiveEvents.AddDefaulted_GetRef();
	Player.Tag = EventTag;
	Player.DelayRemaining = Event->Delay;
	Player.Scale = ScaleMultiplier;
}

void UACFProceduralLocomotionMotionComponent::PlayLocomotionEventReplicated(FGameplayTag EventTag)
{
	PlayLocomotionEvent(EventTag, 1.f);

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerPlayLocomotionEvent(EventTag);
		return;
	}

	ReplicatedEventTag = EventTag;

	// The tag alone would not replicate the same event twice in a row, and two identical landings back to
	// back is the normal case rather than the exception.
	++ReplicatedEventCounter;

	MARK_PROPERTY_DIRTY_FROM_NAME(UACFProceduralLocomotionMotionComponent, ReplicatedEventTag, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UACFProceduralLocomotionMotionComponent, ReplicatedEventCounter, this);
}

void UACFProceduralLocomotionMotionComponent::ServerPlayLocomotionEvent_Implementation(FGameplayTag EventTag)
{
	PlayLocomotionEventReplicated(EventTag);
}

void UACFProceduralLocomotionMotionComponent::OnRep_ReplicatedEvent()
{
	PlayLocomotionEvent(ReplicatedEventTag, 1.f);
}

void UACFProceduralLocomotionMotionComponent::StopLocomotionEvent(FGameplayTag EventTag)
{
	const int32 Removed = ActiveEvents.RemoveAll([&EventTag](const FACFProceduralLocomotionEventPlayer& Player)
	{
		return Player.Tag.MatchesTagExact(EventTag);
	});

	bEventDiscontinuity |= Removed > 0;
}

void UACFProceduralLocomotionMotionComponent::StopAllLocomotionEvents()
{
	bEventDiscontinuity |= ActiveEvents.Num() > 0;
	ActiveEvents.Reset();
}

void UACFProceduralLocomotionMotionComponent::SetLocomotionState(FGameplayTag StateTag)
{
	RequestedStateTag = StateTag;
}

#if ACF_FP_DEBUG_CAPTURE
void UACFProceduralLocomotionMotionComponent::GetDebugState(TArray<FACFProceduralMotionDebugValue>& OutValues) const
{
	OutValues.Emplace(TEXT("State"), ActiveStateTag.IsValid() ? ActiveStateTag.GetTagName().ToString() : FString(TEXT("none")));
	OutValues.Emplace(TEXT("Phase"), FString::Printf(TEXT("%.2f  at %.2f/s"), Phase, CurrentRate * CycleRateScale));
	OutValues.Emplace(TEXT("Speed"), FString::Printf(TEXT("%.0f cm/s  %s"), LocomotionInput.GroundSpeed, LocomotionInput.bGrounded ? TEXT("grounded") : TEXT("airborne")));

	// Only while a step is being absorbed. A value that never settles means something is changing every
	// frame, which is a setup problem rather than a smoothing one.
	const float Correction = CycleGuard.GetCorrectionSize() + EventGuard.GetCorrectionSize();
	if (Correction > KINDA_SMALL_NUMBER)
	{
		OutValues.Emplace(TEXT("Correcting"), FString::Printf(TEXT("%.3f"), Correction));
	}

	for (const FACFProceduralLocomotionEventPlayer& Player : ActiveEvents)
	{
		const FString Value = Player.DelayRemaining > 0.f
			? FString::Printf(TEXT("delayed %.2fs"), Player.DelayRemaining)
			: FString::Printf(TEXT("%.2fs  x%.2f"), Player.Playback, Player.Scale);

		OutValues.Emplace(Player.Tag.GetTagName(), CopyTemp(Value));
	}
}
#endif

#if WITH_EDITOR
void UACFProceduralLocomotionMotionComponent::PreviewState()
{
	bPreviewStateActive = !bPreviewStateActive;
}

void UACFProceduralLocomotionMotionComponent::PreviewEvent()
{
	PlayLocomotionEvent(PreviewEventTag, 1.f);
}
#endif
