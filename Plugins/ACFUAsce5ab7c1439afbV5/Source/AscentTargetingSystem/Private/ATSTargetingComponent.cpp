// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ATSTargetingComponent.h"
#include "ATSTargetPointComponent.h"
#include "ATSTargetableInterface.h"
#include "CCMCameraFunctionLibrary.h"
#include "Interfaces/ACFEntityInterface.h"
#include <Camera/PlayerCameraManager.h>
#include <Components/ActorComponent.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <GameFramework/Character.h>
#include <GameFramework/Controller.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <Kismet/GameplayStatics.h>
#include <Kismet/KismetMathLibrary.h>
#include <Kismet/KismetSystemLibrary.h>

// Sets default values for this component's properties
UATSTargetingComponent::UATSTargetingComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;

    // Assigned directly: the setter touches the camera manager, which does not exist yet here.
    TargetingType = ETargetingType::EMagneticLockYawOnly;
    SetMaxTargetingDistance(1500.f);
    SetComponentTickEnabled(false);
}

// Called when the game starts
void UATSTargetingComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(false);
    SetOwnerReferences();
}

void UATSTargetingComponent::ToggleTargeting()
{
    TriggerTargeting(!bIsTargeting);
}

// Called every frame
void UATSTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bCanTarget && FMath::Abs(Xvalue) < deadband && FMath::Abs(YValue) < deadband) {
        bCanTarget = true;
    }

    UpdateTargeting(DeltaTime);
}

void UATSTargetingComponent::TriggerTargeting(bool bActivate)
{
    bIsTargeting = bActivate;
    if (bIsTargeting) {
        ActivateTargeting();
    } else {
        SetCurrentTarget(nullptr);
    }
    bIsTargeting = CurrentTarget != nullptr;
    // UpdateTargeting runs only from the tick, and it is what notices the target dying or leaving
    // range. Nothing else here needs ticking, so it follows the lock.
    SetComponentTickEnabled(bIsTargeting);
    OnTargetingStateChanged.Broadcast(bIsTargeting);
}

void UATSTargetingComponent::AddFilter(TSubclassOf<UATSTargetingFilter> filterClass)
{
    UATSTargetingFilter* newFilter = NewObject<UATSTargetingFilter>(filterClass);
    TargetFilters.AddUnique(newFilter);
}

void UATSTargetingComponent::AddObjectType(TEnumAsByte<EObjectTypeQuery> objectTypeToTrace)
{
    if (!ObjectsToQuery.Contains(objectTypeToTrace)) {
        ObjectsToQuery.Add(objectTypeToTrace);
    }
}

void UATSTargetingComponent::RemoveObjectType(TEnumAsByte<EObjectTypeQuery> objectTypeToTrace)
{
    if (ObjectsToQuery.Contains(objectTypeToTrace)) {
        ObjectsToQuery.Remove(objectTypeToTrace);
    }
}

bool UATSTargetingComponent::RemoveFilter(TSubclassOf<UATSTargetingFilter> filterClass)
{
    int32 index = -1;
    for (UATSTargetingFilter* filter : TargetFilters) {
        if (filter->IsA(filterClass)) {
            index = TargetFilters.IndexOfByKey(filter);
            break;
        }
    }
    if (TargetFilters.IsValidIndex(index)) {
        TargetFilters.RemoveAt(index);
        return true;
    }
    return false;
}

void UATSTargetingComponent::ActivateTargeting()
{
    AActor* currentTarget = nullptr;
    auto* controlledPawn = GetControlledPawn();

    const APlayerCameraManager* cameraManager = GetCameraManager();

    if (controlledPawn && cameraManager) {
        PopulatePotentialTargetsArray();
        FVector cameraForwardVector = UKismetMathLibrary::GetForwardVector(cameraManager->GetCameraRotation());

        cameraForwardVector.Z = 0;
        cameraForwardVector = cameraForwardVector.GetSafeNormal();
        float maxDistance = BIG_NUMBER;
        float maxDirection = -1.0f;
        for (AActor* target : availableTargets) {

            if (target && target->WasRecentlyRendered() && IsValidTarget(target)) {

                float distance = target->GetDistanceTo(controlledPawn);

                switch (GetTargetSelectionType()) {
                case ETargetSelectionType::ENearestTarget:

                    if (distance < maxDistance) {
                        currentTarget = target;
                        maxDistance = distance;
                    }
                    break;
                case ETargetSelectionType::EForwardTarget:
                    FVector vdistance = target->GetActorLocation() - controlledPawn->GetActorLocation();
                    vdistance = vdistance.GetUnsafeNormal();
                    float direction = FVector::DotProduct(cameraForwardVector, vdistance);
                    if (direction > maxDirection) {
                        currentTarget = target;
                        maxDirection = direction;
                    }
                    break;
                }
            }
        }

        // Outside the loop: picking inside it locked every candidate in turn, firing target
        // changed events and camera events for each one before settling on the best.
        if (currentTarget) {
            SetCurrentTarget(currentTarget);
        } else {
            UE_LOG(LogTemp, Warning, TEXT("No Availble Target"));
        }
    }
}

