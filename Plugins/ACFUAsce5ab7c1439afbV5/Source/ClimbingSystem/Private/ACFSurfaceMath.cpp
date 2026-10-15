// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFSurfaceMath.h"

bool ACFSurfaceMath::SegmentIntersectsAABB(const FVector& Start, const FVector& End, const FBox& Box, float& OutEntryDistance)
{
	OutEntryDistance = 0.0f;
	
	if (!Box.IsValid) return false;
	
	const FVector Direction = End - Start;
	const double SegmentLength = Direction.Size();
	
	if (SegmentLength <= UE_KINDA_SMALL_NUMBER) return false;
	
	double EntryT = 0.0;
	double ExitT = 1.0;
	
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double StartValue = Start[Axis];
		const double DirectionValue = Direction[Axis];
		const double MinValue = Box.Min[Axis];
		const double MaxValue = Box.Max[Axis];
		
		if (FMath::Abs(DirectionValue) <= UE_KINDA_SMALL_NUMBER)
		{
			if (StartValue < MinValue || StartValue > MaxValue) return false;
			continue;
		}
		
		double AxisEntryT = (MinValue - StartValue) / DirectionValue;
		double AxisExitT = (MaxValue - StartValue) / DirectionValue;
		if (AxisEntryT > AxisExitT) Swap(AxisEntryT, AxisExitT);
		
		EntryT = FMath::Max(EntryT, AxisEntryT);
		ExitT = FMath::Min(ExitT, AxisExitT);
		
		if (EntryT > ExitT) return false;
	}
	
	OutEntryDistance = static_cast<float>(EntryT * SegmentLength);
	
	return true;
}

float ACFSurfaceMath::ClosestSegmentSegment(const FVector& P1, const FVector& Q1, const FVector& P2,
                                              const FVector& Q2, FVector& OutC1, FVector& OutC2, float& OutT1)
{
	const FVector D1 = Q1 - P1;
	const FVector D2 = Q2 - P2;
	const FVector R = P1 - P2;

	const float A = D1.SizeSquared();
	const float E = D2.SizeSquared();
	const float F = FVector::DotProduct(D2, R);

	float S = 0.0f;
	float T = 0.0f;

	if (A <= KINDA_SMALL_NUMBER && E <= KINDA_SMALL_NUMBER)
	{
		OutC1 = P1;
		OutC2 = P2;
		OutT1 = 0.0f;
		return FVector::DistSquared(OutC1, OutC2);
	}

	if (A <= KINDA_SMALL_NUMBER)
	{
		S = 0.0f;
		T = FMath::Clamp(F / E, 0.0f, 1.0f);
	}
	else
	{
		const float C = FVector::DotProduct(D1, R);
		if (E <= KINDA_SMALL_NUMBER)
		{
			T = 0.0f;
			S = FMath::Clamp(-C / A, 0.0f, 1.0f);
		}
		else
		{
			const float B = FVector::DotProduct(D1, D2);
			const float Denom = A * E - B * B;

			if (Denom != 0.0f)
			{
				S = FMath::Clamp((B * F - C * E) / Denom, 0.0f, 1.0f);
			}

			T = (B * S + F) / E;

			if (T < 0.0f)
			{
				T = 0.0f;
				S = FMath::Clamp(-C / A, 0.0f, 1.0f);
			}
			else if (T > 1.0f)
			{
				T = 1.0f;
				S = FMath::Clamp((B - C) / A, 0.0f, 1.0f);
			}
		}
	}

	OutC1 = P1 + D1 * S;
	OutC2 = P2 + D2 * T;
	OutT1 = S;
	return FVector::DistSquared(OutC1, OutC2);
}

float ACFSurfaceMath::ClosestSegmentTriangle(const FVector& Start, const FVector& End, const FVector& P0,
	const FVector& P1, const FVector& P2, FVector& OutSegmentPoint, FVector& OutTrianglePoint, float& OutSegmentT)
{
	FVector IntersectPoint;
        FVector IntersectNormal;
        if (FMath::SegmentTriangleIntersection(Start, End, P0, P1, P2, IntersectPoint, IntersectNormal))
        {
            const FVector Segment = End - Start;
            const float SegmentSizeSq = Segment.SizeSquared();
            OutSegmentT = SegmentSizeSq > KINDA_SMALL_NUMBER
                ? FMath::Clamp(FVector::DotProduct(IntersectPoint - Start, Segment) / SegmentSizeSq, 0.0f, 1.0f)
                : 0.0f;
            OutSegmentPoint = IntersectPoint;
            OutTrianglePoint = IntersectPoint;
            return 0.0f;
        }

        float BestDistSq = TNumericLimits<float>::Max();
        OutSegmentPoint = Start;
        OutTrianglePoint = P0;
        OutSegmentT = 0.0f;

        auto TestPointToTriangle = [&](const FVector& Point, float SegmentT)
        {
            const FVector ClosestTrianglePoint = FMath::ClosestPointOnTriangleToPoint(Point, P0, P1, P2);
            const float DistSq = FVector::DistSquared(Point, ClosestTrianglePoint);
            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
                OutSegmentPoint = Point;
                OutTrianglePoint = ClosestTrianglePoint;
                OutSegmentT = SegmentT;
            }
        };

        TestPointToTriangle(Start, 0.0f);
        TestPointToTriangle(End, 1.0f);

        auto TestSegmentEdge = [&](const FVector& EdgeStart, const FVector& EdgeEnd)
        {
            FVector SegmentPoint;
            FVector EdgePoint;
            float SegmentT = 0.0f;
            const float DistSq = ClosestSegmentSegment(Start, End, EdgeStart, EdgeEnd, SegmentPoint, EdgePoint, SegmentT);
            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
                OutSegmentPoint = SegmentPoint;
                OutTrianglePoint = EdgePoint;
                OutSegmentT = SegmentT;
            }
        };

        TestSegmentEdge(P0, P1);
        TestSegmentEdge(P1, P2);
        TestSegmentEdge(P2, P0);

        return BestDistSq;
}

