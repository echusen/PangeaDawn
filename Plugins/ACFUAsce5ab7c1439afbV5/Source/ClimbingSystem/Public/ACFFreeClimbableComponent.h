// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ACFFreeClimbTypes.h"
#include "ACFSurfaceRaycastTypes.h"
#include "ACFFreeClimbableComponent.generated.h"

class UACFSurfaceAsset;
class UACFFreeClimbableWorldSubsystem;
class USkeletalMeshComponent;

UCLASS( ClassGroup=(ACF), meta=(BlueprintSpawnableComponent) )
class CLIMBINGSYSTEM_API UACFFreeClimbableComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UACFFreeClimbableComponent();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    USkeletalMeshComponent* GetSurfaceMeshComponent() const { return MeshComponent; }
	
	bool GetBroadPhaseBounds(FBox& OutBounds) const;

    bool QuerySurface(const FACFSurfaceQuery& Query, FACFSurfaceHitResult& OutHit) const;
	
    bool SweepSphereSurface(const FACFSurfaceSphereSweepQuery& Query, FACFSurfaceHitResult& OutHit) const;

    bool SweepSphereSurfaceMulti(const FACFSurfaceSphereSweepQuery& Query, TArray<FACFSurfaceHitResult>& OutHits) const;

    bool SweepCapsuleSurface(const FACFSurfaceCapsuleSweepQuery& Query, FACFSurfaceCapsuleHitResult& OutHit) const;

    bool SweepCapsuleSurfaceMulti(const FACFSurfaceCapsuleSweepQuery& Query, TArray<FACFSurfaceCapsuleHitResult>& OutHits) const;
	
    bool ResolveAnchor(int32 TriangleIndex, const FVector& Barycentric, FVector& OutPosition, FVector& OutNormal) const;
	
	bool ResolveTriangleFrameRotation(int32 TriangleIndex, FQuat& OutRotation) const;
	
	bool ResolveAnchorLinear(int32 TriangleIndex, const FVector& Barycentric, FVector& OutPosition, FVector& OutNormal) const;

	bool ResolveAnchorBVH(int32 TriangleIndex, const FVector& Barycentric, FVector& OutPosition, FVector& OutNormal) const;

    void ProcessSkinning();

private:
    bool QuerySurfaceLinear(const FACFSurfaceQuery& Query, FACFSurfaceHitResult& OutHit) const;
    bool QuerySurfaceBVH(const FACFSurfaceQuery& Query, FACFSurfaceHitResult& OutHit) const;

    bool SweepSphereSurfaceLinear(const FACFSurfaceSphereSweepQuery& Query, FACFSurfaceHitResult& OutHit) const;
    bool SweepSphereSurfaceBVH(const FACFSurfaceSphereSweepQuery& Query, FACFSurfaceHitResult& OutHit) const;

    bool SweepSphereSurfaceMultiLinear(const FACFSurfaceSphereSweepQuery& Query, TArray<FACFSurfaceHitResult>& OutHits) const;
    bool SweepSphereSurfaceMultiBVH(const FACFSurfaceSphereSweepQuery& Query, TArray<FACFSurfaceHitResult>& OutHits) const;

    bool SweepCapsuleSurfaceLinear(const FACFSurfaceCapsuleSweepQuery& Query, FACFSurfaceCapsuleHitResult& OutHit) const;
    bool SweepCapsuleSurfaceBVH(const FACFSurfaceCapsuleSweepQuery& Query, FACFSurfaceCapsuleHitResult& OutHit) const;

    bool SweepCapsuleSurfaceMultiLinear(const FACFSurfaceCapsuleSweepQuery& Query, TArray<FACFSurfaceCapsuleHitResult>& OutHits) const;
    bool SweepCapsuleSurfaceMultiBVH(const FACFSurfaceCapsuleSweepQuery& Query, TArray<FACFSurfaceCapsuleHitResult>& OutHits) const;

    void DrawDebugSurface();
	void DrawDebugBVHSkinnedTriangles();

private:
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> MeshComponent = nullptr;

	UPROPERTY(EditAnywhere, Category = "ACF")
	TObjectPtr<UACFSurfaceAsset> SurfaceAsset = nullptr;

	TArray<FVector> SkinnedWorldPositions;
	TArray<FVector> SkinnedWorldNormals;

	UPROPERTY(EditAnywhere, Category = "ACF|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugSurface = false;
	
	UPROPERTY(EditAnywhere, Category = "ACF|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugBVHSurface = false;

	mutable FACFSurfaceRaycastRuntimeCache RaycastRuntimeCache;
	mutable uint64 FullSkinningCacheFrame = 0;

	friend class UACFFreeClimbableWorldSubsystem;

public:
	bool ResolveTriangleVertices(int32 TriangleIndex, FVector& OutP0, FVector& OutP1, FVector& OutP2) const;
};