TArray<AActor*> UATSTargetingComponent::GetAllTargetsByDirection(ETargetingDirection direction)
{
    TArray<AActor*> actorFilter;
    for (AActor* target : availableTargets) {
        if (target != CurrentTarget && IsValidTarget(target)) {
            FVector PotentialTargetLocation = target->GetActorLocation();

            // In front of the lens, not inside the 60° acquisition cone. Free-flow framing keeps
            // fighters beside or behind the player on screen; that cone would reject almost all of
            // them and the stick would never swap.
            if (!IsInFrontOfOwner(target)) {
                continue;
            }
            switch (direction) {
            case ETargetingDirection::ERight:
                if (IsRightOfCurrentTarget(PotentialTargetLocation)) {
                    actorFilter.AddUnique(target);
                }
                break;
            case ETargetingDirection::ELeft:
                if (!IsRightOfCurrentTarget(PotentialTargetLocation)) {
                    actorFilter.AddUnique(target);
                }
                break;
            case ETargetingDirection::EDown:
                if (!IsUpOfCurrentTarget(PotentialTargetLocation)) {
                    actorFilter.AddUnique(target);
                }
                break;
            case ETargetingDirection::EUp:
                if (IsUpOfCurrentTarget(PotentialTargetLocation)) {
                    actorFilter.AddUnique(target);
                }
                break;
            default:
                break;
            }
        }
    }

    return actorFilter;
}

