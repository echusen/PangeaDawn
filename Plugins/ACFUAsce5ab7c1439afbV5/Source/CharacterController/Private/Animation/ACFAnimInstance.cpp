// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Animation/ACFAnimInstance.h"
#include "Animation/ACFAnimsetDataAsset.h"
#include "ACFCCFunctionLibrary.h"
#include "ATSBaseTargetComponent.h"
#include "Animation/ACFAnimTypes.h"
#include "Animation/ACFIKLayer.h"
#include "Animation/ACFMovesetLayer.h"
#include "Animation/ACFBaseOverlayLayer.h"
#include "Animation/ACFRiderLayer.h"
#include "Animation/ACFClimbingLayer.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Components/ACFCameraAimComponent.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "Components/ACFClimbingComponent.h"
#include "Components/ACFFirstPersonComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "KismetAnimationLibrary.h"
#include <Animation/AimOffsetBlendSpace1D.h>
#include <GameFramework/CharacterMovementComponent.h>
#include <GameFramework/Pawn.h>
#include <Kismet/KismetMathLibrary.h>
#include <Kismet/KismetSystemLibrary.h>
#include <TimerManager.h>

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
// Mirrors the Blueprint's Delta(Rotator) / MakeRotFromX(-Forward) conversion. Keep the
// reference rotation explicit: an MM root is NOT necessarily aligned with the capsule.
FVector2D CalculateRootRelativeAimOffset(const FRotator& AimRotation, const FRotator& RootRotation,
	bool bStrafing, float YawLimit, float RearBlendAngle)
{
	const FRotator forwardDelta = (AimRotation - RootRotation).GetNormalized();
	const float yawLimit = FMath::Clamp(YawLimit, 0.f, 180.f);
	const FVector2D forwardAim(FMath::Clamp(forwardDelta.Yaw, -yawLimit, yawLimit), forwardDelta.Pitch);
	const float absYaw = FMath::Abs(forwardDelta.Yaw);
	if (bStrafing || absYaw <= yawLimit) {
		return forwardAim;
	}

	// Negate the WORLD aim direction before calculating the relative rotation, exactly like
	// the Blueprint. Negating relative pitch/yaw is not equivalent when the root is tilted.
	const FRotator rearRotation = UKismetMathLibrary::MakeRotFromX(-AimRotation.Vector());
	const FRotator rearDelta = (rearRotation - RootRotation).GetNormalized();
	const FVector2D rearAim(FMath::Clamp(rearDelta.Yaw, -yawLimit, yawLimit), rearDelta.Pitch);
	const float blendWidth = FMath::Clamp(RearBlendAngle, 0.f, 180.f - yawLimit);
	const float blendPosition = blendWidth > UE_SMALL_NUMBER
		? FMath::Clamp((absYaw - yawLimit) / blendWidth, 0.f, 1.f) : 1.f;
	const float alpha = blendPosition * blendPosition * (3.f - 2.f * blendPosition);
	return FMath::Lerp(forwardAim, rearAim, alpha);
}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FACFRootRelativeAimOffsetTest,
	"ACF.Animation.AimOffset.RootRelativeConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FACFRootRelativeAimOffsetTest::RunTest(const FString& Parameters)
{
	const FRotator root(10.f, 70.f, 0.f);
	const FVector2D relative = CalculateRootRelativeAimOffset(FRotator(30.f, 100.f, 0.f), root, true, 90.f, 15.f);
	TestTrue(TEXT("Uses animated root rather than actor rotation"), relative.Equals(FVector2D(30.f, 20.f), 0.001f));
	TestTrue(TEXT("Aligned root gives zero yaw and pitch"),
		CalculateRootRelativeAimOffset(root, root, true, 90.f, 15.f).IsNearlyZero());
	for (const float yaw : { -179.f, -135.f, -91.f, 91.f, 135.f, 179.f }) {
		const FVector2D strafe = CalculateRootRelativeAimOffset(FRotator(25.f, yaw, 0.f), FRotator::ZeroRotator, true, 90.f, 15.f);
		TestTrue(TEXT("Strafe clamps yaw without rear folding or pitch inversion"),
			strafe.Equals(FVector2D(FMath::Sign(yaw) * 90.f, 25.f), 0.001f));
	}
	TestTrue(TEXT("Yaw limit is tunable"),
		CalculateRootRelativeAimOffset(FRotator(0.f, 80.f, 0.f), FRotator::ZeroRotator, true, 45.f, 15.f)
		.Equals(FVector2D(45.f, 0.f), 0.001f));
	const FVector2D rear = CalculateRootRelativeAimOffset(FRotator(25.f, 160.f, 0.f), FRotator::ZeroRotator, false, 90.f, 15.f);
	TestTrue(TEXT("Non-strafe retains Blueprint rear direction"), rear.Equals(FVector2D(-20.f, -25.f), 0.001f));
	const FVector2D before = CalculateRootRelativeAimOffset(FRotator(25.f, 90.f, 0.f), FRotator::ZeroRotator, false, 90.f, 15.f);
	const FVector2D after = CalculateRootRelativeAimOffset(FRotator(25.f, 90.001f, 0.f), FRotator::ZeroRotator, false, 90.f, 15.f);
	TestTrue(TEXT("Rear blend is continuous across the threshold"), before.Equals(after, 0.01f));
	return true;
}
#endif

