// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFWalkingComponent.h"
#include "Animation/ACFAnimInstance.h"

#include "ACFFreeClimbableComponent.h"
#include "ClimbingSystem.h"
#include "ACFWalkingCollisionComponent.h"
#include "ACFWalkingCollisionProxy.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "ACFFreeClimbableWorldSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/RotationMatrix.h"
#include "Net/UnrealNetwork.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void UACFWalkingComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (!OwnerCharacter)
	{
		UE_LOG(LogACFFreeClimb, Warning, TEXT("ACFWalkingComponent requires an ACharacter owner. Owner: %s"), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		ClimbableSubsystem = World->GetSubsystem<UACFFreeClimbableWorldSubsystem>();
	}

	if (!ClimbableSubsystem)
	{
		UE_LOG(LogACFFreeClimb, Warning, TEXT("ACFWalkingComponent disabled for '%s': ACFFreeClimbableWorldSubsystem is unavailable."), *GetNameSafe(OwnerCharacter));
		SetComponentTickEnabled(false);
		return;
	}

	UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement();
	CharacterMovement->AddTickPrerequisiteComponent(this);
	// Observe the finalized floor/base after movement, rather than before it in our patch tick.
	OwnerCharacter->OnCharacterMovementUpdated.AddUniqueDynamic(this, &UACFWalkingComponent::HandleCharacterMovementUpdated);

	if (bTickSpringArmsAfterCharacterMovement)
	{
		TInlineComponentArray<USpringArmComponent*> SpringArms(OwnerCharacter);
		const FTickPrerequisite CharacterMovementPrerequisite(CharacterMovement, CharacterMovement->PrimaryComponentTick);
		for (USpringArmComponent* SpringArm : SpringArms)
		{
			if (!SpringArm->PrimaryComponentTick.bCanEverTick) continue;
			// Leave existing camera prerequisites untouched.
			if (SpringArm->PrimaryComponentTick.GetPrerequisites().Contains(CharacterMovementPrerequisite)) continue;

			SpringArm->AddTickPrerequisiteComponent(CharacterMovement);
			AdjustedSpringArms.Add(SpringArm);
		}
	}

}

void UACFWalkingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetSurfaceMeshPrerequisite(nullptr);

	if (OwnerCharacter)
	{
		OwnerCharacter->OnCharacterMovementUpdated.RemoveDynamic(this, &UACFWalkingComponent::HandleCharacterMovementUpdated);
		UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement();
		// Remove only prerequisites added by this component.
		for (const TWeakObjectPtr<USpringArmComponent>& AdjustedSpringArm : AdjustedSpringArms)
		{
			if (USpringArmComponent* SpringArm = AdjustedSpringArm.Get())
			{
				SpringArm->RemoveTickPrerequisiteComponent(CharacterMovement);
			}
		}
		AdjustedSpringArms.Reset();

		CharacterMovement->RemoveTickPrerequisiteComponent(this);

		if (WalkingCollisionComponent)
		{
			for (TActorIterator<ACharacter> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
			{
				ACharacter* OtherCharacter = *CharacterIt;
				if (OtherCharacter == OwnerCharacter) continue;
				if (UACFWalkingComponent* OtherWalking = OtherCharacter->FindComponentByClass<UACFWalkingComponent>())
				{
					if (OtherWalking->WalkingCollisionComponent)
					{
						OwnerCharacter->GetCapsuleComponent()->IgnoreComponentWhenMoving(OtherWalking->WalkingCollisionComponent, false);
						OtherCharacter->GetCapsuleComponent()->IgnoreComponentWhenMoving(WalkingCollisionComponent, false);
					}
				}
			}
		}

		if (OwnerCharacter->HasAuthority() && CollisionProxy)
		{
			CollisionProxy->Destroy();
		}
	}

	WalkingCollisionComponent = nullptr;
	RefreshWalkingOnClimbableSurfaceState();
	Super::EndPlay(EndPlayReason);
}

void UACFWalkingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CollisionProxy && OwnerCharacter->HasAuthority())
	{
		const FTransform SpawnTransform(FQuat::Identity, OwnerCharacter->GetActorLocation());
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = OwnerCharacter;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CollisionProxy = GetWorld()->SpawnActor<AACFWalkingCollisionProxy>(AACFWalkingCollisionProxy::StaticClass(), SpawnTransform, SpawnParameters);
		if (!CollisionProxy)
		{
			UE_LOG(LogACFFreeClimb, Warning, TEXT("ACFWalking disabled for '%s'. Failed to spawn ACFWalkingCollisionProxy."), *GetNameSafe(OwnerCharacter));
			SetComponentTickEnabled(false);
			return;
		}
		OwnerCharacter->ForceNetUpdate();
	}
	if (!CollisionProxy) return;

	if (!WalkingCollisionComponent)
	{
		WalkingCollisionComponent = CollisionProxy->GetWalkingCollisionComponent();
		WalkingCollisionComponent->InitializeCollisionMesh(CollisionTriangleCount, OwnerCharacter->GetCapsuleComponent()->GetCollisionObjectType(), bEnableFootIKCollision, FootIKTraceType);

		for (TActorIterator<ACharacter> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
		{
			ACharacter* OtherCharacter = *CharacterIt;
			if (OtherCharacter == OwnerCharacter) continue;
			if (UACFWalkingComponent* OtherWalking = OtherCharacter->FindComponentByClass<UACFWalkingComponent>())
			{
				if (OtherWalking->WalkingCollisionComponent)
				{
					OwnerCharacter->GetCapsuleComponent()->IgnoreComponentWhenMoving(OtherWalking->WalkingCollisionComponent, true);
					OtherCharacter->GetCapsuleComponent()->IgnoreComponentWhenMoving(WalkingCollisionComponent, true);
				}
			}
		}
	}

	UpdateCollisionPatch(DeltaTime);
	// Also catch proxy initialization/replacement on clients before their next movement update.
	RefreshWalkingOnClimbableSurfaceState();
}

UACFWalkingComponent::UACFWalkingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

bool UACFWalkingComponent::IsWalkingOnClimbableSurface() const
{
	if (!IsValid(OwnerCharacter) || !IsValid(WalkingCollisionComponent)) return false;

	const UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement();
	return CharacterMovement && CharacterMovement->IsMovingOnGround() &&
		CharacterMovement->GetMovementBaseObject() == WalkingCollisionComponent;
}

void UACFWalkingComponent::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	RefreshWalkingOnClimbableSurfaceState();
}

void UACFWalkingComponent::RefreshWalkingOnClimbableSurfaceState()
{
	const bool bIsWalkingOnSurface = IsWalkingOnClimbableSurface();
	UpdateFootIK(bIsWalkingOnSurface);
	if (bWasWalkingOnClimbableSurface == bIsWalkingOnSurface) return;

	bWasWalkingOnClimbableSurface = bIsWalkingOnSurface;
	OnWalkingOnClimbableSurfaceChanged.Broadcast(bIsWalkingOnSurface);
}

void UACFWalkingComponent::SetDisableFootPlacementOnClimbableSurface(bool bDisable)
{
	bDisableFootPlacementOnClimbableSurface = bDisable;
	UpdateFootIK(IsWalkingOnClimbableSurface());
}

void UACFWalkingComponent::UpdateFootIK(bool bWalkingOnSurface)
{
	USkeletalMeshComponent* Mesh = IsValid(OwnerCharacter) ? OwnerCharacter->GetMesh() : nullptr;
	UACFAnimInstance* AnimInstance = Mesh ? Cast<UACFAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	if (FootPlacementAnimInstance.Get() != AnimInstance)
	{
		if (UACFAnimInstance* PreviousAnimInstance = FootPlacementAnimInstance.Get(); PreviousAnimInstance && bFootIKDisabledByWalking)
		{
			PreviousAnimInstance->SetEnableFootIK(true);
		}
		FootPlacementAnimInstance = AnimInstance;
		bFootIKDisabledByWalking = false;
	}
	const bool bDisableIK = bDisableFootPlacementOnClimbableSurface && bWalkingOnSurface;
	if (AnimInstance && bFootIKDisabledByWalking != bDisableIK)
	{
		AnimInstance->SetEnableFootIK(!bDisableIK);
		bFootIKDisabledByWalking = bDisableIK;
	}
}

void UACFWalkingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UACFWalkingComponent, CollisionProxy);
}

void UACFWalkingComponent::OnRep_CollisionProxy()
{
	WalkingCollisionComponent = nullptr;
}

void UACFWalkingComponent::SetSurfaceMeshPrerequisite(USkeletalMeshComponent* NewSurfaceMesh)
{
	if (SurfaceMeshPrerequisite.Get() == NewSurfaceMesh) return;
	if (USkeletalMeshComponent* PreviousSurfaceMesh = SurfaceMeshPrerequisite.Get())
	{
		RemoveTickPrerequisiteComponent(PreviousSurfaceMesh);
	}
	SurfaceMeshPrerequisite = NewSurfaceMesh;
	if (NewSurfaceMesh)
	{
		AddTickPrerequisiteComponent(NewSurfaceMesh);
	}
}

bool UACFWalkingComponent::RebuildCollisionPatch(const FResolvedTriangle* PinnedTriangle)
{
	SweepHits.Reset();
	PatchVerticesWS.Reset();
	PatchVerticesLocal.Reset();
	PatchTriangleSources.Reset();
	ActivePatchTriangleCount = 0;

	const UCapsuleComponent* CapsuleComponent = OwnerCharacter->GetCapsuleComponent();
	const UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement();
	const FVector Center = CapsuleComponent->GetComponentLocation();
	const FVector UpDirection = CapsuleComponent->GetUpVector();
	const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const float SegmentHalfLength = CapsuleComponent->GetScaledCapsuleHalfHeight() - CapsuleRadius;

	FACFSurfaceSphereSweepQuery Query;
	Query.Start = Center - UpDirection * (SegmentHalfLength + CharacterMovement->MaxStepHeight);
	Query.End = Center + UpDirection * SegmentHalfLength;
	Query.Radius = CapsuleRadius + CapsulePadding;
	Query.bCullBackfaces = false;

	if (bDebugDrawCapsulePadding)
	{
		const FVector QueryCenter = (Query.Start + Query.End) * 0.5;
		const float QueryHalfHeight = FVector::Distance(Query.Start, Query.End) * 0.5 + Query.Radius;
		DrawDebugCapsule(GetWorld(), QueryCenter, QueryHalfHeight, Query.Radius, CapsuleComponent->GetComponentQuat(), FColor::Yellow);
	}

	TArray<FResolvedTriangle> PatchTriangles;

	ClimbableSubsystem->SweepSphereSurfaceMulti(Query, OwnerCharacter, SweepHits);

	PatchTriangles.Reserve(CollisionTriangleCount);
	if (PinnedTriangle)
	{
		PatchTriangles.Add(*PinnedTriangle);
	}

	for (const FACFSurfaceHitResult& Hit : SweepHits)
	{
		if (PatchTriangles.Num() >= CollisionTriangleCount) break;

		UACFFreeClimbableComponent* HitClimbable = Hit.Climbable;
		if (PinnedTriangle && HitClimbable == PinnedTriangle->Climbable && Hit.TriangleIndex == PinnedTriangle->TriangleIndex) continue;

		FResolvedTriangle Triangle;

		if (!HitClimbable->ResolveTriangleVertices(Hit.TriangleIndex, Triangle.VerticesWS[0], Triangle.VerticesWS[1], Triangle.VerticesWS[2])) continue;

		Triangle.Climbable = HitClimbable;
		Triangle.TriangleIndex = Hit.TriangleIndex;

		PatchTriangles.Add(Triangle);
	}

	if (PatchTriangles.IsEmpty()) return false;

	ActivePatchTriangleCount = FMath::Min(CollisionTriangleCount, PatchTriangles.Num());
	PatchVerticesWS.SetNumUninitialized(CollisionTriangleCount * 3);
	PatchTriangleSources.SetNum(CollisionTriangleCount);
	for (int32 SlotIndex = 0; SlotIndex < ActivePatchTriangleCount; ++SlotIndex)
	{
		const FResolvedTriangle& Triangle = PatchTriangles[SlotIndex];
		const int32 VertexBase = SlotIndex * 3;
		PatchVerticesWS[VertexBase] = Triangle.VerticesWS[0];
		PatchVerticesWS[VertexBase + 1] = Triangle.VerticesWS[1];
		PatchVerticesWS[VertexBase + 2] = Triangle.VerticesWS[2];
		PatchTriangleSources[SlotIndex].Climbable = Triangle.Climbable;
		PatchTriangleSources[SlotIndex].TriangleIndex = Triangle.TriangleIndex;
	}

	// Keep unused slots valid for the fixed-size collision buffer.
	for (int32 SlotIndex = ActivePatchTriangleCount; SlotIndex < CollisionTriangleCount; ++SlotIndex)
	{
		const int32 VertexBase = SlotIndex * 3;
		PatchVerticesWS[VertexBase] = PatchVerticesWS[0];
		PatchVerticesWS[VertexBase + 1] = PatchVerticesWS[1];
		PatchVerticesWS[VertexBase + 2] = PatchVerticesWS[2];
		PatchTriangleSources[SlotIndex].Climbable.Reset();
		PatchTriangleSources[SlotIndex].TriangleIndex = INDEX_NONE;
	}

	return true;
}

