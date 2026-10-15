// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFFreeClimbTypes.h"
#include "ACFFreeClimbStateTypes.h"
#include "Engine/NetSerialization.h"
#include "ACFFreeClimbNetworkTypes.generated.h"

class AActor;
class UPrimitiveComponent;

USTRUCT()
struct CLIMBINGSYSTEM_API FACFFreeClimbRepState
{
	GENERATED_BODY()
	
	UPROPERTY()
	uint16 SessionId = 0;
	
	UPROPERTY()
	uint16 Sequence = 0;
	
	UPROPERTY()
	float ServerTimeSeconds = 0.0f;
	
	UPROPERTY()
	EACFFreeClimbState State = EACFFreeClimbState::Idle;
	
	UPROPERTY()
	TObjectPtr<AActor> ClimbableActor = nullptr;
	
	UPROPERTY()
	int32 AnchorTriangleIndex = INDEX_NONE;
	
	UPROPERTY()
	FVector AnchorBarycentric = FVector::ZeroVector;
	
	UPROPERTY()
	FQuat ClimbRotation = FQuat::Identity;

	UPROPERTY()
	EACFSurfaceSource Source = EACFSurfaceSource::Skeletal;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> StaticPrimitive = nullptr;

	UPROPERTY()
	FVector_NetQuantize100 StaticLocalPosition = FVector::ZeroVector;

	UPROPERTY()
	FVector StaticLocalNormal = FVector::ForwardVector;
	
	UPROPERTY()
	FVector2D MoveInput = FVector2D::ZeroVector;
	
	UPROPERTY()
	float StateStartServerTimeSeconds  = 0.0f;
	
	// Cling
	UPROPERTY()
	bool bHasAnchorRelativeLocation = false;
	
	UPROPERTY()
	FVector_NetQuantize100 AnchorRelativeLocation = FVector::ZeroVector;
	
	UPROPERTY()
	bool bExitToWalking = false;

	/** Velocity the character is released with when the climb ended in a drop, zero otherwise. Carried
	 * so the authority and the simulated proxies start the same fall as the owning client. */
	UPROPERTY()
	FVector_NetQuantize100 DropVelocity = FVector::ZeroVector;
};

USTRUCT()
struct CLIMBINGSYSTEM_API FACFJumpPathPoint
{
	GENERATED_BODY()

	UPROPERTY()
	EACFSurfaceSource Source = EACFSurfaceSource::Skeletal;
	
	UPROPERTY()
	TObjectPtr<AActor> ClimbableActor = nullptr;
	
	UPROPERTY()
	int32 TriangleIndex = INDEX_NONE;
	
	UPROPERTY()
	FVector Barycentric = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> StaticPrimitive = nullptr;

	UPROPERTY()
	FVector StaticLocalPosition = FVector::ZeroVector;

	UPROPERTY()
	FVector StaticLocalNormal = FVector::ForwardVector;
	
	bool IsValid() const
	{
		return Source == EACFSurfaceSource::Skeletal
			? ClimbableActor != nullptr && TriangleIndex != INDEX_NONE
			: StaticPrimitive != nullptr;
	}
};

USTRUCT()
struct CLIMBINGSYSTEM_API FACFJumpRepState
{
	GENERATED_BODY()
	
	UPROPERTY()
	FACFFreeClimbRepState CommonState;
	
	UPROPERTY()
	TArray<FACFJumpPathPoint> Path;
	
	UPROPERTY()
	float Duration = 0.0f;
};