bool UATSTargetingComponent::TrySwitchPointOnCurrentTarget(ETargetingDirection direction)
{
    UATSTargetPointComponent* bestpoint = nullptr;
    TArray<UATSTargetPointComponent*> potentialTargets;
    if (CurrentTarget && CurrentTargetPoint) {
        TArray<UActorComponent*> points;
        CurrentTarget->GetComponents(UATSTargetPointComponent::StaticClass(), points, true);
        if (points.Num() > 1) {
            for (auto point : points) {
                UATSTargetPointComponent* targetpoint = Cast<UATSTargetPointComponent>(point);
                if (targetpoint && targetpoint != CurrentTargetPoint) {
                    FVector PotentialTargetLocation = targetpoint->GetComponentLocation();

                    switch (direction) {
                    case ETargetingDirection::ERight:
                        if (IsRightOfCurrentTarget(PotentialTargetLocation)) {
                            potentialTargets.Add(targetpoint);
                        }
                        break;
                    case ETargetingDirection::ELeft:
                        if (!IsRightOfCurrentTarget(PotentialTargetLocation)) {
                            potentialTargets.Add(targetpoint);
                        }
                        break;
                    case ETargetingDirection::EDown:
                        if (!IsUpOfCurrentTarget(PotentialTargetLocation)) {
                            potentialTargets.Add(targetpoint);
                        }
                        break;
                    case ETargetingDirection::EUp:
                        if (IsUpOfCurrentTarget(PotentialTargetLocation)) {
                            potentialTargets.Add(targetpoint);
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    if (potentialTargets.Num() == 0) {
        return false;
    } else {
        UATSTargetPointComponent* finaltarget = GetNearestTargetPoint(potentialTargets);
        UpdateCurrentTargetPoint(finaltarget);
    }

    return false;
}

bool UATSTargetingComponent::IsRightOfCurrentTarget(FVector LocationToCheck)
{
    const APlayerCameraManager* cameraManager = GetCameraManager();
    if (!cameraManager) {
        return false;
    }

    // Camera space, not a screen projection: ProjectWorldLocationToScreen fails (and leaves 0,0)
    // for points near or behind the lens, which is exactly where a framed target on the boom sits.
    const FRotator camRot = cameraManager->GetCameraRotation();
    const FVector camLoc = cameraManager->GetCameraLocation();
    const FVector currentRel = camRot.UnrotateVector(GetCurrentTargetPointLocation() - camLoc);
    const FVector checkRel = camRot.UnrotateVector(LocationToCheck - camLoc);
    return checkRel.Y >= currentRel.Y;
}

bool UATSTargetingComponent::IsUpOfCurrentTarget(FVector LocationToCheck)
{
    const APlayerCameraManager* cameraManager = GetCameraManager();
    if (!cameraManager) {
        return false;
    }

    const FRotator camRot = cameraManager->GetCameraRotation();
    const FVector camLoc = cameraManager->GetCameraLocation();
    const FVector currentRel = camRot.UnrotateVector(GetCurrentTargetPointLocation() - camLoc);
    const FVector checkRel = camRot.UnrotateVector(LocationToCheck - camLoc);
    return checkRel.Z >= currentRel.Z;
}

AActor* UATSTargetingComponent::GetNearestTarget(TArray<AActor*>& actorFilter)
{
    AActor* localTarget = nullptr;
    auto* controlledPawn = GetControlledPawn();
    if (!controlledPawn) {
        return nullptr;
    }
    if (actorFilter.Num() > 0) {
        float distanceFromCurrentTarget = 999999.f;

        for (int32 i = 0; i < actorFilter.Num(); ++i) {
            if (!actorFilter[i]) {
                continue;
            }
            float distance = actorFilter[i]->GetDistanceTo(controlledPawn);
            if (distance < distanceFromCurrentTarget) {
                distanceFromCurrentTarget = distance;
                localTarget = actorFilter[i];
            }
        }
    }
    return localTarget;
}

UATSTargetPointComponent* UATSTargetingComponent::GetNearestTargetPoint(TArray<UATSTargetPointComponent*> points)
{
    UATSTargetPointComponent* localTarget = nullptr;
    if (points.Num() > 0) {
        float distanceFromCurrentTarget = 999999.f;

        for (int32 i = 0; i < points.Num(); ++i) {
            if (!points.IsValidIndex(i)) {
                continue;
            }

            float distance = FVector::Distance(points[i]->GetComponentLocation(), CurrentTargetPoint->GetComponentLocation());
            if (distance < distanceFromCurrentTarget) {
                distanceFromCurrentTarget = distance;
                localTarget = points[i];
            }
        }
    }
    return localTarget;
}

bool UATSTargetingComponent::IsTargetAlive(AActor* target) const
{
    // Actors that are not entities have no notion of death, so they always pass.
    if (target && target->GetClass()->ImplementsInterface(UACFEntityInterface::StaticClass())) {
        return IACFEntityInterface::Execute_IsEntityAlive(target);
    }

    return true;
}

bool UATSTargetingComponent::IsCurrentTargetStillValid() const
{
    auto* controlledPawn = GetControlledPawn();
    if (!controlledPawn) {
        return false;
    }
    AActor* target = GetCurrentTarget();
    if (!target || target->IsPendingKillPending()) {
        return false;
    }

    if (target->GetDistanceTo(controlledPawn) >= GetMaxTargetingDistance()) {
        return false;
    }

    if (bCheckLineSight && !IsTargetInSight()) {
        return false;
    }

    return IsTargetAlive(target);
}

bool UATSTargetingComponent::IsValidTarget(AActor* target)
{
    auto* controlledPawn = GetControlledPawn();

    if (target && controlledPawn && target != controlledPawn) {
        bool bImplements = target->GetClass()->ImplementsInterface(UATSTargetableInterface::StaticClass());
        if (bImplements) {
            // A corpse is still a valid targetable actor in the world, so without this the lock
            // would just hop back onto the enemy that has died.
            if (!IsTargetAlive(target)) {
                return false;
            }
            for (UATSTargetingFilter* filter : TargetFilters) {
                if (filter && !filter->IsActorTargetable(GetOwner(), target)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

bool UATSTargetingComponent::IsInFrontOfOwner(AActor* target)
{
    const APlayerCameraManager* cameraManager = GetCameraManager();
    if (!cameraManager || !target) {
        return false;
    }

    // Hemisphere in front of the camera, so a target already in the shot can be switched to even
    // when it sits beside or behind the player. The acquisition cone is for the first lock, not
    // for cycling with the stick.
    const FVector toTarget = target->GetActorLocation() - cameraManager->GetCameraLocation();
    return FVector::DotProduct(cameraManager->GetCameraRotation().Vector(), toTarget) > 0.f;
}

void UATSTargetingComponent::UpdateTargeting(float deltaTime)
{
    if (!bIsTargeting || IsCurrentTargetStillValid()) {
        return;
    }

    // Let go of the old target first: it may be a corpse, and everything reading the lock - body
    // rotation, aim offset, reticle - must stop pointing at it this same frame.
    SetCurrentTarget(nullptr);

    // Then hand the lock to the next enemy instead of kicking the player out of targeting mode
    // mid-fight. TriggerTargeting settles the targeting state and notifies listeners, so if
    // nothing else is targetable the lock simply turns off.
    TriggerTargeting(true);
}

void UATSTargetingComponent::SetCurrentTarget(AActor* target)
{

    if (CurrentTarget) {
        const bool bImplements = CurrentTarget->GetClass()->ImplementsInterface(UATSTargetableInterface::StaticClass());
        if (bImplements) {
            IATSTargetableInterface::Execute_OnTargetLosed(CurrentTarget);
        }
        CurrentTarget = nullptr;
        CurrentTargetPoint = nullptr;
    }
    CleanCamera();

    if (target) {
        CurrentTarget = target;
        bCanTarget = false;
        UpdateCurrentTargetPoint(GetBestTargetPointForTarget(target));
    }
    UpdateCameraBehaviour();
    SetTarget(CurrentTarget);
    OnTargetChanged.Broadcast(CurrentTarget);
}

void UATSTargetingComponent::SetTargetingType(ETargetingType val)
{
    TargetingType = val;
    // The camera behaviour is part of the targeting type, so switching it mid-lock has to reach the
    // camera manager right away.
    UpdateCameraBehaviour();
}

bool UATSTargetingComponent::GetIsFramingBothActors() const
{
    return TargetingType == ETargetingType::EFrameBothActors;
}

void UATSTargetingComponent::SetActorFramingSettings(const FCCMActorFramingSettings& inSettings)
{
    ActorFramingSettings = inSettings;
    UpdateCameraBehaviour();
}

void UATSTargetingComponent::UpdateCameraBehaviour()
{
    // Exactly one call into the camera manager per targeting type, and the manager owns the camera
    // from there: asking for the framing gives up the lock and the other way round, so the two can
    // never end up writing the same control rotation.
    if (!CurrentTarget) {
        UCCMCameraFunctionLibrary::StopLockingCameraOnActor(this);
        UCCMCameraFunctionLibrary::StopFramingActors(this);
        return;
    }

    switch (TargetingType) {
    case ETargetingType::EFrameBothActors:
        UCCMCameraFunctionLibrary::StartFramingActors(this, CurrentTarget, ActorFramingSettings, GetLockMagnetism());
        break;
    case ETargetingType::EMagneticLock:
    case ETargetingType::EMagneticLockYawOnly:
        // The target point is what the lock centers on when the target has one, since it is authored
        // on the chest or the head instead of the actor's feet.
        if (CurrentTargetPoint) {
            UCCMCameraFunctionLibrary::LockCameraOnComponent(this, CurrentTargetPoint, GetLockTypeByTargetingType(TargetingType), GetLockMagnetism());
        } else {
            UCCMCameraFunctionLibrary::LockCameraOnActor(this, CurrentTarget, GetLockTypeByTargetingType(TargetingType), GetLockMagnetism());
        }
        break;
    case ETargetingType::EDontLock:
    default:
        UCCMCameraFunctionLibrary::StopLockingCameraOnActor(this);
        UCCMCameraFunctionLibrary::StopFramingActors(this);
        break;
    }
}

void UATSTargetingComponent::UpdateCurrentTargetPoint(UATSTargetPointComponent* inCurrentTargetPoint)
{
    const bool bImplements = CurrentTarget->GetClass()->ImplementsInterface(UATSTargetableInterface::StaticClass());
    if (bImplements) {
        SetTargetPoint(inCurrentTargetPoint);
        CurrentTargetPoint = inCurrentTargetPoint;
        CleanCamera();
        IATSTargetableInterface::Execute_OnTargetAcquired(CurrentTarget, inCurrentTargetPoint);
        if (CurrentTargetPoint) {
            currentCameraEvent = CurrentTargetPoint->GetPointCameraEvent();
            if (currentCameraEvent != NAME_None) {
                UCCMCameraFunctionLibrary::TriggerCameraEvent(this, currentCameraEvent);
            }
            UpdateCameraBehaviour();
        }
    }
    bCanTarget = false;
}

void UATSTargetingComponent::CleanCamera()
{
    if (currentCameraEvent != NAME_None) {
        UCCMCameraFunctionLibrary::StopCameraEvent(this, currentCameraEvent);
        currentCameraEvent = NAME_None;
    }
}

UATSTargetPointComponent* UATSTargetingComponent::GetBestTargetPointForTarget(AActor* target)
{
    UATSTargetPointComponent* bestpoint = nullptr;
    float maxDirection = -1.f;
    auto* controlledPawn = GetControlledPawn();
    const APlayerCameraManager* cameraManager = GetCameraManager();
    if (!controlledPawn || !cameraManager) {
        return nullptr;
    }

    if (target) {
        TArray<UActorComponent*> points;
        target->GetComponents(UATSTargetPointComponent::StaticClass(), points, true);
        FVector cameraForwardVector = UKismetMathLibrary::GetForwardVector(cameraManager->GetCameraRotation());

        cameraForwardVector.Z = 0;
        cameraForwardVector = cameraForwardVector.GetSafeNormal();

        if (points.Num() != 0) {
            for (auto point : points) {
                UATSTargetPointComponent* targetpoint = Cast<UATSTargetPointComponent>(point);
                if (targetpoint) {
                    FVector distance = targetpoint->GetComponentLocation() - controlledPawn->GetActorLocation();
                    distance = distance.GetUnsafeNormal();
                    const float direction = FVector::DotProduct(cameraForwardVector, distance);
                    if (direction > maxDirection) {
                        bestpoint = targetpoint;
                        maxDirection = direction;
                    }
                }
            }
        }
    }

    return bestpoint;
}

void UATSTargetingComponent::RightSearchTargetWithInput(float InputValue)
{
    Xvalue = InputValue;
    if (bIsTargeting && FMath::Abs(InputValue) > InputTrasholdForSearch && bCanTarget) {
        ETargetingDirection direction;
        if (InputValue < 0) {
            direction = ETargetingDirection::ELeft;
        } else {
            direction = ETargetingDirection::ERight;
        }

        SwitchTargetByDirection(direction);
        return;
    }
}

ELockType UATSTargetingComponent::GetLockTypeByTargetingType(ETargetingType targetType)
{
    switch (targetType) {
    case ETargetingType::EDontLock:
        break;
    case ETargetingType::EMagneticLock:
        return ELockType::EAllAxis;
        break;
    case ETargetingType::EMagneticLockYawOnly:
        return ELockType::EYawOnly;
        break;
    default:
        break;
    }
    return ELockType::EAllAxis;
}

void UATSTargetingComponent::PopulatePotentialTargetsArray()
{
    auto* controlledPawn = GetControlledPawn();
    if (!controlledPawn)
    {
        return;
    }
    if (controlledPawn) {

        availableTargets.Empty();
        TArray<AActor*> ignoredActors;
        ignoredActors.Add(controlledPawn);
        TArray<AActor*> potentialTargets;
        UKismetSystemLibrary::SphereOverlapActors(this, controlledPawn->GetActorLocation(),
            GetMaxTargetingDistance(), ObjectsToQuery, AActor::StaticClass(), ignoredActors, potentialTargets);

        if (bCheckLineSight) {
            for (int32 i = potentialTargets.Num() - 1; i >= 0; i--) {
                if (!IsPotentialTargetInSight(potentialTargets[i])) {
                    potentialTargets.RemoveAt(i);
                }
            }
        }

        availableTargets = potentialTargets;
    } else {
        SetOwnerReferences();
    }
}

void UATSTargetingComponent::SwitchTargetByDirection(ETargetingDirection direction)
{
    if (TrySwitchPointOnCurrentTarget(direction)) {
        return;
    }

    PopulatePotentialTargetsArray();
    TArray<AActor*> actorFilter = GetAllTargetsByDirection(direction);

    AActor* localCurrentTarget = GetNearestTarget(actorFilter);

    if (localCurrentTarget && localCurrentTarget != CurrentTarget) {
        SetCurrentTarget(localCurrentTarget);

        return;
    }
}

void UATSTargetingComponent::UpSearchTargetWithInput(float InputValue)
{
    YValue = InputValue;

    if (bIsTargeting && FMath::Abs(InputValue) > InputTrasholdForSearch && bCanTarget) {
        ETargetingDirection direction;
        if (InputValue < 0) {
            direction = ETargetingDirection::EDown;
        } else {
            direction = ETargetingDirection::EUp;
        }

        SwitchTargetByDirection(direction);
        return;
    }
}

APawn* UATSTargetingComponent::GetControlledPawn() const
{
    if (const AController* controller = Cast<AController>(GetOwner())) {
        return controller->GetPawn();
    }

    return Cast<APawn>(GetOwner());
}

APlayerCameraManager* UATSTargetingComponent::GetCameraManager() const
{
    if (const APlayerController* playerController = Cast<APlayerController>(GetOwner())) {
        return playerController->PlayerCameraManager;
    }

    return nullptr;
}

void UATSTargetingComponent::SetOwnerReferences()
{
    CurrentTarget = nullptr;
}
