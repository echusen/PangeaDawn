// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFWalkingCollisionComponent.h"

#include "PhysicsEngine/BodySetup.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UACFWalkingCollisionComponent::UACFWalkingCollisionComponent()
{
	SetMobility(EComponentMobility::Movable);
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	CanCharacterStepUpOn = ECB_Yes;
}

bool UACFWalkingCollisionComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	CollisionData->Vertices.Reserve(CollisionVertices.Num());
	for (const FVector& Vertex : CollisionVertices)
	{
		CollisionData->Vertices.Add(FVector3f(Vertex));
	}

	CollisionData->Indices.Reserve(TriangleCount);
	CollisionData->MaterialIndices.Reserve(TriangleCount);
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		FTriIndices Indices;
		Indices.v0 = TriangleIndex * 3;
		Indices.v1 = Indices.v0 + 1;
		Indices.v2 = Indices.v0 + 2;
		CollisionData->Indices.Add(Indices);
		CollisionData->MaterialIndices.Add(0);
	}

	CollisionData->bFlipNormals = true;
	CollisionData->bDeformableMesh = true;
	CollisionData->bFastCook = true;
	return true;
}

bool UACFWalkingCollisionComponent::GetTriMeshSizeEstimates(FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	OutTriMeshEstimates.VerticeCount = CollisionVertices.Num();
	return true;
}

FBoxSphereBounds UACFWalkingCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds CaculatedBounds = LocalBounds.TransformBy(LocalToWorld);
	CaculatedBounds.BoxExtent *= BoundsScale;
	CaculatedBounds.SphereRadius *= BoundsScale;
	return CaculatedBounds;
}


void UACFWalkingCollisionComponent::InitializeCollisionMesh(int32 InTriangleCount, ECollisionChannel InCharacterCollisionChannel, bool bInEnableFootIKCollision, ETraceTypeQuery FootIKChannel)
{
	TriangleCount = InTriangleCount;
	CharacterCollisionChannel = InCharacterCollisionChannel;
	FootIKCollisionChannel = ECC_MAX;
	if (bInEnableFootIKCollision)
	{
		FootIKCollisionChannel = UEngineTypes::ConvertToCollisionChannel(FootIKChannel);
	}
	CollisionVertices.SetNumUninitialized(TriangleCount * 3);

	const int32 GridWidth = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(TriangleCount)));
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const int32 Column = TriangleIndex % GridWidth;
		const int32 Row = TriangleIndex / GridWidth;
		const FVector BasePosition(Column * 30.0, Row * 30.0, 0.0);
		const int32 VertexBase = TriangleIndex * 3;

		CollisionVertices[VertexBase] = BasePosition;
		CollisionVertices[VertexBase + 1] = BasePosition + FVector(10.0, 0.0, 0.0);
		CollisionVertices[VertexBase + 2] = BasePosition + FVector(0.0, 10.0, 0.0);
	}

	LocalBounds = FBoxSphereBounds(FBox(CollisionVertices));
	CreateCollisionBodySetup();
	CollisionBodySetup->bHasCookedCollisionData = true;
	CollisionBodySetup->InvalidatePhysicsData();
	CollisionBodySetup->CreatePhysicsMeshes();
	RecreatePhysicsState();
}

void UACFWalkingCollisionComponent::UpdateCollisionVertices(TConstArrayView<FVector> InVertices)
{
	FMemory::Memcpy(CollisionVertices.GetData(), InVertices.GetData(), CollisionVertices.Num() * sizeof(FVector));
	LocalBounds = FBoxSphereBounds(FBox(CollisionVertices));
	UpdateBounds();
	BodyInstance.UpdateTriMeshVertices(CollisionVertices);

	if (!bPatchActive)
	{
		SetCollisionResponseToChannel(CharacterCollisionChannel, ECR_Block);
		if (FootIKCollisionChannel < ECC_MAX)
		{
			SetCollisionResponseToChannel(FootIKCollisionChannel, ECR_Block);
		}
		bPatchActive = true;
	}
}

void UACFWalkingCollisionComponent::ClearCollisionPatch()
{
	if (bPatchActive)
	{
		SetCollisionResponseToChannel(CharacterCollisionChannel, ECR_Ignore);
		if (FootIKCollisionChannel < ECC_MAX)
		{
			SetCollisionResponseToChannel(FootIKCollisionChannel, ECR_Ignore);
		}
		bPatchActive = false;
	}
}


void UACFWalkingCollisionComponent::CreateCollisionBodySetup()
{
	if (CollisionBodySetup) return;

	EObjectFlags BodySetupObjectFlags = RF_NoFlags;
	if (IsTemplate()) BodySetupObjectFlags = RF_Public | RF_ArchetypeObject;
	CollisionBodySetup = NewObject<UBodySetup>(this, NAME_None, BodySetupObjectFlags);
	CollisionBodySetup->BodySetupGuid = FGuid::NewGuid();
	CollisionBodySetup->bGenerateMirroredCollision = false;
	CollisionBodySetup->bDoubleSidedGeometry = false;
	CollisionBodySetup->bSupportUVsAndFaceRemap = false;
	CollisionBodySetup->bSupportFaceRemapOnMeshBVH = false;
	CollisionBodySetup->bSupportVertexRemap = true;
	CollisionBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
}
