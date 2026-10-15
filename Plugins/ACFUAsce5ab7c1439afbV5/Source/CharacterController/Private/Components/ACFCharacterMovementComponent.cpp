// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFCharacterMovementComponent.h"
#include "ACFCCTypes.h"
#include "ACFRPGFunctionLibrary.h"
#include "ACFRPGTypes.h"
#include "ACFGASStatisticsComponent.h"
#include "Animation/ACFAnimInstance.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include <Camera/CameraComponent.h>
#include <Components/SkeletalMeshComponent.h>
#include "ACFCCFunctionLibrary.h"
#include "ACFActionTypes.h"
#include "ATSBaseTargetComponent.h"
#include "Actions/ACFActionAbility.h"
#include "Components/ACFClimbingComponent.h"
#include "Components/ACFFirstPersonComponent.h"

UACFCharacterMovementComponent::UACFCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEditableWhenInherited = true;
	PrimaryComponentTick.bCanEverTick = true;

	LocomotionStates.Add(FACFLocomotionState(ELocomotionState::EIdle, 0.f, 0.f));
	LocomotionStates.Add(FACFLocomotionState(ELocomotionState::EWalk, 250.f, 180.f));
	LocomotionStates.Add(FACFLocomotionState(ELocomotionState::EJog, 500.f, 350.f));
	LocomotionStates.Add(FACFLocomotionState(ELocomotionState::ESprint, 650.f, 500.f));

	SetIsReplicatedByDefault(true);
	currentLocomotionState = ELocomotionState::EIdle;
	currentMovestance = FGameplayTag();
	SetIsAiming(false);
}


float UACFCharacterMovementComponent::GetRotationRateYaw() const
{
	if (IsSwimming()) {
		return SwimRotationRate.Yaw;
	}
	return RotationRate.Yaw;
}

void UACFCharacterMovementComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UACFCharacterMovementComponent, targetLocomotionState);
	DOREPLIFETIME(UACFCharacterMovementComponent, bAiming);
	DOREPLIFETIME(UACFCharacterMovementComponent, LocomotionStates);
	DOREPLIFETIME(UACFCharacterMovementComponent, MaxSwimAcceleration);
	DOREPLIFETIME(UACFCharacterMovementComponent, MaxSwimBrakingDeceleration);
	DOREPLIFETIME(UACFCharacterMovementComponent, CharacterMaxSpeed);
	DOREPLIFETIME(UACFCharacterMovementComponent, CharacterMaxSwimSpeed);
	DOREPLIFETIME(UACFCharacterMovementComponent, bCanMove);
	DOREPLIFETIME(UACFCharacterMovementComponent, currentMovestance);
	DOREPLIFETIME(UACFCharacterMovementComponent, RotationMode);
	DOREPLIFETIME(UACFCharacterMovementComponent, currentLocomotionState);
	DOREPLIFETIME(UACFCharacterMovementComponent, bIsClimbingReplicated);
	DOREPLIFETIME(UACFCharacterMovementComponent, currentMoveset);
	DOREPLIFETIME(UACFCharacterMovementComponent, currentOverlay);
	DOREPLIFETIME(UACFCharacterMovementComponent, currentRider);
	// The body rotation it drives is part of the replicated move, so server and client must
	// agree on it even when a Blueprint toggles it at runtime.
	DOREPLIFETIME(UACFCharacterMovementComponent, bOrientToLockOnTarget);
	DOREPLIFETIME(UACFCharacterMovementComponent, MaxFallingSpeedOverride);
}

void UACFCharacterMovementComponent::SimulateMovement(float DeltaTime)
{
	if (bHasReplicatedAcceleration) {
		// Preserve our replicated acceleration
		const FVector OriginalAcceleration = Acceleration;
		Super::SimulateMovement(DeltaTime);
		Acceleration = OriginalAcceleration;
	}
	else {
		Super::SimulateMovement(DeltaTime);
	}
}

bool UACFCharacterMovementComponent::CanAttemptJump() const
{
	const UACFAbilitySystemComponent* actionsManager = GetOwner()->FindComponentByClass<UACFAbilitySystemComponent>();
	if (actionsManager && actionsManager->IsPerformingAction()) {
		return IsJumpAllowed() && actionsManager->GetCurrentAction()->GetActionConfig().PerformableInMovementModes.Contains(EMovementMode::MOVE_Falling);
	}

	return IsJumpAllowed(); //(IsMovingOnGround() || IsFalling() Falling included for double-jump and non-zero jump hold time, but validated by character.
}

