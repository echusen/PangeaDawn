// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "BehavioralThree/ACFStudyTargetBTTask.h"
#include "ACFAIController.h"
#include "Components/ACFCombatBehaviourComponent.h"
#include "Components/ACFDefenseStanceComponent.h"
#include <BehaviorTree/BehaviorTreeComponent.h>
#include <CollisionQueryParams.h>
#include <DrawDebugHelpers.h>
#include <Engine/World.h>
#include <GameFramework/Pawn.h>
#include <NavigationSystem.h>
#include <Navigation/PathFollowingComponent.h>

UACFStudyTargetBTTask::UACFStudyTargetBTTask()
{
    NodeName = "Study Target (Circle Around)";
    INIT_TASK_NODE_NOTIFY_FLAGS();
}

EBTNodeResult::Type UACFStudyTargetBTTask::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FACFStudyTargetMemory* memory = reinterpret_cast<FACFStudyTargetMemory*>(NodeMemory);
    *memory = FACFStudyTargetMemory();

    AACFAIController* aiController = Cast<AACFAIController>(OwnerComp.GetAIOwner());
    APawn* pawnOwner = aiController ? aiController->GetPawn() : nullptr;
    AActor* target = aiController ? aiController->GetTargetActorBK() : nullptr;

    if (!pawnOwner || !target) {
        return EBTNodeResult::Failed;
    }

    memory->OrbitDirection = FMath::RandBool() ? 1 : -1;
    memory->StudyDuration = FMath::FRandRange(FMath::Min(MinStudyDuration, MaxStudyDuration), FMath::Max(MinStudyDuration, MaxStudyDuration));
    memory->OrbitRadius = GetOrbitRadius(*aiController, *pawnOwner, *target);

    if (bDefendWhileStudying) {
        UACFDefenseStanceComponent* defenseComp = TryGetDefenseStance(pawnOwner);
        if (defenseComp && !defenseComp->IsInDefensePosition() && defenseComp->CanStartDefensePosition()) {
            defenseComp->StartDefending();
            memory->bIsDefending = true;
        }
    }

    // The first goal is placed right away so that the agent starts moving on the very first frame
    // instead of standing still until the first update interval has elapsed.
    TryMoveToNextOrbitPoint(*aiController, *pawnOwner, *target, *memory);
    memory->TimeToNextGoal = GoalUpdateInterval;

    return EBTNodeResult::InProgress;
}

void UACFStudyTargetBTTask::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    FACFStudyTargetMemory* memory = reinterpret_cast<FACFStudyTargetMemory*>(NodeMemory);

    AACFAIController* aiController = Cast<AACFAIController>(OwnerComp.GetAIOwner());
    APawn* pawnOwner = aiController ? aiController->GetPawn() : nullptr;
    AActor* target = aiController ? aiController->GetTargetActorBK() : nullptr;

    if (!pawnOwner || !target) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    memory->ElapsedTime += DeltaSeconds;
    if (memory->ElapsedTime >= memory->StudyDuration) {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    memory->TimeToNextGoal -= DeltaSeconds;

    // A new goal is placed either on the timer or as soon as the agent has nothing left to follow,
    // so that a goal reached earlier than expected does not leave him standing.
    const bool bHasNoMove = aiController->GetMoveStatus() == EPathFollowingStatus::Idle;
    if (memory->TimeToNextGoal > 0.f && !bHasNoMove) {
        return;
    }

    if (DirectionChangeChance > 0.f && FMath::FRandRange(0.f, 100.f) <= DirectionChangeChance) {
        memory->OrbitDirection *= -1;
    }

    if (!TryMoveToNextOrbitPoint(*aiController, *pawnOwner, *target, *memory)) {
        // The circle is blocked on this side (a wall, a cliff, another agent): turn around and
        // keep orbiting the other way instead of giving up the state.
        memory->OrbitDirection *= -1;
    }

    memory->TimeToNextGoal = GoalUpdateInterval;
}

EBTNodeResult::Type UACFStudyTargetBTTask::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FACFStudyTargetMemory* memory = reinterpret_cast<FACFStudyTargetMemory*>(NodeMemory);
    StopStudying(OwnerComp, *memory);

    return EBTNodeResult::Aborted;
}

void UACFStudyTargetBTTask::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    FACFStudyTargetMemory* memory = reinterpret_cast<FACFStudyTargetMemory*>(NodeMemory);
    StopStudying(OwnerComp, *memory);

    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

