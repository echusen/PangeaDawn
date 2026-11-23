// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbNetworkRuntime.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

namespace ACFFreeClimbNetwork
{
	bool IsSequenceNewer(uint16 A, uint16 B) { return A != B && static_cast<int16>(A - B) > 0; }

	double GetServerTimeSeconds(const UWorld* World)
	{
		if (!World) return 0.0;
		
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		
		return World->GetTimeSeconds();
	}

	FQuat BuildAnchorFrameRotation(const FVector& AnchorNormal)
	{
		const FVector Normal = AnchorNormal.GetSafeNormal();
		if (Normal.IsNearlyZero()) return FQuat::Identity;
		
		FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Normal);
		if (!Up.Normalize())
		{
			Up = FVector::VectorPlaneProject(FVector::ForwardVector, Normal).GetSafeNormal();
		}
		
		return FRotationMatrix::MakeFromXZ(-Normal, Up).ToQuat().GetNormalized();
	}

	FVector MakeAnchorRelativeLocation(const FVector& WorldLocation, const FVector& AnchorPoint, const FVector& AnchorNormal)
	{
		const FQuat AnchorRotation = BuildAnchorFrameRotation(AnchorNormal);
		return AnchorRotation.UnrotateVector(WorldLocation - AnchorPoint);
	}

	FVector MakeWorldLocationFromAnchor(const FVector& RelativeLocation, const FVector& AnchorPoint, const FVector& AnchorNormal)
	{
		const FQuat AnchorRotation = BuildAnchorFrameRotation(AnchorNormal);
		return AnchorPoint + AnchorRotation.RotateVector(RelativeLocation);
	}
}