void UACFCharacterMovementComponent::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

void UACFCharacterMovementComponent::SetReplicatedClimbingState(bool bInIsClimbing)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}

	if (bIsClimbingReplicated == bInIsClimbing) {
		return;
	}

	bIsClimbingReplicated = bInIsClimbing;
}

void UACFCharacterMovementComponent::SetIsAiming_Implementation(bool bIsAiming)
{
	if (bIsAiming) {
		ActivateLocomotionStance(UGameplayTagsManager::Get().RequestGameplayTag(ACF::AimTag));
		// SetLocomotionState(LocomotionStateWhileAiming);
	}
	else {
		DeactivateCurrentLocomotionStance();
		// ResetToDefaultLocomotionState();
	}
	bAiming = bIsAiming;
	if (FirstPersonComponent) {
		FirstPersonComponent->SetAiming(bAiming);
	}
	OnAimChanged.Broadcast(bAiming);
}



void UACFCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	Character = Cast<ACharacter>(GetOwner());
	UpdateCharacterMaxSpeed();

	LocomotionStates.Sort();
}

void UACFCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	defaultRotMode = RotationMode;

	// targetLocomotionState default-constructs to EWalk, so SetLocomotionState(EWalk) would hit
	// the "already in this state" early-out and never apply the state speeds: the character would
	// move at the raw component MaxWalkSpeed (jog) until the first real state change. Apply the
	// default state directly. Done on every instance, not just the server, because a replicated
	// value equal to the default never triggers OnRep_LocomotionState on clients.
	Internal_ApplyLocomotionState(DefaultState);

	if (Character && Character->GetMesh()) {
		animInstance = Cast<UACFAnimInstance>(Character->GetMesh()->GetAnimInstance());
		if (animInstance) {
			SetCurrentMoveset(UACFCCFunctionLibrary::GetMovesetTypeTagRoot());
			SetCurrentOverlay(UACFCCFunctionLibrary::GetAnimationOverlayTagRoot());
		}
	}
	Internal_SetStrafe();

	ClimbingComponent = GetOwner()->FindComponentByClass<UACFClimbingComponent>();
	if (ClimbingComponent && !ClimbingComponent->OnClimbingStateChanged.IsAlreadyBound(this, &UACFCharacterMovementComponent::HandleClimbingStateChanged)) {
		ClimbingComponent->OnClimbingStateChanged.AddDynamic(this, &UACFCharacterMovementComponent::HandleClimbingStateChanged);
	}

	// Keep animation layer state coherent if replication arrived before anim init.
	if (bIsClimbingReplicated) {
		HandleClimbingStateChanged(true);
	}

	FirstPersonComponent = GetOwner()->FindComponentByClass<UACFFirstPersonComponent>();
	if (FirstPersonComponent && !FirstPersonComponent->OnFirstPersonChanged.IsAlreadyBound(this, &UACFCharacterMovementComponent::HandleFirstPersonChanged)) {
		FirstPersonComponent->OnFirstPersonChanged.AddDynamic(this, &UACFCharacterMovementComponent::HandleFirstPersonChanged);
	}

	// First person may already be on if its state replicated before this BeginPlay.
	if (FirstPersonComponent && FirstPersonComponent->IsInFirstPerson()) {
		HandleFirstPersonChanged(true);
	}

	// The aiming state may have replicated before the component was found, too.
	if (FirstPersonComponent) {
		FirstPersonComponent->SetAiming(bAiming);
	}
}

void UACFCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateLocomotion();

	if (ClimbingComponent) {
		ClimbingComponent->TickClimbing();
	}
}