bool UACFWalkingComponent::TryFindFloorAnchor(FFloorAnchor& OutAnchor) const
{
	// CurrentFloor refers to the collision patch from the previous tick.
	const UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement();
	const FVector ImpactPoint = CharacterMovement->CurrentFloor.HitResult.ImpactPoint;
	const FVector ImpactNormal = CharacterMovement->CurrentFloor.HitResult.ImpactNormal;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	int32 BestSlotIndex = INDEX_NONE;
	FVector BestPoint = FVector::ZeroVector;

	for (int32 SlotIndex = 0; SlotIndex < ActivePatchTriangleCount; ++SlotIndex)
	{
		if (!PatchTriangleSources[SlotIndex].Climbable.IsValid()) continue;

		const int32 VertexBase = SlotIndex * 3;
		const FVector& P0 = PatchVerticesWS[VertexBase];
		const FVector& P1 = PatchVerticesWS[VertexBase + 1];
		const FVector& P2 = PatchVerticesWS[VertexBase + 2];
		FVector TriangleNormal = FVector::CrossProduct(P2 - P0, P1 - P0);
		if (!TriangleNormal.Normalize()) continue;
		if (FVector::DotProduct(TriangleNormal, ImpactNormal) <= 0.0) continue;

		const FVector ClosestPoint = FMath::ClosestPointOnTriangleToPoint(ImpactPoint, P0, P1, P2);
		const double DistanceSquared = FVector::DistSquared(ImpactPoint, ClosestPoint);
		if (DistanceSquared >= BestDistanceSquared) continue;

		BestDistanceSquared = DistanceSquared;
		BestSlotIndex = SlotIndex;
		BestPoint = ClosestPoint;
	}

	if (BestSlotIndex == INDEX_NONE) return false;

	const int32 VertexBase = BestSlotIndex * 3;
	const FVector& P0 = PatchVerticesWS[VertexBase];
	const FVector& P1 = PatchVerticesWS[VertexBase + 1];
	const FVector& P2 = PatchVerticesWS[VertexBase + 2];
	FVector Barycentric;
	if (!FMath::ComputeBarycentricTri(BestPoint, P0, P1, P2, Barycentric)) return false;
	if (!TryBuildTriangleTransform(P0, P1, P2, Barycentric, OutAnchor.PreviousTransform)) return false;

	const FPatchTriangleSource& Source = PatchTriangleSources[BestSlotIndex];
	OutAnchor.Climbable = Source.Climbable;
	OutAnchor.TriangleIndex = Source.TriangleIndex;
	OutAnchor.Barycentric = Barycentric;
	return true;
}

