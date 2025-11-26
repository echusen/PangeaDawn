// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "Components/ACFSplineFollowerComponent.h"
#include "AIController.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

UACFSplineFollowerComponent::UACFSplineFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bIsActive = false;
	bIsWaitingForTarget = false;
	CurrentSplinePointIndex = 0;

	// Initialize default values
	PointReachedThreshold = 100.0f;
	MaxFollowDistance = 2000.0f;
	MinFollowDistance = 500.0f;
	NavMeshProjectionExtent = 500.0f;
}

void UACFSplineFollowerComponent::BeginPlay()
{
	Super::BeginPlay();

	/** Get AI Controller reference */
	OwnerAIController = Cast<AAIController>(GetOwner());
	if (!OwnerAIController) {
		UE_LOG(LogTemp, Warning, TEXT("ACFSplineFollowerComponent: Owner is not an AIController"));
	}
}

void UACFSplineFollowerComponent::StartFollowing(USplineComponent* InSpline, APawn* InTargetPawn)
{
	if (!OwnerAIController) {
		UE_LOG(LogTemp, Error, TEXT("SplineFollower: No AIController found"));
		return;
	}

	InitPath(InSpline, InTargetPawn);
	StartFollowingCurrentSpline();
}

void UACFSplineFollowerComponent::InitPath(USplineComponent* InSpline, APawn* InTargetPawn)
{
	if (!InSpline || !InTargetPawn) {
		UE_LOG(LogTemp, Error, TEXT("SplineFollower: Invalid spline or target pawn"));
		return;
	}

	/** Setup references */
	SplinePath = InSpline;
	TargetPawn = InTargetPawn;
}

void UACFSplineFollowerComponent::StartFollowingCurrentSpline()
{
    /** Initialize state */
    bIsActive = true;
    bIsWaitingForTarget = false;

    /** Validate spline */
    if (!SplinePath || SplinePath->GetNumberOfSplinePoints() < 2) {
        UE_LOG(LogTemp, Error, TEXT("SplineFollower: Invalid spline or not enough points"));
        StopFollowing();
        return;
    }

    /** Find the NEXT spline point ahead of current AI position */
    if (OwnerAIController) {
        APawn* AIPawn = OwnerAIController->GetPawn();
        if (AIPawn) {
            const FVector CurrentLocation = AIPawn->GetActorLocation();
            const int32 NumPoints = SplinePath->GetNumberOfSplinePoints();
            
            /** Find closest point on spline */
            const float InputKey = SplinePath->FindInputKeyClosestToWorldLocation(CurrentLocation);
            
            /** ALWAYS round UP to get the NEXT point, never go back */
            int32 NextPointIndex = FMath::CeilToInt(InputKey);
            
            /** Clamp to valid range */
            CurrentSplinePointIndex = FMath::Clamp(
                NextPointIndex, 
                0, 
                NumPoints - 1
            );
            
            UE_LOG(LogTemp, Log, TEXT("SplineFollower: Resuming from NEXT point %d (InputKey: %f)"), 
                CurrentSplinePointIndex, InputKey);
        } else {
            CurrentSplinePointIndex = 0;
        }
    } else {
        CurrentSplinePointIndex = 0;
    }

    UE_LOG(LogTemp, Log, TEXT("SplineFollower: Starting to follow spline with %d points from point %d"), 
        SplinePath->GetNumberOfSplinePoints(), CurrentSplinePointIndex);

    /** Enable ticking */
    SetComponentTickEnabled(true);

    /** Start movement to current spline point */
    MoveToNextSplinePoint();

    /** Broadcast start event */
    OnFollowingStarted.Broadcast();
}

void UACFSplineFollowerComponent::StopFollowing()
{
	if (!bIsActive) {
		return;
	}

	bIsActive = false;
	SetComponentTickEnabled(false);

	/** Remove move completed delegate */
	if (OwnerAIController && MoveCompletedDelegateHandle.IsValid()) {
		OwnerAIController->GetPathFollowingComponent()->OnRequestFinished.Remove(MoveCompletedDelegateHandle);
		MoveCompletedDelegateHandle.Reset();
	}

	/** Clear references */
	SplinePath = nullptr;
	TargetPawn = nullptr;

	/** Broadcast stop event */
	OnFollowingStopped.Broadcast();
}

void UACFSplineFollowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsActive || !SplinePath || !TargetPawn || !OwnerAIController) {
		return;
	}

	UpdateMovement(DeltaTime);
}

void UACFSplineFollowerComponent::UpdateMovement(float DeltaTime)
{
	/** Check if spline is completed */
	if (HasReachedSplineEnd()) {
		UE_LOG(LogTemp, Warning, TEXT("SplineFollower: Spline completed!"));
		OnSplineCompleted.Broadcast();
		StopFollowing();
		return;
	}

	/** Handle target pawn distance checks */
	if (IsTargetPawnTooFar()) {
		if (!bIsWaitingForTarget) {
			StopAndWaitForTargetPawn();
		}
		return;
	}
	else if (bIsWaitingForTarget && IsTargetPawnCloseEnough()) {
		ResumeFollowingSpline();
		return;
	}
}

float UACFSplineFollowerComponent::GetProgressPercentage() const
{
	if (!SplinePath) {
		return 0.0f;
	}

	const int32 NumPoints = SplinePath->GetNumberOfSplinePoints();
	if (NumPoints <= 1) {
		return 0.0f;
	}

	return FMath::Clamp(
		(float)CurrentSplinePointIndex / (float)(NumPoints - 1) * 100.0f,
		0.0f,
		100.0f
	);
}

int32 UACFSplineFollowerComponent::GetCurrentSegmentIndex() const
{
	return CurrentSplinePointIndex;
}

float UACFSplineFollowerComponent::GetDistanceToTargetPawn() const
{
	if (!OwnerAIController || !TargetPawn) {
		return 0.0f;
	}

	const APawn* const AIPawn = OwnerAIController->GetPawn();
	if (!AIPawn) {
		return 0.0f;
	}

	return FVector::Dist(AIPawn->GetActorLocation(), TargetPawn->GetActorLocation());
}

bool UACFSplineFollowerComponent::IsTargetPawnTooFar() const
{
	return GetDistanceToTargetPawn() > MaxFollowDistance;
}

bool UACFSplineFollowerComponent::IsTargetPawnCloseEnough() const
{
	return GetDistanceToTargetPawn() < MinFollowDistance;
}

FVector UACFSplineFollowerComponent::GetNextSplineLocation() const
{
	if (!SplinePath) {
		return FVector::ZeroVector;
	}

	const int32 NumPoints = SplinePath->GetNumberOfSplinePoints();
	if (CurrentSplinePointIndex >= NumPoints) {
		return SplinePath->GetLocationAtSplinePoint(
			NumPoints - 1,
			ESplineCoordinateSpace::World
		);
	}

	return SplinePath->GetLocationAtSplinePoint(
		CurrentSplinePointIndex,
		ESplineCoordinateSpace::World
	);
}

bool UACFSplineFollowerComponent::HasReachedSplineEnd() const
{
	if (!SplinePath) {
		return true;
	}

	return CurrentSplinePointIndex >= SplinePath->GetNumberOfSplinePoints();
}