void UACFCharacterMovementComponent::UpdateLocomotion()
{
	if (!Character) {
		return;
	}

	if (GetOwner() && !IsFalling() && animInstance && !animInstance->IsAnyMontagePlaying()) {
		if (MovementMode == MOVE_Walking || MovementMode == MOVE_Swimming) {
			// While swimming, velocity is capped at MaxStateSwimSpeed, so states must be
			// classified against the swim-speed bands or Sprint can never be reached.
			const bool bIsSwimmingMode = MovementMode == MOVE_Swimming;
			for (int i = 0; i < LocomotionStates.Num() - 1; i++) {
				const float Speed = GetOwner()->GetVelocity().Size();
				const float LowerStateSpeed = bIsSwimmingMode ? LocomotionStates[i].MaxStateSwimSpeed : LocomotionStates[i].MaxStateSpeed;
				const float UpperStateSpeed = bIsSwimmingMode ? LocomotionStates[i + 1].MaxStateSwimSpeed : LocomotionStates[i + 1].MaxStateSpeed;
				if (FMath::IsNearlyZero(Speed) && currentLocomotionState != ELocomotionState::EIdle) {
					HandleStateChanged(ELocomotionState::EIdle);
				}

				else if (LocomotionStates[i + 1].State != currentLocomotionState && Speed > LowerStateSpeed + 5.f && Speed <= UpperStateSpeed + 5.f) {
					HandleStateChanged(LocomotionStates[i + 1].State);
				}
			}

			if (GetOwner()->HasAuthority() && currentLocomotionState == ELocomotionState::ESprint) {
				const float Direction = animInstance->GetDirection();
				if (FMath::Abs(Direction) > SprintDirectionCone) {
					SetLocomotionState(ELocomotionState::EJog);
				}

			}
		}

	}
}

const FCharacterGroundInfo& UACFCharacterMovementComponent::GetGroundInfo()
{
	if (!CharacterOwner || (GFrameCounter == CachedGroundInfo.LastUpdateFrame)) {
		return CachedGroundInfo;
	}

	if (MovementMode == MOVE_Walking) {
		CachedGroundInfo.GroundHitResult = CurrentFloor.HitResult;
		CachedGroundInfo.GroundDistance = 0.0f;
	}
	else {
		const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = (UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn);
		const FVector TraceStart(CharacterOwner->GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, (TraceStart.Z - GroundTraceDistance - CapsuleHalfHeight));

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ACFCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParam);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking) {
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit) {
			CachedGroundInfo.GroundDistance = FMath::Max((HitResult.Distance - CapsuleHalfHeight), 0.0f);
		}

		const FVector normal = HitResult.ImpactNormal;
		const FRotator actorRot = CharacterOwner->GetActorRotation();
		const FVector upVector = UKismetMathLibrary::GetUpVector(actorRot);
		const FVector rightVector = UKismetMathLibrary::GetUpVector(FRotator(0.f, actorRot.Yaw, 0.f));
		UKismetMathLibrary::GetSlopeDegreeAngles(rightVector, normal, upVector,
			CachedGroundInfo.SlopePitch, CachedGroundInfo.SlopeRoll);
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;

	return CachedGroundInfo;
}

const float UACFCharacterMovementComponent::GetGroundDistance()
{
	return GetGroundInfo().GroundDistance;
}


void UACFCharacterMovementComponent::SetLocomotionState(ELocomotionState State)
{
	if (targetLocomotionState == State) {
		return;
	}

	// Applied locally before the RPC: MaxWalkSpeed feeds GetMaxSpeed while the owning client simulates
	// its own moves, so waiting for the replicated value would run a full round trip at the old speed
	// and have every move made in that window corrected by the server.
	Internal_ApplyLocomotionState(State);

	if (GetOwner() && !GetOwner()->HasAuthority()) {
		Server_SetLocomotionState(State);
	}
}

void UACFCharacterMovementComponent::SetDefaultLocomotionState(ELocomotionState State)
{
	DefaultState = State;
}

ELocomotionState UACFCharacterMovementComponent::GetDefaultLocomotionState() 
{
	return DefaultState;
}

void UACFCharacterMovementComponent::Server_SetLocomotionState_Implementation(ELocomotionState State)
{
	if (targetLocomotionState == State) {
		return;
	}

	Internal_ApplyLocomotionState(State);
}

void UACFCharacterMovementComponent::Internal_ApplyLocomotionState(ELocomotionState State)
{
	UpdateMaxSpeed(State);

	OnTargetLocomotionStateChanged.Broadcast(State);
}

void UACFCharacterMovementComponent::SetLocomotionStateSpeed_Implementation(
	ELocomotionState State, float speed, float swimSpeed)
{
	FACFLocomotionState newState = FACFLocomotionState(State, speed, swimSpeed);
	newState.StateModifier = LocomotionStates.FindByKey(State)->StateModifier;
	LocomotionStates.Remove(State);
	LocomotionStates.AddUnique(newState);
	UpdateCharacterMaxSpeed();
	// needed to force the update of the speed
	SetLocomotionState(currentLocomotionState);
}