UACFAnimInstance::UACFAnimInstance()
{
	IKLayer = UACFIKLayer::StaticClass();
}

void UACFAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	SetReferences();

	bIsMoving = false;
}

void UACFAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();

	// NativeInitializeAnimation runs when the native mesh registers, before the construction script adds the
	// Blueprint components, so the optional ones (first person, camera aim, climbing) were not there yet.
	SetReferences();
	//	ResetToDefaultMoveset();
	//	ResetToDefaultOverlay();
	if (IKLayer && IKLayer != UACFIKLayer::StaticClass()) {
		LinkAnimClassLayers(IKLayer);
	}
}

void UACFAnimInstance::SetMoveset(const FGameplayTag& MovesetTag)
{
	FMoveset* movLayer = MovesetLayers.FindByKey(MovesetTag);
	if (movLayer && movLayer->Moveset) {
		if (currentMovesetInstance) {
			currentMovesetInstance->OnDeactivated();
		}
		LinkAnimClassLayers(movLayer->Moveset);
		currentMoveset = *movLayer;
		currentMovesetInstance = Cast<UACFMovesetLayer>(GetLinkedAnimLayerInstanceByClass(movLayer->Moveset));
		if (currentMovesetInstance) {
			currentMovesetInstance->OnActivated();
		}
		OnMovesetChanged.Broadcast(MovesetTag);
	}
}

bool UACFAnimInstance::GetAnimationOverlay(const FGameplayTag& tag, FOverlayLayer& outOverlay)
{
	FOverlayLayer* outOv = OverlayLayers.FindByKey(tag);
	if (outOv) {
		outOverlay = *outOv;
		return true;
	}

	return false;
}

void UACFAnimInstance::SetRidingLayer(const FGameplayTag& mountTag)
{
	FRiderLayer* rider = RiderLayers.FindByKey(mountTag);
	if (rider && rider->RiderLayer) {
		if (currentRiderInstance) {
			currentRiderInstance->OnDeactivated();
		}
		LinkAnimClassLayers(rider->RiderLayer);
		currentRiderLayer = *rider;
		currentRiderInstance = Cast<UACFRiderLayer>(GetLinkedAnimLayerInstanceByClass(rider->RiderLayer));
		if (currentRiderInstance) {
			currentRiderInstance->OnActivated();
		}
		OnRiderLayerChanged.Broadcast(mountTag);
	}
}

