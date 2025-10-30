// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Components/ACFAIPatrolComponent.h"
#include "ACFAIController.h"
#include "ACFPatrolPath.h"
#include <Components/SplineComponent.h>
#include <NavigationSystem.h>
#include <NavFilters/NavigationQueryFilter.h>

// Sets default values for this component's properties
UACFAIPatrolComponent::UACFAIPatrolComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
    SetPatrolType(EPatrolType::EFollowSpline);
    RandomPatrolRadius = 5000;
    WaitTimeAtPoint = 2.f;
}

// Called when the game starts
void UACFAIPatrolComponent::BeginPlay()
{
    Super::BeginPlay();
}

bool UACFAIPatrolComponent::TryGetNextWaypoint(FVector& outLocation)
{

    if (GetPatrolType() == EPatrolType::EFollowSpline && PathToFollow) {
        const USplineComponent* path = PathToFollow->GetSplineComponent();
        if (!path) {
            return false;
        }
        const FTransform waypoint = path->GetTransformAtSplinePoint(patrolIndex, ESplineCoordinateSpace::World);
        if (patrolIndex > path->GetNumberOfSplinePoints() - 1) {
            patrolIndex = 0;

        } else {
            patrolIndex++;
        }

        FVector outLoc;
        if (UNavigationSystemV1::K2_ProjectPointToNavigation(this, waypoint.GetLocation(), outLoc, nullptr, UNavigationQueryFilter::StaticClass())) {
            outLocation = outLoc;
        } else if (waypoint.GetLocation() != FVector::ZeroVector) {
            outLocation = waypoint.GetLocation();
        }
        return true;
    } else if (GetPatrolType() == EPatrolType::ERandomPoint) {
        TObjectPtr<APawn> pawnOwner = Cast<APawn>(GetOwner());
        if (pawnOwner) {
            TObjectPtr<AACFAIController> controller = Cast<AACFAIController>(pawnOwner->GetController());
            if (controller && UNavigationSystemV1::K2_GetRandomReachablePointInRadius(controller, controller->GetHomeLocation(), outLocation, RandomPatrolRadius)) {
                return true;
            }
        }
    }

    return false;
}

float UACFAIPatrolComponent::GetWaitTime() const
{

    return WaitTimeAtPoint;
}