void UACFCharacterMovementComponent::SetCanMove_Implementation(bool inCanMove)
{
	bCanMove = inCanMove;
}

float UACFCharacterMovementComponent::GetCharacterMaxSpeedByState(ELocomotionState State)
{
	FACFLocomotionState* state = LocomotionStates.FindByKey(State);
	if (state) {
		return state->MaxStateSpeed;
	}
	return 0.0f;
}

float UACFCharacterMovementComponent::GetCharacterMaxSwimSpeedByState(ELocomotionState State)
{
	FACFLocomotionState* state = LocomotionStates.FindByKey(State);
	if (state) {
		return state->MaxStateSwimSpeed;
	}
	return 0.0f;
}

void UACFCharacterMovementComponent::UpdateCharacterMaxSpeed()
{
	if (Character->HasAuthority()) {
		float maxspeed = 0.0f;
		float maxSwimSpeed = 0.0f;
		for (const FACFLocomotionState& state : LocomotionStates) {
			if (state.MaxStateSpeed >= maxspeed) {
				maxspeed = state.MaxStateSpeed;
			}
			if (state.MaxStateSwimSpeed >= maxSwimSpeed) {
				maxSwimSpeed = state.MaxStateSwimSpeed;
			}
		}
		CharacterMaxSpeed = maxspeed;
		CharacterMaxSwimSpeed = maxSwimSpeed;
	}
}

float UACFCharacterMovementComponent::GetNormalizedSwimSpeed() const
{
	if (!GetOwner()) {
		return 0.f;
	}

	return UKismetMathLibrary::SafeDivide(GetOwner()->GetVelocity().Size(), CharacterMaxSwimSpeed);
}

void UACFCharacterMovementComponent::OnRep_LocomotionState()
{
	MaxWalkSpeed = GetCharacterMaxSpeedByState(targetLocomotionState.State);
	MaxSwimSpeed = GetCharacterMaxSwimSpeedByState(targetLocomotionState.State);
	targetLocomotionState.MaxStateSpeed = GetCharacterMaxSpeedByState(targetLocomotionState.State);
}

void UACFCharacterMovementComponent::OnRep_Moveset()
{
	if (GetACFAnimInstance()) {
		GetACFAnimInstance()->SetMoveset(currentMoveset);
	}
}

void UACFCharacterMovementComponent::OnRep_Overlay()
{
	if (GetACFAnimInstance()) {
		GetACFAnimInstance()->SetAnimationOverlay(currentOverlay);
	}
}

void UACFCharacterMovementComponent::OnRep_RiderLayer()
{
	if (GetACFAnimInstance()) {
		GetACFAnimInstance()->SetRidingLayer(currentRider);
	}
}

void UACFCharacterMovementComponent::OnRep_CurrentLocomotionState()
{
	OnLocomotionStateChanged.Broadcast(currentLocomotionState);
}

void UACFCharacterMovementComponent::OnRep_IsClimbingReplicated()
{
	HandleClimbingStateChanged(bIsClimbingReplicated);
}

void UACFCharacterMovementComponent::HandleStateChanged(ELocomotionState newState)
{
	if (currentLocomotionState == newState) {
		return;
	}

	FACFLocomotionState* oldState = LocomotionStates.FindByKey(currentLocomotionState);
	FACFLocomotionState* nextState = LocomotionStates.FindByKey(newState);

	if (oldState && nextState && Character) {
		if (GetOwner()->HasAuthority()) {

			if (activeEffect.IsValid()) {
				UACFRPGFunctionLibrary::RemovesActiveGameplayEffectFromActor(activeEffect, GetOwner());
				activeEffect.Invalidate();
			}

			UACFGASStatisticsComponent* statComp = Character->FindComponentByClass<UACFGASStatisticsComponent>();
			if (statComp) {
				activeEffect = statComp->AddAttributeSetModifier(nextState->StateModifier);
			}
		}
	}
	currentLocomotionState = newState;
	OnLocomotionStateChanged.Broadcast(newState);
}

void UACFCharacterMovementComponent::OnRep_LocomotionStance()
{
	OnLocomotionStanceChanged.Broadcast(currentMovestance);
}

void UACFCharacterMovementComponent::OnRep_IsStrafing()
{
	// The owner holds its lock locally, so a value replicated before the server locked too must not undo it.
	// Simulated proxies just mirror the server, and track it so unlocking lands on the server's mode.
	if (bRotationModeLocked && CharacterOwner && CharacterOwner->IsLocallyControlled()) {
		RotationMode = LockedRotationMode;
	} else {
		RequestedRotationMode = RotationMode;
	}
	Internal_SetStrafe();
}