void UACFAnimInstance::SetAnimationOverlay(const FGameplayTag& overlayTag)
{
	FOverlayLayer* overlay = OverlayLayers.FindByKey(overlayTag);
	if (overlay && overlay->Overlay) {
		RemoveOverlay();
		LinkAnimClassLayers(overlay->Overlay);
		currentOverlay = *overlay;
		currentOverlayInstance = Cast<UACFBaseOverlayLayer>(GetLinkedAnimLayerInstanceByClass(overlay->Overlay));
		if (currentOverlayInstance) {
			currentOverlayInstance->OnActivated();
		}
		OnOverlayChanged.Broadcast(overlayTag);
	}
	else {
		RemoveOverlay();
	}
}

void UACFAnimInstance::RemoveOverlay()
{
	if (currentOverlayInstance) {
		currentOverlayInstance->OnDeactivated();
		currentOverlayInstance = nullptr;
	}
	if (currentOverlay.Overlay) {
		UnlinkAnimClassLayers(currentOverlay.Overlay);
	}

	currentOverlay = FOverlayLayer();
	OnOverlayChanged.Broadcast(FGameplayTag());
}

void UACFAnimInstance::ActivateClimbingLayer()
{
	if (!ClimbingLayer || currentClimbingInstance) {
		return;
	}
	LinkAnimClassLayers(ClimbingLayer);
	currentClimbingInstance = Cast<UACFClimbingLayer>(GetLinkedAnimLayerInstanceByClass(ClimbingLayer));
	if (currentClimbingInstance) {
		currentClimbingInstance->OnActivated();
	}
	OnClimbingLayerChanged.Broadcast(true);
}

void UACFAnimInstance::DeactivateClimbingLayer()
{
	if (!currentClimbingInstance) {
		return;
	}
	currentClimbingInstance->OnDeactivated();
	UnlinkAnimClassLayers(ClimbingLayer);
	currentClimbingInstance = nullptr;
	OnClimbingLayerChanged.Broadcast(false);
}

bool UACFAnimInstance::GetMovesetByTag(const FGameplayTag& movesetTag, FMoveset& outMoveset) const
{
	const FMoveset* outOv = MovesetLayers.FindByKey(movesetTag);
	if (outOv) {
		outMoveset = *outOv;
		return true;
	}

	return false;
}

FVector UACFAnimInstance::CalculateRelativeAccelerationAmount() const
{
	if (FVector::DotProduct(MovementComp->GetCurrentAcceleration(), WorldSpeed) > 0.0f) {
		const float MaxAcc = CharacterOwner->GetCharacterMovement()->GetMaxAcceleration();
		ensure(MaxAcc != 0.f);
		return OwnerRotation.UnrotateVector(
			MovementComp->GetCurrentAcceleration().GetClampedToMaxSize(MaxAcc) / MaxAcc);
	}

	const float MaxBrakingDec = CharacterOwner->GetCharacterMovement()->GetMaxBrakingDeceleration();
	ensure(MaxBrakingDec != 0.f);
	return OwnerRotation.UnrotateVector(
		MovementComp->GetCurrentAcceleration().GetClampedToMaxSize(MaxBrakingDec) / MaxBrakingDec);
}

void UACFAnimInstance::HandleTargetLocomotionStateChanged(ELocomotionState newState)
{
	bIsWalking = newState == ELocomotionState::EWalk;
}

void UACFAnimInstance::SetReferences()
{
	CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
	if (CharacterOwner) {
		MovementComp = Cast<UACFCharacterMovementComponent>(CharacterOwner->GetCharacterMovement());
		if (MovementComp) {
			bIsWalking = (MovementComp->GetTargetLocomotionState() == ELocomotionState::EWalk);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Owner doesn't have ACFCharachterMovement Comp!!!!"));
		}
		ClimbingComponent = CharacterOwner->FindComponentByClass<UACFClimbingComponent>();
		CameraAimComponent = CharacterOwner->FindComponentByClass<UACFCameraAimComponent>();
		FirstPersonComponent = CharacterOwner->FindComponentByClass<UACFFirstPersonComponent>();
		UpdateTargetComponentRef();
		bReferencesResolved = true;
	}
}

