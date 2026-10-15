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
		const FVector DebugLoc = BlockingHitResult.bBlockingHit ? BlockingHitResult.Location : BlockingTraceEndLocation;
		DrawDebugPoint(GetWorld(), BlockingTraceStartLocation, 5.0f, FColor::Cyan);
		DrawDebugLine(GetWorld(), BlockingTraceStartLocation, DebugLoc, BlockingHitResult.bBlockingHit ? FColor::Red : FColor::Green);
		DrawDebugSphere(GetWorld(), DebugLoc, TraceRadius, 12, BlockingHitResult.bBlockingHit ? FColor::Red : FColor::Green);
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

	BlockingTraceStartLocation = BeforeBlockingPose.GetLocation();

	const FVector ViewDirection = (OwnerCharacter->IsLocallyControlled() || OwnerCharacter->IsLocallyViewed())
		? OwnerCharacter->GetViewRotation().Vector()
		: OwnerCharacter->GetBaseAimRotation().Vector();

	// The item already covers the distance out to its collision point, so the alpha only ramps over the reach past
	// it. Without one this is the holder itself, and the sweep is exactly CollisionPointDistance long.
	const FVector ReachStartLocation = bHasCollisionPoint ? (CollisionPoint * BeforeBlockingPose).GetLocation() : BlockingTraceStartLocation;
	const float ItemLength = FVector::Distance(BlockingTraceStartLocation, ReachStartLocation);

	BlockingTraceEndLocation = ReachStartLocation + (ViewDirection * CollisionPointDistance);

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
		const float DistToObstacle = (BlockingHitResult.Location - BlockingTraceStartLocation).Size();
		if (CollisionPointDistance > KINDA_SMALL_NUMBER)
		{
			const float AlphaRaw = 1.0f - ((DistToObstacle - ItemLength) / CollisionPointDistance);
			TargetAlpha = FMath::Clamp(AlphaRaw, 0.0f, 1.0f);
		}
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

TSubclassOf<UACFProceduralPreset> UACFProceduralBlockingMotionComponent::GetPresetClass() const
{
	return UACFProceduralPresetBlockingMotion::StaticClass();
}
