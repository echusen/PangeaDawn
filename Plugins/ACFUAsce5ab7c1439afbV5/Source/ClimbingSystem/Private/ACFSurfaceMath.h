// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ACFSurfaceMath
{
	bool SegmentIntersectsAABB(
		const FVector& Start,
		const FVector& End,
		const FBox& Box,
		float& OutEntryDistance);
	
	float ClosestSegmentSegment(
		const FVector& P1,
		const FVector& Q1,
		const FVector& P2,
		const FVector& Q2,
		FVector& OutC1,
		FVector& OutC2,
		float& OutT1);

	float ClosestSegmentTriangle(
		const FVector& Start,
		const FVector& End,
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		FVector& OutSegmentPoint,
		FVector& OutTrianglePoint,
		float& OutSegmentT);
	
	float GetSphereContactRadius(float Radius);
	
	bool SweepSphereTriangleTOI(
		const FVector& Start,
		const FVector& End,
		float Radius,
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		float& OutHitT,
		FVector& OutSphereCenter,
		FVector& OutImpactPoint);
}