// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ATSBaseTargetComponent.h"
#include "ATSTargetPointComponent.h"
#include "Net/UnrealNetwork.h"
#include <GameFramework/Controller.h>
#include <GameFramework/Pawn.h>

// Sets default values for this component's properties
UATSBaseTargetComponent::UATSBaseTargetComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UATSBaseTargetComponent::SetTargetPoint_Implementation(UATSTargetPointComponent* targetPoint)
{
    CurrentTargetPoint = targetPoint;
}

bool UATSBaseTargetComponent::SetTargetPoint_Validate(UATSTargetPointComponent* targetPoint)
{
    return true;
}

void UATSBaseTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UATSBaseTargetComponent, CurrentTarget);
    DOREPLIFETIME(UATSBaseTargetComponent, CurrentTargetPoint);
}

void UATSBaseTargetComponent::SetTarget_Implementation(AActor* target)
{
    if (target != CurrentTarget) {
        CurrentTarget = target;
        OnNewTarget.Broadcast(CurrentTarget);
    }
}

bool UATSBaseTargetComponent::SetTarget_Validate(AActor* target)
{
    return true;
}

bool UATSBaseTargetComponent::IsTargetInSight() const
{

    AController* contr = Cast<AController>(GetOwner());
    if (contr) {
        return contr->LineOfSightTo(CurrentTarget);
    }
    return false;
}

bool UATSBaseTargetComponent::IsPotentialTargetInSight(AActor* potentialTarget) const
{
    AController* contr = Cast<AController>(GetOwner());
    if (contr) {
        return contr->LineOfSightTo(potentialTarget);
    }
    return false;
}

void UATSBaseTargetComponent::OnRep_CurrentTarget()
{
    OnNewTarget.Broadcast(CurrentTarget);
    OnTargetChanged();
}

void UATSBaseTargetComponent::OnTargetChanged()
{
}

FVector UATSBaseTargetComponent::GetCurrentTargetPointLocation() const
{
    if (CurrentTarget) {
        if (CurrentTargetPoint) {
            return CurrentTargetPoint->GetComponentLocation();
        } else {
            return CurrentTarget->GetActorLocation();
        }
    }

    return FVector();
}

bool UATSBaseTargetComponent::GetLookAtLocation(FVector& OutLocation) const
{
    if (!CurrentTarget) {
        return false;
    }

    OutLocation = ResolveLookAtPoint();
    return true;
}

UATSBaseTargetComponent* UATSBaseTargetComponent::FindForActor(const AActor* Actor)
{
    if (!Actor) {
        return nullptr;
    }

    if (const APawn* pawn = Cast<APawn>(Actor)) {
        if (AController* controller = pawn->GetController()) {
            if (UATSBaseTargetComponent* targetComp = controller->FindComponentByClass<UATSBaseTargetComponent>()) {
                return targetComp;
            }
        }
    }

    if (const AController* controller = Cast<AController>(Actor)) {
        return controller->FindComponentByClass<UATSBaseTargetComponent>();
    }

    return Actor->FindComponentByClass<UATSBaseTargetComponent>();
}

FVector UATSBaseTargetComponent::ResolveLookAtPoint() const
{
    FVector lookAtPoint = GetCurrentTargetPointLocation();

    // A target point is authored for the camera lock and for target switching, so its height is
    // whatever framed the target best - usually the chest. Aiming a ray from the eyes at it tilts
    // the character down by however much the two heights differ, and the closer the opponent gets
    // the steeper that tilt becomes, until the character is staring at its feet.
    // So keep only the point's horizontal direction and put its height at the same eye level the
    // ray starts from: an equally tall opponent is then always dead level, whatever eye height the
    // project uses, while a genuinely taller or shorter one still gives a real pitch because its
    // capsule sits higher or lower.
    if (bAimAtTargetEyeHeight) {
        if (const APawn* viewerPawn = GetViewerPawn()) {
            lookAtPoint.Z = CurrentTarget->GetActorLocation().Z + viewerPawn->BaseEyeHeight;
        }
    }

    return lookAtPoint;
}

APawn* UATSBaseTargetComponent::GetViewerPawn() const
{
    if (const AController* controller = Cast<AController>(GetOwner())) {
        return controller->GetPawn();
    }

    return Cast<APawn>(GetOwner());
}