bool UACFWalkingComponent::TryBuildTriangleTransform(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& Barycentric, FTransform& OutTransform)
{
	FVector Tangent = P1 - P0;
	FVector Normal = FVector::CrossProduct(P2 - P0, Tangent);
	if (!Tangent.Normalize() || !Normal.Normalize()) return false;

	const FVector Position = Barycentric.X * P0 + Barycentric.Y * P1 + Barycentric.Z * P2;
	const FQuat Rotation = FRotationMatrix::MakeFromXZ(Tangent, Normal).ToQuat().GetNormalized();

	OutTransform = FTransform(Rotation, Position, FVector::OneVector);
	return true;
}

void UACFWalkingComponent::UpdateCollisionPatch(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ACFWalking_UpdateCollisionPatch);

	UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement();
	const EMovementMode MovementMode = CharacterMovement->MovementMode;
	if (MovementMode != MOVE_Walking && MovementMode != MOVE_Falling && !bPrimeCollisionPatch)
	{
		WalkingCollisionComponent->ClearCollisionPatch();
		WalkingCollisionComponent->ComponentVelocity = FVector::ZeroVector;
		ActivePatchTriangleCount = 0;
		PatchTriangleSources.Reset();
		return;
	}

	FFloorAnchor FloorAnchor;
	FResolvedTriangle CurrentFloorTriangle;
	const FResolvedTriangle* PinnedFloorTriangle = nullptr;
	FTransform CurrentAnchorTransform = FTransform::Identity;
	UACFFreeClimbableComponent* FloorSource = nullptr;
	if (MovementMode == MOVE_Walking && CharacterMovement->GetMovementBaseObject() == WalkingCollisionComponent && TryFindFloorAnchor(FloorAnchor))
	{
		FloorSource = FloorAnchor.Climbable.Get();

		// Resolve the same floor point against the current surface pose.
		if (FloorSource->ResolveTriangleVertices(FloorAnchor.TriangleIndex, CurrentFloorTriangle.VerticesWS[0], CurrentFloorTriangle.VerticesWS[1], CurrentFloorTriangle.VerticesWS[2]))
		{
			CurrentFloorTriangle.Climbable = FloorSource;
			CurrentFloorTriangle.TriangleIndex = FloorAnchor.TriangleIndex;
			if (TryBuildTriangleTransform(CurrentFloorTriangle.VerticesWS[0], CurrentFloorTriangle.VerticesWS[1], CurrentFloorTriangle.VerticesWS[2], FloorAnchor.Barycentric, CurrentAnchorTransform))
			{
				PinnedFloorTriangle = &CurrentFloorTriangle;
			}
		}
	}

	if (FloorSource)
	{
		SetSurfaceMeshPrerequisite(FloorSource->GetSurfaceMeshComponent());
	}
	else
	{
		SetSurfaceMeshPrerequisite(nullptr);
	}

	const FTransform PreviousCollisionComponentTransform = WalkingCollisionComponent->GetComponentTransform();
	FTransform CollisionComponentTransform = PreviousCollisionComponentTransform;
	if (PinnedFloorTriangle)
	{
		const FQuat SurfaceRotationDelta = (CurrentAnchorTransform.GetRotation() * FloorAnchor.PreviousTransform.GetRotation().Inverse()).GetNormalized();
		const FVector SurfaceTranslationDelta = CurrentAnchorTransform.GetLocation() - SurfaceRotationDelta.RotateVector(FloorAnchor.PreviousTransform.GetLocation());
		if (!SurfaceTranslationDelta.IsNearlyZero() || !SurfaceRotationDelta.Equals(FQuat::Identity, UE_SMALL_NUMBER))
		{
			const FTransform SurfaceDelta(SurfaceRotationDelta, SurfaceTranslationDelta, FVector::OneVector);
			CollisionComponentTransform = CollisionComponentTransform * SurfaceDelta;
			WalkingCollisionComponent->SetWorldLocationAndRotation(CollisionComponentTransform.GetLocation(), CollisionComponentTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	WalkingCollisionComponent->ComponentVelocity = FVector::ZeroVector;
	if (bImpartSurfaceVelocity && PinnedFloorTriangle && DeltaTime > UE_SMALL_NUMBER)
	{
		const UCapsuleComponent* CapsuleComponent = OwnerCharacter->GetCapsuleComponent();
		const FVector CapsuleBase = CapsuleComponent->GetComponentLocation() + CapsuleComponent->GetScaledCapsuleHalfHeight() * CharacterMovement->GetGravityDirection();
		const FVector PreviousLocalCapsuleBase = PreviousCollisionComponentTransform.InverseTransformPositionNoScale(CapsuleBase);
		const FVector NewCapsuleBase = CollisionComponentTransform.TransformPositionNoScale(PreviousLocalCapsuleBase);
		const FVector OriginVelocity = (CollisionComponentTransform.GetLocation() - PreviousCollisionComponentTransform.GetLocation()) / DeltaTime;
		WalkingCollisionComponent->ComponentVelocity = CharacterMovement->bImpartBaseAngularVelocity ? (NewCapsuleBase - CapsuleBase) / DeltaTime : OriginVelocity;
	}

	// Keep the tracked floor triangle in the rebuilt collision patch.
	if (!RebuildCollisionPatch(PinnedFloorTriangle))
	{
		WalkingCollisionComponent->ClearCollisionPatch();
		WalkingCollisionComponent->ComponentVelocity = FVector::ZeroVector;
		return;
	}

	PatchVerticesLocal.SetNumUninitialized(PatchVerticesWS.Num());
	for (int32 VertexIndex = 0; VertexIndex < PatchVerticesWS.Num(); ++VertexIndex)
	{
		PatchVerticesLocal[VertexIndex] = CollisionComponentTransform.InverseTransformPositionNoScale(PatchVerticesWS[VertexIndex]);
	}

	WalkingCollisionComponent->UpdateCollisionVertices(PatchVerticesLocal);

	if (bDebugDrawCollisionPatch)
	{
		const FVector UpDirection = -CharacterMovement->GetGravityDirection();
		const float WalkableFloorZ = CharacterMovement->GetWalkableFloorZ();

		for (int32 TriangleIndex = 0; TriangleIndex < ActivePatchTriangleCount; ++TriangleIndex)
		{
			const int32 VertexBase = TriangleIndex * 3;
			const FVector& P0 = PatchVerticesWS[VertexBase];
			const FVector& P1 = PatchVerticesWS[VertexBase + 1];
			const FVector& P2 = PatchVerticesWS[VertexBase + 2];
			const FVector TriangleNormal = FVector::CrossProduct(P2 - P0, P1 - P0).GetSafeNormal();

			constexpr float DebugOffset = 0.5f;
			const FVector Offset = TriangleNormal * DebugOffset;

			const FVector D0 = P0 + Offset;
			const FVector D1 = P1 + Offset;
			const FVector D2 = P2 + Offset;

			FColor TriangleColor(255, 128, 0);
			if (FVector::DotProduct(TriangleNormal, UpDirection) >= WalkableFloorZ)
			{
				TriangleColor = FColor::Green;
			}

			DrawDebugLine(GetWorld(), D0, D1, TriangleColor, false, -1.0f, SDPG_World);
			DrawDebugLine(GetWorld(), D1, D2, TriangleColor, false, -1.0f, SDPG_World);
			DrawDebugLine(GetWorld(), D2, D0, TriangleColor, false, -1.0f, SDPG_World);
		}
	}
}
