// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "ACFWalkingCollisionComponent.generated.h"

class UBodySetup;

UCLASS()
class CLIMBINGSYSTEM_API UACFWalkingCollisionComponent : public UPrimitiveComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()

public:
	UACFWalkingCollisionComponent();

	virtual bool GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool GetTriMeshSizeEstimates(FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override { return TriangleCount > 0; }
	virtual bool WantsNegXTriMesh() override { return false; }

	virtual UBodySetup* GetBodySetup() override { CreateCollisionBodySetup(); return CollisionBodySetup; }
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	void InitializeCollisionMesh(int32 InTriangleCount, ECollisionChannel InCharacterCollisionChannel, bool bInEnableFootIKCollision, ETraceTypeQuery FootIKChannel);
	void UpdateCollisionVertices(TConstArrayView<FVector> InVertices);
	void ClearCollisionPatch();

private:
	void CreateCollisionBodySetup();

	UPROPERTY(Instanced)
	TObjectPtr<UBodySetup> CollisionBodySetup = nullptr;

	TArray<FVector> CollisionVertices;
	FBoxSphereBounds LocalBounds = FBoxSphereBounds(EForceInit::ForceInit);
	ECollisionChannel CharacterCollisionChannel = ECC_Pawn;
	ECollisionChannel FootIKCollisionChannel = ECC_MAX;
	int32 TriangleCount = 0;
	bool bPatchActive = false;
};