void UACFCharacterMovementComponent::OnRep_IsAiming()
{
	if (FirstPersonComponent) {
		FirstPersonComponent->SetAiming(bAiming);
	}
	OnAimChanged.Broadcast(bAiming);
}

void UACFCharacterMovementComponent::Internal_SetStrafe()
{
	bOrientRotationToMovement = !(RotationMode == ERotationMode::EStrafing); // Character moves in the direction of input..
	bUseControllerDesiredRotation = RotationMode == ERotationMode::EStrafing;
	OnRotationModeChanged.Broadcast(RotationMode);
}

void UACFCharacterMovementComponent::SetRotationMode(ERotationMode inRotMode)
{
	if (bRotationModeLocked) {
		// Locked, e.g. in first person: only remember the request, it is applied on unlock.
		RequestedRotationMode = inRotMode;
	} else {
		// Apply immediately on the local machine so the visual change is instant.
		RotationMode = inRotMode;
		Internal_SetStrafe();
	}

	// If we are not the server, send a Server RPC so the authoritative RotationMode
	// matches. Without this the replicated value coming from the server would
	// overwrite the local change on every replication tick, keeping bOrientRotationToMovement
	// and bUseControllerDesiredRotation out of sync and causing continuous CMC corrections.
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_SetRotationMode(inRotMode);
	}
}

void UACFCharacterMovementComponent::Server_SetRotationMode_Implementation(ERotationMode inRotMode)
{
	// Still sent while the owner is locked, so both sides restore the same mode on unlock.
	if (bRotationModeLocked) {
		RequestedRotationMode = inRotMode;
		return;
	}

	RotationMode = inRotMode;
	Internal_SetStrafe();
}

void UACFCharacterMovementComponent::LockRotationMode(ERotationMode inRotMode)
{
	if (!bRotationModeLocked) {
		RequestedRotationMode = RotationMode;
	}

	bRotationModeLocked = true;
	LockedRotationMode = inRotMode;
	RotationMode = inRotMode;
	Internal_SetStrafe();
}

void UACFCharacterMovementComponent::UnlockRotationMode()
{
	if (!bRotationModeLocked) {
		return;
	}

	bRotationModeLocked = false;
	RotationMode = RequestedRotationMode;
	Internal_SetStrafe();
}

void UACFCharacterMovementComponent::HandleFirstPersonChanged(bool bIsInFirstPerson)
{
	// First person always faces the camera. Whatever the rotation mode was asked to be meanwhile comes back on leaving it.
	if (bIsInFirstPerson) {
		LockRotationMode(ERotationMode::EStrafing);
	} else {
		UnlockRotationMode();
	}
}

void UACFCharacterMovementComponent::ResetStrafeMovement()
{
	SetRotationMode(defaultRotMode);
}

void UACFCharacterMovementComponent::ActivateLocomotionStance_Implementation(FGameplayTag locStance)
{
	if (currentMovestance == locStance) {
		return;
	}

	if (currentMovestance != FGameplayTag()) {
		DeactivateCurrentLocomotionStance();
	}

	currentMovestance = locStance;
	// UACFRPGFunctionLibrary::AddGameplayTagToActor(GetOwner(), currentMovestance);
	OnLocomotionStanceChanged.Broadcast(currentMovestance);
}

void UACFCharacterMovementComponent::DeactivateLocomotionStance_Implementation(FGameplayTag locStance)
{
	if (currentMovestance == locStance) {
		DeactivateCurrentLocomotionStance();
	}
}

void UACFCharacterMovementComponent::DeactivateCurrentLocomotionStance_Implementation()
{
	//  UACFRPGFunctionLibrary::RemoveGameplayTagFromActor(GetOwner(), currentMovestance);
	currentMovestance = FGameplayTag();

	OnLocomotionStanceChanged.Broadcast(currentMovestance);
}

void UACFCharacterMovementComponent::AccelerateToNextState()
{
	LocomotionStates.Sort();

	const int32 actualindex = LocomotionStates.IndexOfByKey(currentLocomotionState);

	if (LocomotionStates.IsValidIndex(actualindex + 1)) {
		SetLocomotionState(LocomotionStates[actualindex + 1].State);
	}
}

