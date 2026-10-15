// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ACFFreeClimbTypes.h"
#include "ACFWalkingComponent.generated.h"

class ACharacter;
class UACFAnimInstance;
class AACFWalkingCollisionProxy;
class UACFFreeClimbableComponent;
class UACFFreeClimbableWorldSubsystem;
class UACFWalkingCollisionComponent;
class USkeletalMeshComponent;
class USpringArmComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnACFWalkingOnClimbableSurfaceChanged, bool, bIsWalkingOnClimbableSurface);

UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class CLIMBINGSYSTEM_API UACFWalkingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UACFWalkingComponent();

	/** True while grounded on this character's generated climbable walking surface. */
	UFUNCTION(BlueprintPure, Category = "ACF|Walking")
	bool IsWalkingOnClimbableSurface() const;

	/** Fires locally only when entering or leaving the generated walking surface.
	 *  Read IsWalkingOnClimbableSurface once after binding to initialize late subscribers. */
	UPROPERTY(BlueprintAssignable, Category = "ACF|Walking")
	FOnACFWalkingOnClimbableSurfaceChanged OnWalkingOnClimbableSurfaceChanged;

	UFUNCTION(BlueprintCallable, Category = "ACF|Walking")
	void SetDisableFootPlacementOnClimbableSurface(bool bDisable);

	void PrimeCollisionPatch(bool bEnable) { bPrimeCollisionPatch = bEnable; }

private:
	// A source triangle resolved in world space.
	struct FResolvedTriangle
	{
		TWeakObjectPtr<UACFFreeClimbableComponent> Climbable;
		int32 TriangleIndex = INDEX_NONE;
		FVector VerticesWS[3];
	};

	// Maps a patch triangle back to its source.
	struct FPatchTriangleSource
	{
		TWeakObjectPtr<UACFFreeClimbableComponent> Climbable;
		int32 TriangleIndex = INDEX_NONE;
	};

	// Tracks one floor point across surface poses.
	struct FFloorAnchor
	{
		TWeakObjectPtr<UACFFreeClimbableComponent> Climbable;
		int32 TriangleIndex = INDEX_NONE;
		FVector Barycentric = FVector::ZeroVector;
		FTransform PreviousTransform = FTransform::Identity;
	};

	UFUNCTION()
	void OnRep_CollisionProxy();

	UFUNCTION()
	void HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	void RefreshWalkingOnClimbableSurfaceState();
	void UpdateFootIK(bool bWalkingOnSurface);

	void SetSurfaceMeshPrerequisite(USkeletalMeshComponent* NewSurfaceMesh);
	void UpdateCollisionPatch(float DeltaTime);
	bool RebuildCollisionPatch(const FResolvedTriangle* PinnedTriangle);
	bool TryFindFloorAnchor(FFloorAnchor& OutAnchor) const;
	static bool TryBuildTriangleTransform(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& Barycentric, FTransform& OutTransform);

	/** Requires the Foot Placement node to use ACFAnimInstance.GetEnableFootIK for its enable input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Walking", meta = (AllowPrivateAccess = "true"))
	bool bDisableFootPlacementOnClimbableSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (AllowPrivateAccess = "true", DisplayName = "Enable Foot IK Collision", ToolTip = "Allows the generated walking patch to block the selected Foot IK trace channel."))
	bool bEnableFootIKCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (AllowPrivateAccess = "true", EditCondition = "bEnableFootIKCollision"))
	TEnumAsByte<ETraceTypeQuery> FootIKTraceType = TraceTypeQuery1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (ClampMin = "1", UIMin = "1", AllowPrivateAccess = "true"))
	int32 CollisionTriangleCount = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (ClampMin = "0.0", UIMin = "0.0", AllowPrivateAccess = "true"))
	float CapsulePadding = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (AllowPrivateAccess = "true", DisplayName = "Impart Surface Velocity", ToolTip = "Inherit the surface's movement when jumping or falling."))
	bool bImpartSurfaceVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (AllowPrivateAccess = "true"))
	bool bDebugDrawCollisionPatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (AllowPrivateAccess = "true"))
	bool bDebugDrawCapsulePadding = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF", meta = (AllowPrivateAccess = "true", DisplayName = "Tick Spring Arms After Character Movement", ToolTip = "Ensures Spring Arms tick after character movement."))
	bool bTickSpringArmsAfterCharacterMovement = true;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_CollisionProxy)
	TObjectPtr<AACFWalkingCollisionProxy> CollisionProxy = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UACFWalkingCollisionComponent> WalkingCollisionComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UACFFreeClimbableWorldSubsystem> ClimbableSubsystem = nullptr;

	TWeakObjectPtr<USkeletalMeshComponent> SurfaceMeshPrerequisite;
	TWeakObjectPtr<UACFAnimInstance> FootPlacementAnimInstance;
	bool bFootIKDisabledByWalking = false;
	TArray<TWeakObjectPtr<USpringArmComponent>> AdjustedSpringArms;

	TArray<FACFSurfaceHitResult> SweepHits;
	TArray<FVector> PatchVerticesWS;
	TArray<FVector> PatchVerticesLocal;
	TArray<FPatchTriangleSource> PatchTriangleSources;
	int32 ActivePatchTriangleCount = 0;
	bool bPrimeCollisionPatch = false;
	bool bWasWalkingOnClimbableSurface = false;
};
