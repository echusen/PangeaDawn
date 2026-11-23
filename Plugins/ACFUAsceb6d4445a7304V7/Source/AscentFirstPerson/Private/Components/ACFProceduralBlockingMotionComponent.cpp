// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFProceduralBlockingMotionComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Presets/ACFProceduralPresetBlockingMotion.h"

static TAutoConsoleVariable<int32> CVarBlockingDrawDebug(
	TEXT("ACF.FirstPerson.Debug.Blocking"),
	0,
	TEXT("Draw blocking debug lines\n<=0: off, 1: on"),
	ECVF_Default);

UACFProceduralBlockingMotionComponent::UACFProceduralBlockingMotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bBlockingEnabled = true;
	bAutoFindMeshComponent = true;
	Stage = 6;
}

void UACFProceduralBlockingMotionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindMeshComponent && GetOwner())
	{
		TargetMeshComponent = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	}
}

void UACFProceduralBlockingMotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnPoseFinalizedHandle.IsValid() && IsValid(TargetMeshComponent))
	{
		TargetMeshComponent->UnregisterOnBoneTransformsFinalizedDelegate(OnPoseFinalizedHandle);
		OnPoseFinalizedHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UACFProceduralBlockingMotionComponent::BeforeEvaluate(float DeltaSeconds, FComponentSpacePoseContext& Context, FCompactPoseBoneIndex& BoneIndex)
{
	USkeletalMeshComponent* CurrentMesh = Context.AnimInstanceProxy->GetSkelMeshComponent();
	if (CurrentMesh && CurrentMesh != TargetMeshComponent)
	{
		if (OnPoseFinalizedHandle.IsValid() && IsValid(TargetMeshComponent))
		{
			TargetMeshComponent->UnregisterOnBoneTransformsFinalizedDelegate(OnPoseFinalizedHandle);
			OnPoseFinalizedHandle.Reset();
		}
		TargetMeshComponent = CurrentMesh;
	}

	if (!OnPoseFinalizedHandle.IsValid() && IsValid(TargetMeshComponent))
	{
		FOnBoneTransformsFinalizedMultiCast::FDelegate Delegate;
		Delegate.BindUObject(this, &UACFProceduralBlockingMotionComponent::OnPoseFinalized);
		TargetMeshComponent->RegisterOnBoneTransformsFinalizedDelegate(Delegate);
		OnPoseFinalizedHandle = Delegate.GetHandle();
	}

	const FBoneContainer& BoneContainer = Context.Pose.GetPose().GetBoneContainer();
	FCompactPoseBoneIndex ParentBoneIndex = BoneContainer.GetParentBoneIndex(BoneIndex);

	if (ParentBoneIndex != INDEX_NONE)
	{
		if (ParentBoneName.IsNone())
		{
			ParentBoneName = BoneContainer.GetReferenceSkeleton().GetBoneName(ParentBoneIndex.GetInt());
		}

		const FTransform& PivotCS = Context.Pose.GetComponentSpaceTransform(BoneIndex);
		const FTransform& ParentCS = Context.Pose.GetComponentSpaceTransform(ParentBoneIndex);
		StableRelativeTransform = PivotCS.GetRelativeTransform(ParentCS);
	}
}

bool UACFProceduralBlockingMotionComponent::ComputeOutput(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, FACFProceduralMotionOutput& Out)
{
	const UACFProceduralPresetBlockingMotion* PresetBlocking = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetBlockingMotion>(GetEffectivePresetData());
	if (!PresetBlocking) return false;

	RefreshBlocking(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (CVarBlockingDrawDebug.GetValueOnGameThread() > 0)
	{
		// Cyan: the item start. Yellow: where the item reaches. Red: the contact, while blocking.
		DrawDebugPoint(GetWorld(), BlockingTraceStartLocation, 6.0f, FColor::Cyan);
		DrawDebugPoint(GetWorld(), BlockingTraceEndLocation, 6.0f, FColor::Yellow);
		DrawDebugLine(GetWorld(), BlockingTraceStartLocation, BlockingTraceEndLocation, BlockingHitResult.bBlockingHit ? FColor::Red : FColor::Green);
		DrawDebugSphere(GetWorld(), BlockingHitResult.bBlockingHit ? BlockingHitResult.Location : BlockingTraceEndLocation, TraceRadius, 12, BlockingHitResult.bBlockingHit ? FColor::Red : FColor::Green);
		if (BlockingHitResult.bBlockingHit)
		{
			DrawDebugPoint(GetWorld(), BlockingHitResult.ImpactPoint, 8.0f, FColor::Red);
		}
	}
#endif

	BlockingOutput = FTransform(PresetBlocking->BlockingOffsetRotation, PresetBlocking->BlockingOffsetLocation);

	Out.Stage = Stage;
	Out.Transform = BlockingOutput;
	Out.Weight = BlockingAlpha;

	return true;
}

void UACFProceduralBlockingMotionComponent::RefreshBlocking(float DeltaTime)
{
	if (!bBlockingEnabled)
	{
		BlockingAlpha = FMath::FInterpTo(BlockingAlpha, 0.0f, DeltaTime, BlockingInterpSpeed);
		BlockingOutput = UKismetMathLibrary::TInterpTo(BlockingOutput, FTransform::Identity, DeltaTime, BlockingInterpSpeed);
		return;
	}

	AActor* Owner = GetOwner();
	ACharacter* OwnerCharacter = AnimOwnerCharacter;
	const UACFProceduralPresetBlockingMotion* PresetBlocking = UACFProceduralPresetData::GetPresetOrDefault<UACFProceduralPresetBlockingMotion>(GetEffectivePresetData());

	// Ensure BeforeBlockingPose is valid before sweeping
	if (!IsValid(Owner) || !IsValid(OwnerCharacter) || !IsValid(TargetMeshComponent) || !PresetBlocking || BeforeBlockingPose.Equals(FTransform::Identity)) return;

	const FVector ViewDirection = (OwnerCharacter->IsLocallyControlled() || OwnerCharacter->IsLocallyViewed())
		? OwnerCharacter->GetViewRotation().Vector()
		: OwnerCharacter->GetBaseAimRotation().Vector();

	// The sweep runs from where the item starts, along the view, for as far as the item reaches: the distance its
	// collision point sits ahead of the start along the view, so height and side offsets (a muzzle above the grip)
	// do not count. Blocking then starts exactly when the end touches a wall and is full once the start reaches it.
	BlockingTraceStartLocation = (bHasItemOrigin ? ItemOrigin * BeforeBlockingPose : BeforeBlockingPose).GetLocation();

	BlockingReach = CollisionPointDistance;
	if (bHasCollisionPoint)
	{
		const FVector CollisionPointLocation = (CollisionPoint * BeforeBlockingPose).GetLocation();
		const float MeasuredReach = FVector::DotProduct(CollisionPointLocation - BlockingTraceStartLocation, ViewDirection);

		// An end that sits behind the start, e.g. an item held pointing back, keeps the fallback reach.
		if (MeasuredReach > 1.0f)
		{
			BlockingReach = MeasuredReach;
		}
	}
	BlockingReach = FMath::Max(BlockingReach, 1.0f);

	BlockingTraceEndLocation = BlockingTraceStartLocation + (ViewDirection * BlockingReach);

	BlockingHitResult = FHitResult(ForceInit);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerCharacter);
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bTraceComplex = false;

	const bool bHit = GetWorld()->SweepSingleByChannel(
		BlockingHitResult,
		BlockingTraceStartLocation,
		BlockingTraceEndLocation,
		FQuat::Identity,
		PresetBlocking->TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams
	);

	float TargetAlpha = 0.0f;
	if (bHit)
	{
		// Measured to the contact along the view, not to the sphere's centre, so the radius does not start it early.
		const float DistToObstacle = BlockingHitResult.bStartPenetrating
			? 0.0f
			: FMath::Max(FVector::DotProduct(BlockingHitResult.ImpactPoint - BlockingTraceStartLocation, ViewDirection), 0.0f);

		TargetAlpha = FMath::Clamp(1.0f - (DistToObstacle / BlockingReach), 0.0f, 1.0f);
	}
	if (OverrideBlockingAlpha >= 0.0f)
	{
		TargetAlpha = OverrideBlockingAlpha;
	}

	BlockingAlpha = FMath::FInterpTo(BlockingAlpha, TargetAlpha, DeltaTime, BlockingInterpSpeed);
}

void UACFProceduralBlockingMotionComponent::OnPoseFinalized()
{
	if (!IsValid(TargetMeshComponent) || ParentBoneName.IsNone()) return;

	FTransform ParentWorldTransform = TargetMeshComponent->GetSocketTransform(ParentBoneName, RTS_World);
	BeforeBlockingPose = StableRelativeTransform * ParentWorldTransform;
}
void UACFProceduralBlockingMotionComponent::SetCollisionPoint(const FTransform& InCollisionPoint)
{
	CollisionPoint = InCollisionPoint;
	bHasCollisionPoint = true;
}

void UACFProceduralBlockingMotionComponent::ClearCollisionPoint()
{
	CollisionPoint = FTransform::Identity;
	bHasCollisionPoint = false;
}

void UACFProceduralBlockingMotionComponent::SetItemOrigin(const FTransform& InItemOrigin)
{
	ItemOrigin = InItemOrigin;
	bHasItemOrigin = true;
}

void UACFProceduralBlockingMotionComponent::ClearItemOrigin()
{
	ItemOrigin = FTransform::Identity;
	bHasItemOrigin = false;
}

TSubclassOf<UACFProceduralPreset> UACFProceduralBlockingMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetBlockingMotion::StaticClass();
}