void UACFCharacterMovementComponent::BrakeToPreviousState()
{
	LocomotionStates.Sort();

	const int32 actualindex = LocomotionStates.IndexOfByKey(currentLocomotionState);

	if (LocomotionStates.IsValidIndex(actualindex - 1)) {
		SetLocomotionState(LocomotionStates[actualindex - 1].State);
	}
}

void UACFCharacterMovementComponent::TurnAtRate(float Rate)
{
	if (Character) {
		Character->AddControllerYawInput(Rate * TurnRate * GetWorld()->GetDeltaSeconds());
	}
}

void UACFCharacterMovementComponent::LookUpAtRate(float Rate)
{
	if (Character) {
		Character->AddControllerPitchInput(Rate * LookUpRate * GetWorld()->GetDeltaSeconds());
	}
}



void UACFCharacterMovementComponent::TurnAtRateLocal(float Value)
{
	if (Value != 0 && Character && Character->Controller) {
		const FRotator Rotation = Character->Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		const float RotationValue = Value * GetRotationRateYaw() * UGameplayStatics::GetWorldDeltaSeconds(this);
		const FRotator rotationDir(0, Rotation.Yaw + RotationValue, 0);
		Direction = rotationDir.RotateVector(Direction);

		AddInputVector(Direction * Value, false);
	}
}

void UACFCharacterMovementComponent::MoveForward(float Value)
{
	if (!bCanMove) {
		return;
	}
	// catch the forward axis

	MoveForwardAxis = Value;

	if (Character && Character->Controller && (MoveForwardAxis != 0.0f)) {
		// find out which way is forward
		const FRotator Rotation = Character->Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// When climbing, use the climbing surface-aligned forward direction
		const FVector Direction = (ClimbingComponent && ClimbingComponent->IsClimbing()) ?
			ClimbingComponent->GetClimbingForwardDirection() :
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		AddInputVector(Direction * Value, false);
	}
}

void UACFCharacterMovementComponent::MoveRight(float Value)
{
	if (!bCanMove)
		return;

	// Catch the right axis

	MoveRightAxis = Value;

	if (Character && Character->Controller && (MoveRightAxis != 0.0f)) {
		// find out which way is right
		const FRotator Rotation = Character->Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// When climbing, use the climbing surface-aligned right direction
		const FVector Direction = (ClimbingComponent && ClimbingComponent->IsClimbing()) ?
			ClimbingComponent->GetClimbingRightDirection() :
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement in that direction
		Character->AddMovementInput(Direction, Value);
	}
}

void UACFCharacterMovementComponent::MoveUp(float Value)
{
	if (!bCanMove)
		return;

	// Catch the right axis

	MoveUpAxis = Value;

	if (Character && Character->Controller && (MoveUpAxis != 0.0f)) {
		// get up vector
		const FVector Direction = FVector(0.f, 0.f, 1.f);
		// add movement in that direction
		Character->AddMovementInput(Direction, Value);
	}
}

FVector UACFCharacterMovementComponent::GetWorldMovementInputVector()
{
	if (Character) {
		const FVector localDir = FVector(MoveForwardAxis, MoveRightAxis, 0.f);

		FVector WorlDir = Character->GetActorForwardVector().Rotation().RotateVector(localDir);
		WorlDir.Normalize();
		return WorlDir;
	}
	return FVector();
}

FVector UACFCharacterMovementComponent::GetCameraMovementInputVector()
{
	if (Character) {
		const FVector localDir = FVector(MoveForwardAxis, MoveRightAxis, 0.f);
		if (localDir.IsNearlyZero()) {
			return FVector::ZeroVector;
		}
		FVector WorlDir = Character->GetController()->GetControlRotation().RotateVector(localDir);
		WorlDir.Normalize();
		return WorlDir;
	}
	return FVector();
}

FVector UACFCharacterMovementComponent::GetCurrentInputVector()
{
	FVector dir = FVector(MoveForwardAxis, MoveRightAxis, 0.f);
	dir.Normalize();
	return dir;
}