void UACFSplineFollowerComponent::MoveToNextSplinePoint()
{
	if (!OwnerAIController || !SplinePath) {
		return;
	}

	/** Check if we've reached the end of the spline */
	const int32 NumSplinePoints = SplinePath->GetNumberOfSplinePoints();
	if (CurrentSplinePointIndex >= NumSplinePoints) {
		UE_LOG(LogTemp, Log, TEXT("SplineFollower: No more points to move to"));
		return;
	}

	/** Get the location of the current spline point we're targeting */
	const FVector SplinePointLocation = SplinePath->GetLocationAtSplinePoint(
		CurrentSplinePointIndex,
		ESplineCoordinateSpace::World
	);

	UE_LOG(LogTemp, Log, TEXT("SplineFollower: Moving to spline point %d/%d"),
		CurrentSplinePointIndex, NumSplinePoints - 1);

	/** Project the spline point onto the NavMesh */
	FVector TargetLocation = SplinePointLocation;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys) {
		FNavLocation NavLocation;
		const FVector SearchExtent(NavMeshProjectionExtent, NavMeshProjectionExtent, NavMeshProjectionExtent);

		const bool bProjected = NavSys->ProjectPointToNavigation(
			SplinePointLocation,
			NavLocation,
			SearchExtent
		);

		if (bProjected) {
			TargetLocation = NavLocation.Location;
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("SplineFollower: Failed to project point %d to NavMesh"),
				CurrentSplinePointIndex);
		}
	}

	/** Setup the move request */
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(TargetLocation);
	MoveRequest.SetAcceptanceRadius(PointReachedThreshold);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetCanStrafe(false);

	/** Bind to move completed delegate */
	if (MoveCompletedDelegateHandle.IsValid()) {
		OwnerAIController->GetPathFollowingComponent()->OnRequestFinished.Remove(MoveCompletedDelegateHandle);
	}

	MoveCompletedDelegateHandle = OwnerAIController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(
		this,
		&UACFSplineFollowerComponent::OnMoveCompleted
	);

	/** Execute the move */
	FPathFollowingRequestResult MoveResult = OwnerAIController->MoveTo(MoveRequest);

	/** Log the result */
	if (MoveResult.Code == EPathFollowingRequestResult::Failed) {
		UE_LOG(LogTemp, Error, TEXT("SplineFollower: MoveTo FAILED for point %d"),
			CurrentSplinePointIndex);
	}
	else if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal) {
		UE_LOG(LogTemp, Warning, TEXT("SplineFollower: Already at point %d, moving to next"),
			CurrentSplinePointIndex);
		CurrentSplinePointIndex++;
		OnSegmentReached.Broadcast(CurrentSplinePointIndex - 1);
		MoveToNextSplinePoint();
	}
}

void UACFSplineFollowerComponent::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!bIsActive || bIsWaitingForTarget) {
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("SplineFollower: Reached spline point %d"), CurrentSplinePointIndex);

	/** Broadcast that we reached this point */
	OnSegmentReached.Broadcast(CurrentSplinePointIndex);

	/** Check if this is the last point */
	const int32 NumSplinePoints = SplinePath->GetNumberOfSplinePoints();
	if (CurrentSplinePointIndex >= NumSplinePoints - 1) {
		/** Apply final rotation from spline */
		if (APawn* const AIPawn = OwnerAIController->GetPawn()) {
			const FRotator FinalRotation = SplinePath->GetRotationAtSplinePoint(
				CurrentSplinePointIndex,
				ESplineCoordinateSpace::World
			);

			UE_LOG(LogTemp, Log, TEXT("SplineFollower: Applying final spline rotation: %s"),
				*FinalRotation.ToString());

			AIPawn->SetActorRotation(FinalRotation);
		}
	}

	/** Move to the next spline point */
	CurrentSplinePointIndex++;
	MoveToNextSplinePoint();
}

void UACFSplineFollowerComponent::StopAndWaitForTargetPawn()
{
	if (!OwnerAIController || !TargetPawn) {
		return;
	}

	bIsWaitingForTarget = true;
	OwnerAIController->StopMovement();

	/** Rotate towards target pawn */
	if (APawn* const AIPawn = OwnerAIController->GetPawn()) {
		const FVector DirectionToTarget = (TargetPawn->GetActorLocation() - AIPawn->GetActorLocation()).GetSafeNormal();
		const FRotator NewRotation = DirectionToTarget.Rotation();
		AIPawn->SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
	}

	/** Broadcast wait event */
	OnStartedWaitingForTarget.Broadcast();
}

void UACFSplineFollowerComponent::ResumeFollowingSpline()
{
	bIsWaitingForTarget = false;

	/** Broadcast resume event */
	OnTargetRejoined.Broadcast();

	MoveToNextSplinePoint();
}