bool UACFAnimInstance::GetTargetLookAtLocation(FVector& outLookAtLocation) const
{
	if (!bHasTargetLookAt) {
		return false;
	}

	outLookAtLocation = TargetLookAtLocation;
	return true;
}

void UACFAnimInstance::UpdateTargetData()
{
	if (!CharacterOwner) {
		return;
	}

	// The look-at math lives on the targeting component so the Look At node can use it without
	// this anim instance. We only keep a Game Thread copy for the thread-safe Blueprint getters
	// and for the aim offset, which is still gated on lock-on rather than "has a target".
	bHasTargetLookAt = TargetComponent && TargetComponent->GetLookAtLocation(TargetLookAtLocation);

	bIsLockedOnTarget = bHasTargetLookAt && TargetComponent->GetIsLockOnActive();

	if (bIsLockedOnTarget) {
		const FVector eyeLocation = CharacterOwner->GetActorLocation() + FVector(0.f, 0.f, CharacterOwner->BaseEyeHeight);
		AimRotation = UKismetMathLibrary::FindLookAtRotation(eyeLocation, TargetLookAtLocation);
	} else {
		// Control rotation only exists where the controller lives, so simulated proxies fall back
		// to the base aim rotation, which is rebuilt from the replicated remote view pitch.
		AimRotation = CharacterOwner->IsLocallyControlled()
			? CharacterOwner->GetControlRotation()
			: CharacterOwner->GetBaseAimRotation();
	}
}

void UACFAnimInstance::UpdateTargetComponentRef()
{
	AController* controller = CharacterOwner ? CharacterOwner->GetController() : nullptr;
	if (controller == cachedTargetController.Get()) {
		return;
	}

	cachedTargetController = controller;
	TargetComponent = UATSBaseTargetComponent::FindForActor(CharacterOwner);
}

void UACFAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	// Game Thread only. NativeThreadSafeUpdateAnimation runs on a parallel worker thread and
	// must never call FindComponentByClass there - it can race with the owner's component
	// array being mutated on the Game Thread (this is what caused the "Container has changed
	// during ranged-for iteration" ensure/crash). This is a one-shot safety net in case the
	// pawn wasn't ready yet during NativeInitializeAnimation; once resolved we don't redo the
	// lookup every tick.
	if (!bReferencesResolved) {
		SetReferences();
	}
	bAimOffsetStrafing = MovementComp && MovementComp->IsCharacterStrafing();

	// The controller can change after references resolved (possession, respawn), and the
	// targeting component lives on it, so this has to be re-checked on the Game Thread.
	UpdateTargetComponentRef();

	// The target moves every frame and resolving its transform is only safe here. Aim offset
	// graphs read this copy; the Look At node reads the targeting component from PreUpdate.
	UpdateTargetData();

	UpdateCameraAimData();

	UpdateFirstPersonData();
}

void UACFAnimInstance::UpdateFirstPersonData()
{
	bIsInFirstPerson = FirstPersonComponent && FirstPersonComponent->IsInFirstPerson();

	if (FirstPersonComponent) {
		AimingAlpha = FirstPersonComponent->GetAimingAlpha();
		ItemReferenceBone = FirstPersonComponent->GetItemReferenceBone();
		HandOffsetData = FirstPersonComponent->GetHandOffsetData();
	} else {
		AimingAlpha = MovementComp && MovementComp->GetIsAiming() ? 1.f : 0.f;
		ItemReferenceBone = NAME_None;
		HandOffsetData = FACFProceduralHandOffsetData();
	}
}

void UACFAnimInstance::UpdateCameraAimData()
{
	if (!CameraAimComponent) {
		bCameraAimActive = false;
		return;
	}

	// The component samples the view frame in its own tick, which is ordered before the mesh's,
	// so what lands here is this frame's value and not the previous one.
	bCameraAimActive = CameraAimComponent->IsCameraAimActive();
	if (bCameraAimActive) {
		CameraViewRotation = CameraAimComponent->GetViewRotation();
	}
}

void UACFAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	if (CharacterOwner && MovementComp) {
		if (bUpdateMovementData) {
			UpdateStateData(DeltaSeconds);
		}
		if (bUpdateLocationData) {
			UpdateLocation(DeltaSeconds);
		}
		if (bUpdateRotationData) {
			UpdateRotation(DeltaSeconds);
		}
		if (bUpdateSpeedData) {
			UpdateVelocity(DeltaSeconds);
		}
		if (bUpdateAccelerationData) {
			UpdateAcceleration(DeltaSeconds);
		}
		if (bUpdateAimData) {
			UpdateAimData(DeltaSeconds);
		}
		if (bUpdateJumpData) {
			UpdateJump(DeltaSeconds);
		}
		if (bUpdateLeaningData) {
			UpdateLeaning(DeltaSeconds);
		}
	}
}
void UACFAnimInstance::UpdateLocation(float deltatime)
{
	DisplacementDelta = UKismetMathLibrary::VSizeXY(GetOwningActor()->GetActorLocation() - OwnerLocation);
	OwnerLocation = GetOwningActor()->GetActorLocation();
	Displacement = UKismetMathLibrary::SafeDivide(DisplacementDelta, deltatime);
}

void UACFAnimInstance::UpdateRotation(float deltatime)
{
	PreviousRotation = OwnerRotation;
	OwnerRotation = CharacterOwner->GetActorRotation();
	ControlRotation = CharacterOwner->GetControlRotation();
	YawDelta = OwnerRotation.Yaw - PreviousRotation.Yaw;
	YawSpeed = UKismetMathLibrary::SafeDivide(YawDelta, deltatime);
	const FRotator delta = UKismetMathLibrary::NormalizedDeltaRotator(OwnerRotation, PreviousRotation);
	const float turn = delta.Yaw;
	TurnRate = FMath::FInterpTo(TurnRate, turn, deltatime, TurnRateSmoothing);
}

void UACFAnimInstance::UpdateLeaning(float deltatime)
{
	LeanAngle = UKismetMathLibrary::SafeDivide(YawSpeed, deltatime) * 0.01f * UKismetMathLibrary::Abs(NormalizedSpeed) * LeaningFactor;
}

void UACFAnimInstance::UpdateVelocity(float deltatime)
{
	LastSpeedDirectionWithOffset = SpeedDirectionWithOffset;
	LastSpeedDirection = SpeedDirection;
	PreviousSpeed = Speed;
	bWasMoving = !FMath::IsNearlyZero(Speed, IsMovingSpeedThreshold);
	WorldSpeed = CharacterOwner->GetVelocity();
	Speed = WorldSpeed.Size();
	WorldSpeed2D = WorldSpeed * FVector(1.f, 1.f, 0.f);
	LocalSpeed2D = OwnerRotation.UnrotateVector(WorldSpeed2D);
	Direction = UKismetAnimationLibrary::CalculateDirection(WorldSpeed2D, OwnerRotation);
	DirectionWithOffset = Direction - YawOffset;
	SpeedDirection = GetDirectionFromAngle(Direction, LastSpeedDirection, bWasMoving);
	SpeedDirectionWithOffset = GetDirectionFromAngle(DirectionWithOffset, LastSpeedDirectionWithOffset, bWasMoving);
	NormalizedSpeed = UKismetMathLibrary::SafeDivide(Speed, MovementComp->GetCharacterMaxSpeed());
	if (IsLocalPlayer()) {
		DeltaInputDirection = UKismetAnimationLibrary::CalculateDirection(MovementComp->GetLastInputVector(), CharacterOwner->GetActorRotation());
	}
	else {
		// We don't have inputs on server
		DeltaInputDirection = DirectionWithOffset;
	}
	bIsMoving = !FMath::IsNearlyZero(Speed, IsMovingSpeedThreshold);
	if (bIsSwimming) {
		NormalizedSwimSpeed = MovementComp->GetNormalizedSwimSpeed();
	}
}