EACFDirection UACFCharacterMovementComponent::GetCurrentInputDirection(bool bUseEightDirections)
{
	if (Character) {
		const FVector direction = GetCurrentInputVector();
		if (direction.IsNearlyZero()) {
			return EACFDirection::Front;
		}

		const float angle = FMath::RadiansToDegrees(FMath::Atan2(direction.Y, direction.X));
		const float absAngle = FMath::Abs(angle);
		const bool bIsRightSide = angle > 0.f;

		if (!bUseEightDirections) {
			if (absAngle <= 45.f) {
				return EACFDirection::Front;
			}
			if (absAngle >= 135.f) {
				return EACFDirection::Back;
			}
			return bIsRightSide ? EACFDirection::Right : EACFDirection::Left;
		}

		if (absAngle <= 22.5f) {
			return EACFDirection::Front;
		}
		if (absAngle < 67.5f) {
			return bIsRightSide ? EACFDirection::FrontRight : EACFDirection::FrontLeft;
		}
		if (absAngle <= 112.5f) {
			return bIsRightSide ? EACFDirection::Right : EACFDirection::Left;
		}
		if (absAngle < 157.5f) {
			return bIsRightSide ? EACFDirection::BackRight : EACFDirection::BackLeft;
		}
		return EACFDirection::Back;
	}
	return EACFDirection::Front;
}

void UACFCharacterMovementComponent::ResetToDefaultLocomotionState()
{
	SetLocomotionState(DefaultState);
}

void UACFCharacterMovementComponent::UpdateMaxSpeed(ELocomotionState State)
{
	FACFLocomotionState* locState = LocomotionStates.FindByKey(State);

	if (locState) {
		targetLocomotionState = *(locState);
		MaxWalkSpeed = GetCharacterMaxSpeedByState(State);
		MaxSwimSpeed = GetCharacterMaxSwimSpeedByState(State);
		targetLocomotionState.MaxStateSpeed = GetCharacterMaxSpeedByState(State);

	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Locomotion State inexistent"));
	}
}

// --- Climbing Integration ---

float UACFCharacterMovementComponent::GetMaxSpeed() const
{
	if (ClimbingComponent && ClimbingComponent->IsClimbing()) {
		return ClimbingComponent->GetMaxClimbingSpeed();
	}
	return Super::GetMaxSpeed();
}

float UACFCharacterMovementComponent::GetMaxAcceleration() const
{
	if (ClimbingComponent && ClimbingComponent->IsClimbing()) {
		return ClimbingComponent->GetMaxClimbingAcceleration();
	}
	if (IsSwimming()) {
		return MaxSwimAcceleration;
	}
	return Super::GetMaxAcceleration();
}

float UACFCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (IsSwimming()) {
		return MaxSwimBrakingDeceleration;
	}
	return Super::GetMaxBrakingDeceleration();
}

void UACFCharacterMovementComponent::SetMaxFallingSpeedOverride(float NewMaxFallingSpeed)
{
	MaxFallingSpeedOverride = FMath::Max(0.f, NewMaxFallingSpeed);
}

FVector UACFCharacterMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	FVector Result = Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);

	if (MaxFallingSpeedOverride > 0.f && Result.Z < -MaxFallingSpeedOverride)
	{
		Result.Z = -MaxFallingSpeedOverride;
	}

	return Result;
}

void UACFCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	// The engine only calls PhysicsRotation outside anim root motion, so lock-on rotation never
	// fights motion warping, and the rotation applied here travels with the replicated move.
	UpdateTargetComponentRef();
	if (IsOrientingToLockOnTarget()) {
		Internal_RotateTowardsLockOnTarget(DeltaTime);
		return;
	}

	if (IsSwimming()) {
		const FRotator SavedRotationRate = RotationRate;
		RotationRate = SwimRotationRate;
		Super::PhysicsRotation(DeltaTime);
		RotationRate = SavedRotationRate;
		return;
	}

	Super::PhysicsRotation(DeltaTime);
}

void UACFCharacterMovementComponent::UpdateTargetComponentRef()
{
	AController* controller = CharacterOwner ? CharacterOwner->GetController() : nullptr;
	if (controller == cachedTargetController.Get()) {
		return;
	}

	cachedTargetController = controller;
	TargetComponent = controller ? controller->FindComponentByClass<UATSBaseTargetComponent>() : nullptr;
}

bool UACFCharacterMovementComponent::IsOrientingToLockOnTarget() const
{
	// Forward facing keeps orienting to movement, so lock-on must not touch it.
	if (!bOrientToLockOnTarget || RotationMode != ERotationMode::EStrafing) {
		return false;
	}

	// Climbing and custom movement modes drive rotation themselves.
	if (MovementMode == MOVE_Custom || (ClimbingComponent && ClimbingComponent->IsClimbing())) {
		return false;
	}

	return TargetComponent && TargetComponent->GetIsLockOnActive();
}