bool UACFStudyTargetBTTask::TryMoveToNextOrbitPoint(AACFAIController& aiController, const APawn& pawnOwner, const AActor& target, FACFStudyTargetMemory& memory) const
{
#if ENABLE_DRAW_DEBUG
    if (bShowDebug && pawnOwner.GetWorld()) {
        DrawDebugCircle(pawnOwner.GetWorld(), target.GetActorLocation(), memory.OrbitRadius, 48, FColor::Yellow, false, GoalUpdateInterval * 2.f, 0, 2.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
    }
#endif

    // The goal is looked for from the full look ahead angle downwards: if the spot the agent would
    // naturally reach is not navigable, or it would break the sight on the target, the agent slides
    // to a nearer point on the same circle instead of immediately turning around.
    const int32 totalAttempts = FMath::Max(GoalSearchAttempts, 1);
    for (int32 attempt = 0; attempt < totalAttempts; attempt++) {
        const float lookAheadAngle = GoalLookAheadAngle * (totalAttempts - attempt) / totalAttempts;

        FVector goalLocation;
        if (!TryGetOrbitPoint(pawnOwner, target, memory, lookAheadAngle, goalLocation)) {
            continue;
        }

        const bool bHasSight = !bRequireLineOfSightToTarget || HasLineOfSightToTarget(pawnOwner, target, goalLocation);

#if ENABLE_DRAW_DEBUG
        if (bShowDebug && pawnOwner.GetWorld()) {
            const FColor goalColor = bHasSight ? (memory.OrbitDirection > 0 ? FColor::Green : FColor::Cyan) : FColor::Red;
            DrawDebugSphere(pawnOwner.GetWorld(), goalLocation, 35.f, 12, goalColor, false, GoalUpdateInterval * 2.f);
        }
#endif

        if (!bHasSight) {
            continue;
        }

        // bCanStrafe is requested so that the path following does not fight the Gameplay focus the
        // targeting component keeps on the target while the agent walks the circle.
        const EPathFollowingRequestResult::Type moveResult = aiController.MoveToLocation(goalLocation, GoalAcceptanceRadius, false, bUsePathfinding, false, true);

        if (moveResult != EPathFollowingRequestResult::Failed) {
            return true;
        }
    }

    return false;
}

bool UACFStudyTargetBTTask::TryGetOrbitPoint(const APawn& pawnOwner, const AActor& target, const FACFStudyTargetMemory& memory, float lookAheadAngle, FVector& outGoal) const
{
    const FVector pawnLocation = pawnOwner.GetActorLocation();
    const FVector targetLocation = target.GetActorLocation();

    FVector orbitOffset = pawnLocation - targetLocation;
    orbitOffset.Z = 0.f;

    if (orbitOffset.IsNearlyZero()) {
        // Perfectly overlapping the target: start the circle behind the agent's own back.
        orbitOffset = -pawnOwner.GetActorForwardVector().GetSafeNormal2D();
    }

    const float stepDegrees = lookAheadAngle * memory.OrbitDirection;
    const FVector nextOffset = orbitOffset.GetSafeNormal2D().RotateAngleAxis(stepDegrees, FVector::UpVector) * memory.OrbitRadius;

    outGoal = targetLocation + nextOffset;
    outGoal.Z = pawnLocation.Z;

    UNavigationSystemV1* navSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(pawnOwner.GetWorld());
    if (!navSys) {
        return true;
    }

    FNavLocation projectedGoal;
    if (!navSys->ProjectPointToNavigation(outGoal, projectedGoal, NavProjectionExtent)) {
        return false;
    }

    outGoal = projectedGoal.Location;
    return true;
}

bool UACFStudyTargetBTTask::HasLineOfSightToTarget(const APawn& pawnOwner, const AActor& target, const FVector& fromLocation) const
{
    const UWorld* world = pawnOwner.GetWorld();
    if (!world) {
        return true;
    }

    const FVector traceOffset = FVector(0.f, 0.f, LineOfSightHeightOffset);
    const FVector traceStart = fromLocation + traceOffset;
    const FVector traceEnd = target.GetActorLocation() + traceOffset;

    FCollisionQueryParams queryParams(SCENE_QUERY_STAT(ACFStudyTargetSight), false, &pawnOwner);
    queryParams.AddIgnoredActor(&target);

    return !world->LineTraceTestByChannel(traceStart, traceEnd, LineOfSightChannel, queryParams);
}

float UACFStudyTargetBTTask::GetOrbitRadius(const AACFAIController& aiController, const APawn& pawnOwner, const AActor& target) const
{
    if (OrbitRadiusOverride > 0.f) {
        return OrbitRadiusOverride;
    }

    const UACFCombatBehaviourComponent* combatBehav = aiController.GetCombatBehavior();
    if (combatBehav) {
        const float idealDistance = combatBehav->GetIdealDistanceByCombatState(EAICombatState::EStudyTarget);
        if (idealDistance > 0.f) {
            return idealDistance;
        }
    }

    // No configuration at all: keep the distance the agent already has when he enters the state.
    return FMath::Max(FVector::Dist2D(pawnOwner.GetActorLocation(), target.GetActorLocation()), GoalAcceptanceRadius);
}

void UACFStudyTargetBTTask::StopStudying(UBehaviorTreeComponent& OwnerComp, FACFStudyTargetMemory& memory) const
{
    AACFAIController* aiController = Cast<AACFAIController>(OwnerComp.GetAIOwner());
    if (!aiController) {
        return;
    }

    if (memory.bIsDefending) {
        UACFDefenseStanceComponent* defenseComp = TryGetDefenseStance(aiController->GetPawn());
        if (defenseComp) {
            defenseComp->StopDefending();
        }
        memory.bIsDefending = false;
    }

    if (aiController->GetMoveStatus() != EPathFollowingStatus::Idle) {
        aiController->StopMovement();
    }
}

UACFDefenseStanceComponent* UACFStudyTargetBTTask::TryGetDefenseStance(const APawn* pawnOwner) const
{
    return pawnOwner ? pawnOwner->FindComponentByClass<UACFDefenseStanceComponent>() : nullptr;
}

FString UACFStudyTargetBTTask::GetStaticDescription() const
{
    const FString radiusDescription = OrbitRadiusOverride > 0.f
        ? FString::Printf(TEXT("%.0f cm"), OrbitRadiusOverride)
        : FString(TEXT("Study Target ideal distance"));

    return FString::Printf(TEXT("Circle around the current target\nRadius: %s\nStep: %.0f deg every %.2fs (%d attempts)\nDuration: %.1f - %.1fs\nKeep line of sight: %s\nDefend while studying: %s"),
        *radiusDescription, GoalLookAheadAngle, GoalUpdateInterval, GoalSearchAttempts, MinStudyDuration, MaxStudyDuration,
        bRequireLineOfSightToTarget ? TEXT("true") : TEXT("false"),
        bDefendWhileStudying ? TEXT("true") : TEXT("false"));
}