bool UACFAnimInstance::IsLocalPlayer() const
{
    return CharacterOwner && CharacterOwner->IsLocallyControlled() && CharacterOwner->IsPlayerControlled();
}

FRotator UACFAnimInstance::GetOwnerControllerRotation() const
{
	return ControlRotation;
}

ELocomotionState UACFAnimInstance::GetCurrentLocomotionState() const
{
    return MovementComp ? MovementComp->GetCurrentLocomotionState() : ELocomotionState::EIdle;
}

ELocomotionState UACFAnimInstance::GetTargetLocomotionState() const
{
    return MovementComp ? MovementComp->GetTargetLocomotionState() : ELocomotionState::EIdle;
}

EACFDirection UACFAnimInstance::GetDirectionFromAngle_Implementation(float angle, EACFDirection currentDirection, bool bUseCurrentDirection)
{
	const float absAngle = FMath::Abs(angle);
	float fwdZone = DirectionDeadZone;
	float bwdZone = DirectionDeadZone;

	if (bUseCurrentDirection) {
		switch (currentDirection) {
		case EACFDirection::Front:
			fwdZone *= 2.f;
			break;
		case EACFDirection::Back:
			bwdZone *= 2.f;
			break;
		default:
			break;
		}
	}
	if (absAngle <= (45.f + fwdZone)) {
		return EACFDirection::Front;
	}
	if (absAngle >= (135.f - bwdZone)) {
		return EACFDirection::Back;
	}
	if (angle > 0.f) {
		return EACFDirection::Right;
	}
	return EACFDirection::Left;
}

FVector2D UACFAnimInstance::GetAimOffset(const FTransform& InRootTransform) const
{
	return ConvertRotationToAimOffset(AimRotation, InRootTransform);
}

FVector2D UACFAnimInstance::ConvertRotationToAimOffset(FRotator InRotation, const FTransform& InRootTransform) const
{
	const FVector2D offset = CalculateRootRelativeAimOffset(InRotation, InRootTransform.Rotator(),
		bAimOffsetStrafing, AimOffsetYawLimit, AimOffsetRearBlendAngle);
	static const FName DisableAimCurve(TEXT("Disable_AO"));
	return FMath::Lerp(offset, FVector2D::ZeroVector, GetCurveValue(DisableAimCurve));
}

void UACFAnimInstance::UpdateAimData(float deltatime)
{
	// Preserve the legacy cached property for non-MM consumers. MM graphs should call
	// GetAimOffset with their current RootTransform: it includes root-motion/offset rotation
	// and must not acquire an additional frame of latency or this legacy interpolation.
	FRotator referenceRotation = CharacterOwner->GetActorRotation();
	referenceRotation.Yaw += YawOffset;
	const FVector2D target = CalculateRootRelativeAimOffset(AimRotation, referenceRotation,
		bAimOffsetStrafing, AimOffsetYawLimit, AimOffsetRearBlendAngle);

	AimOffset.X = FMath::FInterpTo(
		AimOffset.X, target.X, deltatime, AimOffsetInterpSpeed);

	AimOffset.Y = FMath::FInterpTo(AimOffset.Y, target.Y, deltatime,
		AimOffsetInterpSpeed);
}

void UACFAnimInstance::UpdateAcceleration(float deltatime)
{
	Acceleration = MovementComp->GetCurrentAcceleration().Size();
	Acceleration2D = MovementComp->GetCurrentAcceleration() * FVector(1.f, 1.f, 0.f);
	LocalAccel2D = OwnerRotation.UnrotateVector(Acceleration2D);
	bIsAccelerating = Acceleration > 0;
	NormalizedAccel = Acceleration2D;
	NormalizedAccel.Normalize();
	PivotDirection = UKismetMathLibrary::VLerp(PivotDirection, NormalizedAccel, .5f);
	PivotDirection.Normalize();
	const float accelerationAngle = UKismetAnimationLibrary::CalculateDirection(PivotDirection, OwnerRotation);
	AccelerationDirection = UACFCCFunctionLibrary::GetOppositeDirectionFromAngle(accelerationAngle);
}