float ACFSurfaceMath::GetSphereContactRadius(float Radius)
{
	const float SafeRadius = FMath::Max(Radius, 0.0f);
	if (SafeRadius <= KINDA_SMALL_NUMBER) return 0.0f;
	
	const float ContactTolerance = FMath::Max(0.001f, SafeRadius * 0.0001f);
	
	return SafeRadius + ContactTolerance;
}

bool ACFSurfaceMath::SweepSphereTriangleTOI(const FVector& Start, const FVector& End, float Radius, const FVector& P0, const FVector& P1,
	const FVector& P2, float& OutHitT, FVector& OutSphereCenter, FVector& OutImpactPoint)
{
	OutHitT = 0.0f;
	OutSphereCenter = Start;
	OutImpactPoint = FVector::ZeroVector;
	
	const FVector Segment = End - Start;
	const float SegmentSizeSq = Segment.SizeSquared();
	
	const float SafeRadius = FMath::Max(Radius, 0.0f);
	if (SafeRadius <= KINDA_SMALL_NUMBER)
	{
		if (SegmentSizeSq <= FMath::Square(KINDA_SMALL_NUMBER)) return false;
		
		FVector IntersectPoint;
		FVector IntersectNormal;
		if (!FMath::SegmentTriangleIntersection(Start, End, P0, P1, P2, IntersectPoint, IntersectNormal)) return false;
		
		OutHitT = FMath::Clamp(FVector::DotProduct(IntersectPoint - Start, Segment) / SegmentSizeSq, 0.0f, 1.0f);
		
		OutSphereCenter = IntersectPoint;
		OutImpactPoint = IntersectPoint;
		
		return true;
	}
	
	const float ContactRadius = GetSphereContactRadius(SafeRadius);
	const float ContactRadiusSq = FMath::Square(ContactRadius);
	const auto EvaluateDistanceSq = [&](float T, FVector& OutCenter, FVector& OutPoint)
	{
		OutCenter = FMath::Lerp(Start, End, T);
		OutPoint = FMath::ClosestPointOnTriangleToPoint(OutCenter, P0, P1, P2);
		return FVector::DistSquared(OutCenter, OutPoint);
	};
	
	FVector TestCenter;
	FVector TestPoint;
	if (EvaluateDistanceSq(0.0f, TestCenter, TestPoint) <= ContactRadiusSq)
	{
		OutHitT = 0.0f;
		OutSphereCenter = TestCenter;
		OutImpactPoint = TestPoint;
		
		return true;
	}
	
	if (SegmentSizeSq <= FMath::Square(KINDA_SMALL_NUMBER)) return false;
	
	const float SegmentLength = FMath::Sqrt(SegmentSizeSq);
	
	FVector ClosestOnSegment;
	FVector ClosestOnTriangle;
	float ClosestSegmentT = 0.0f;
	const float MinimumDistanceSq = ClosestSegmentTriangle(Start, End, P0, P1, P2, ClosestOnSegment, ClosestOnTriangle, ClosestSegmentT);
	if (MinimumDistanceSq > ContactRadiusSq) return false;
	
	const float HighBoundT = FMath::Clamp(ClosestSegmentT, 0.0f, 1.0f);
	if (HighBoundT <= 0.0f) return false;
	
	if (EvaluateDistanceSq(HighBoundT, TestCenter, TestPoint) > ContactRadiusSq) return false;
	
	float LowT = 0.0f;
	float HighT = HighBoundT;
	constexpr float TOITolerance = 0.001f;
	
	while ((HighT - LowT) * SegmentLength > TOITolerance)
	{
		const float MidT = (LowT + HighT) * 0.5f;
		if (MidT <= LowT || MidT >= HighT) break;
		
		if (EvaluateDistanceSq(MidT, TestCenter, TestPoint) <= ContactRadiusSq) HighT = MidT;
		else LowT = MidT;
	}
	
	OutHitT = HighT;
	EvaluateDistanceSq(OutHitT, OutSphereCenter, OutImpactPoint);
	
	return true;	
}