void UACFCharacterMovementComponent::Internal_RotateTowardsLockOnTarget(float DeltaTime)
{
	if (!HasValidData() || !UpdatedComponent) {
		return;
	}

	// Game Thread, so the live target location is fine; the cached one exists for anim threads.
	FVector toTarget = TargetComponent->GetCurrentTargetPointLocation() - UpdatedComponent->GetComponentLocation();
	toTarget.Z = 0.f;
	if (toTarget.IsNearlyZero()) {
		return;
	}

	const FRotator currentRotation = UpdatedComponent->GetComponentRotation();
	const float desiredYaw = FRotator::NormalizeAxis(toTarget.Rotation().Yaw);

	constexpr float AngleTolerance = 1e-3f;
	if (FMath::IsNearlyEqual(currentRotation.Yaw, desiredYaw, AngleTolerance)) {
		return;
	}

	// GetRotationRateYaw already accounts for SwimRotationRate. Clamping mirrors the engine's
	// GetAxisDeltaRotation, where a negative rate means "snap instantly".
	const float rotationRateYaw = GetRotationRateYaw();
	const float maxDeltaYaw = (rotationRateYaw >= 0.f) ? FMath::Min(rotationRateYaw * DeltaTime, 360.f) : 360.f;

	// Yaw only: pitch and roll are left untouched so the body never tilts, not even while swimming.
	FRotator newRotation = currentRotation;
	newRotation.Yaw = FMath::FixedTurn(currentRotation.Yaw, desiredYaw, maxDeltaYaw);

	MoveUpdatedComponent(FVector::ZeroVector, newRotation, /*bSweep*/ false);
}

void UACFCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (ClimbingComponent && CustomMovementMode == EACFCustomMovementMode::CMOVE_Climbing) {
		ClimbingComponent->PhysClimbing(DeltaTime, Iterations);
	}

	if (CustomMovementMode == (uint8)EACFCustomMovementMode::CMOVE_Ladder)
	{
		if (!CharacterOwner || !UpdatedComponent)
		{
			return;
		}

		// Root motion is extracted automatically
		if (RootMotionParams.bHasRootMotion)
		{
			const FTransform RootMotionDelta =
				RootMotionParams.GetRootMotionTransform();

			// Apply TRANSLATION only
			MoveUpdatedComponent(
				RootMotionDelta.GetTranslation(),
				UpdatedComponent->GetComponentQuat(),
				true
			);
		}

		// Prevent physics from interfering
		Velocity = FVector::ZeroVector;
		GravityScale = 0.f;

		return;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

void UACFCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	if (ClimbingComponent) {
		ClimbingComponent->HandleMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	}

	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

void UACFCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (ClimbingComponent) {
		ClimbingComponent->HandleMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (GetOwner() && GetOwner()->HasAuthority()) {
		const bool bNowClimbing = MovementMode == EMovementMode::MOVE_Custom
			&& CustomMovementMode == EACFCustomMovementMode::CMOVE_Climbing;
		SetReplicatedClimbingState(bNowClimbing);
	}

	OnMovementModeChangedEvent.Broadcast(MovementMode);
}

void UACFCharacterMovementComponent::StartNewPhysicsForClimbing(float DeltaTime, int32 Iterations)
{
	StartNewPhysics(DeltaTime, Iterations);
}

void UACFCharacterMovementComponent::RestorePreAdditiveRootMotionVelocityForClimbing()
{
	RestorePreAdditiveRootMotionVelocity();
}

void UACFCharacterMovementComponent::ApplyRootMotionToVelocityForClimbing(float DeltaTime)
{
	ApplyRootMotionToVelocity(DeltaTime);
}

void UACFCharacterMovementComponent::HandleImpactForClimbing(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	HandleImpact(Hit, TimeSlice, MoveDelta);
}

float UACFCharacterMovementComponent::SlideAlongSurfaceForClimbing(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	return SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

void UACFCharacterMovementComponent::HandleClimbingStateChanged(bool bNewIsClimbing)
{
	if (!animInstance && Character && Character->GetMesh()) {
		animInstance = Cast<UACFAnimInstance>(Character->GetMesh()->GetAnimInstance());
	}

	if (!animInstance) {
		return;
	}
	if (bNewIsClimbing) {
		animInstance->ActivateClimbingLayer();
	}
	else {
		animInstance->DeactivateClimbingLayer();
	}
}