void UACFAnimInstance::UpdateStateData(float deltatime)
{
	bIsOnGround = MovementComp->IsMovingOnGround();
	bWasCrouching = bIsCrouching;
	bIsCrouching = CharacterOwner->bIsCrouched;
	bCrouchChanged = bIsCrouching != bWasCrouching;
	bWalkStateChanged = bWasWalking != bIsWalking;
	bWasWalking = bIsWalking;
	bIsStrafing = MovementComp->IsCharacterStrafing();
	bIsWalking = MovementComp->GetTargetLocomotionState() == ELocomotionState::EWalk; // || MovementComp->GetCurrentLocomotionState() == ELocomotionState::EIdle;
	const bool bNowSwimming = MovementComp->IsSwimming();
	if (bNowSwimming) {
		if (!bIsSwimming) {
			bCachedFootIKEnabled = EnableFootIK;
		}
		EnableFootIK = false;
	}
	else if (bIsSwimming) {
		EnableFootIK = bCachedFootIKEnabled;
	}
	bIsSwimming = bNowSwimming;
	// ClimbingComponent is cached on the Game Thread in SetReferences() - do NOT call
	// FindComponentByClass here, this function runs on a parallel worker thread.
	bIsClimbing = ClimbingComponent && ClimbingComponent->IsClimbing();
}

void UACFAnimInstance::UpdateJump(float deltatime)
{
	// Check if the play is falling?
	bIsJumping = false;
	bIsFalling = false;
	bIsInAir = false;
	if (MovementComp->IsFalling()) {
		if (WorldSpeed.Z > 20.f) {
			bIsJumping = true;
			bIsInAir = true;
		}
		else if (WorldSpeed.Z < -20.f) {
			bIsFalling = true;
			bIsInAir = true;
		}
	}

	const FCharacterGroundInfo& GroundInfo = MovementComp->GetGroundInfo();
	GroundDistance = GroundInfo.GroundDistance;

	if (bIsJumping) {
		TimeToApex = UKismetMathLibrary::SafeDivide(-WorldSpeed.Z, MovementComp->GetGravityZ());
	}
	else {
		TimeToApex = 0.f;
	}
}

bool UACFAnimInstance::GetIsCrouching() const
{
	return bIsCrouching;
}

void UACFAnimInstance::ResetToDefaultMoveset()
{
	if (MovementComp) {
		MovementComp->SetCurrentMoveset(UACFCCFunctionLibrary::GetMovesetTypeTagRoot());

	}
}

void UACFAnimInstance::ResetToDefaultOverlay()
{
	if (MovementComp) {
		MovementComp->SetCurrentOverlay(UACFCCFunctionLibrary::GetAnimationOverlayTagRoot());
	}
}

void UACFAnimInstance::SetTurnInPlaceYawOffset(float inYawOffset)
{
	YawOffset = inYawOffset;
}

void UACFAnimInstance::ApplyAnimset(UACFAnimsetDataAsset* AnimsetData)
{
    if (!AnimsetData)
    {
        return;
    }

    // Replace only non-empty tables so a partial animset can act as an overlay.
    if (AnimsetData->MovesetLayers.Num() > 0)
    {
        MovesetLayers = AnimsetData->MovesetLayers;
    }

    // Only override the overlay setup when the animset actually provides one, so a partial
    // animset (or none) does NOT wipe the AnimInstance's own overlay config set in the editor.
    if (AnimsetData->OverlayLayers.Num() > 0)
    {
        OverlayLayers = AnimsetData->OverlayLayers;
    }

    if (AnimsetData->RiderLayers.Num() > 0)
    {
        RiderLayers = AnimsetData->RiderLayers;
    }

    if (AnimsetData->IKLayer)
    {
        IKLayer = AnimsetData->IKLayer;
    }

    if (AnimsetData->ClimbingLayer)
    {
        ClimbingLayer = AnimsetData->ClimbingLayer;
    }
}
