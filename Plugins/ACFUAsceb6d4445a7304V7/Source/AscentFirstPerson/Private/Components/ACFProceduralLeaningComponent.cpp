// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralLeaningComponent.h"

#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#include "DrawDebugHelpers.h"

#include "Utility/ACFFirstPersonFunctionLibrary.h"

static TAutoConsoleVariable<int32> CVarLeaningDrawDebug(
	TEXT("ACF.FirstPerson.Debug.Leaning"),
	0,
	TEXT("Draw the lean wall sweep\n<=0: off, 1: on"),
	ECVF_Default);

namespace
{
	/** Used instead of LeanInterpSpeed when a wall forces the lean back in, so the head never clips. */
	constexpr float LeanRetractionSpeed = 20.0f;

	/** Keeps the sweep off the wall surface. Fraction of the trace length. */
	constexpr float LeanWallSafetyBuffer = 0.1f;

	/** Approximates the downward arc of the neck without solving it. */
	constexpr float LeanTraceDropOffset = 10.0f;
}

UACFProceduralLeaningComponent::UACFProceduralLeaningComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxLeanAngle = 45.0f;
	LeanInterpSpeed = 5.0f;
	MaxLeanDistance = 40.0f;
	TraceChannel = ECC_Visibility;

	CurrentLeanAngle = 0.0f;
	TargetLeanDirection = 0;
	bIsHoldingLeft = false;
	bIsHoldingRight = false;

	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UACFProceduralLeaningComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UACFProceduralLeaningComponent, TargetLeanDirection, COND_SkipOwner);
}

void UACFProceduralLeaningComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	float TargetAngle = (float)TargetLeanDirection * MaxLeanAngle;

	if (TargetLeanDirection != 0)
	{
		TargetAngle = GetWallClampedTargetAngle(TargetAngle);
	}

	float CurrentInterpSpeed = LeanInterpSpeed;

	if (TargetLeanDirection != 0 && FMath::Abs(CurrentLeanAngle) > (FMath::Abs(TargetAngle) + KINDA_SMALL_NUMBER))
	{
		CurrentInterpSpeed = LeanRetractionSpeed;
	}

	CurrentLeanAngle = FMath::FInterpTo(CurrentLeanAngle, TargetAngle, DeltaTime, CurrentInterpSpeed);
}

void UACFProceduralLeaningComponent::InputLeanLeft(bool bPressed)
{
	ResolveLeanState(true, bPressed);

	Server_SetLeanLeft(bPressed);
}

void UACFProceduralLeaningComponent::InputLeanRight(bool bPressed)
{
	ResolveLeanState(false, bPressed);

	Server_SetLeanRight(bPressed);
}

ACharacter* UACFProceduralLeaningComponent::GetOwnerCharacter() const
{
	return GetOwner<ACharacter>();
}

void UACFProceduralLeaningComponent::ResolveLeanState(bool bIsLeftInput, bool bPressed)
{
	if (bIsLeftInput) bIsHoldingLeft = bPressed;
	else bIsHoldingRight = bPressed;

	// Smart logic: Last pressed wins, but releasing falls back to held
	if (bPressed)
	{
		// If we just pressed something, that takes priority
		TargetLeanDirection = bIsLeftInput ? -1 : 1;
	}
	else
	{
		// We released a key. Check if the other is still held.
		if (bIsLeftInput)
		{
			// Released Left. If Right is held, go Right (1), else Stop (0).
			TargetLeanDirection = bIsHoldingRight ? 1 : 0;
		}
		else
		{
			// Released Right. If Left is held, go Left (-1), else Stop (0).
			TargetLeanDirection = bIsHoldingLeft ? -1 : 0;
		}
	}
}

void UACFProceduralLeaningComponent::Server_SetLeanLeft_Implementation(bool bPressed)
{
	ResolveLeanState(true, bPressed);
}

void UACFProceduralLeaningComponent::Server_SetLeanRight_Implementation(bool bPressed)
{
	ResolveLeanState(false, bPressed);
}

float UACFProceduralLeaningComponent::GetWallClampedTargetAngle(float DesiredTargetAngle) const
{
	const ACharacter* OwnerChar = GetOwnerCharacter();
	if (!OwnerChar) return DesiredTargetAngle;

	const UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent();
	const USkeletalMeshComponent* Mesh = OwnerChar->GetMesh();
	if (!Capsule || !Mesh) return DesiredTargetAngle;

	const FRotator CameraRotation = UACFFirstPersonFunctionLibrary::GetCameraRotation(OwnerChar);
	const FVector RightVec = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
	const FVector LeanDir = RightVec * FMath::Sign(DesiredTargetAngle);

	// Capsule for X/Y (stable centre), head bone for Z (tracks crouch/stand without extra state).
	FVector TraceStart = Capsule->GetComponentLocation();

	if (Mesh->DoesSocketExist(HeadBoneName))
	{
		const FVector HeadSocketLocation = Mesh->GetSocketLocation(HeadBoneName);
		TraceStart.Z = HeadSocketLocation.Z;
	}
	else
	{
		TraceStart.Z += OwnerChar->BaseEyeHeight;
	}

	FVector TraceEnd = TraceStart + (LeanDir * MaxLeanDistance);
	TraceEnd.Z -= LeanTraceDropOffset;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerChar);

	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(CameraTraceRadius),
		Params
	);

#if ENABLE_DRAW_DEBUG
	if (CVarLeaningDrawDebug.GetValueOnGameThread() > 0)
	{
		const FVector DebugLocation = bHit ? Hit.Location : TraceEnd;
		const FColor DebugColor = bHit ? FColor::Red : FColor::Green;

		DrawDebugSphere(GetWorld(), DebugLocation, CameraTraceRadius, 12, DebugColor);
		DrawDebugLine(GetWorld(), TraceStart, DebugLocation, DebugColor);
	}
#endif

	if (!bHit)
	{
		return DesiredTargetAngle;
	}

	// Hit.Time is the fraction of the sweep travelled, so it scales the angle directly.
	return DesiredTargetAngle * FMath::Clamp(Hit.Time - LeanWallSafetyBuffer, 0.0f, 1.0f);
}
