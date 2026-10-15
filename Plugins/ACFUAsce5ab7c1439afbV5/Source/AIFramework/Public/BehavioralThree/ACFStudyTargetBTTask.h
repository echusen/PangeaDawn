// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "CoreMinimal.h"

#include "ACFStudyTargetBTTask.generated.h"

class AACFAIController;
class UACFDefenseStanceComponent;

/*Per instance state of the orbit, so that every agent keeps his own direction and his own timer*/
struct FACFStudyTargetMemory {
    float ElapsedTime = 0.f;
    float StudyDuration = 0.f;
    float TimeToNextGoal = 0.f;
    float OrbitRadius = 0.f;
    /*1 = clockwise, -1 = counter clockwise*/
    int32 OrbitDirection = 1;
    bool bIsDefending = false;
};

/**
 * Makes the agent actually circle around his current target instead of doing the small
 * back and forth adjustments an EQS point query produces: every GoalUpdateInterval a new
 * goal is placed on the circle of radius OrbitRadius, GoalLookAheadAngle degrees ahead in
 * the orbit direction chosen when the task started, so the agent always has somewhere to go
 * and never reaches the goal until the study time is over.
 *
 * Whether the agent looks at his target while circling is decided by the Combat Behaviour
 * data asset: set the Study Target state to override the rotation mode with Strafing.
 */
UCLASS()
class AIFRAMEWORK_API UACFStudyTargetBTTask : public UBTTaskNode {
    GENERATED_BODY()

public:
    UACFStudyTargetBTTask();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FACFStudyTargetMemory); }

    virtual FString GetStaticDescription() const override;

protected:
    /*Radius of the circle the agent walks on. If left to 0, the ideal distance configured for the
    Study Target state in the Combat Behaviour data asset is used*/
    UPROPERTY(EditAnywhere, Category = "ACF|Orbit")
    float OrbitRadiusOverride = 0.f;

    /*How many degrees ahead on the circle the next goal is placed. Small values make the agent
    hug the circle but stutter, large values make him cut the corner and get closer to the target*/
    UPROPERTY(EditAnywhere, meta = (ClampMin = "5.0", ClampMax = "170.0"), Category = "ACF|Orbit")
    float GoalLookAheadAngle = 45.f;

    /*How many goals are tried before giving up on the current orbit direction. Every attempt shortens
    the look ahead angle (45, 30, 15 degrees with the default values), so the agent slides along the
    circle looking for a valid spot instead of turning around at the first obstacle*/
    UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "8"), Category = "ACF|Orbit")
    int32 GoalSearchAttempts = 3;

    /*Seconds between two goal updates. The actual orbit speed comes from the Locomotion State
    configured for the Study Target combat state*/
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.05"), Category = "ACF|Orbit")
    float GoalUpdateInterval = 0.4f;

    /*Chance, evaluated at every goal update, of inverting the orbit direction*/
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "100.0"), Category = "ACF|Orbit")
    float DirectionChangeChance = 8.f;

    /*Minimum time the agent keeps circling before this task succeeds*/
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"), Category = "ACF|Orbit")
    float MinStudyDuration = 2.5f;

    /*Maximum time the agent keeps circling before this task succeeds*/
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"), Category = "ACF|Orbit")
    float MaxStudyDuration = 5.f;

    /*If true, the agent raises his guard through the Defense Stance Component for the whole
    duration of the orbit, and lowers it as soon as the task ends*/
    UPROPERTY(EditAnywhere, Category = "ACF|Defense")
    bool bDefendWhileStudying = true;

    UPROPERTY(EditAnywhere, Category = "ACF|Movement")
    bool bUsePathfinding = true;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"), Category = "ACF|Movement")
    float GoalAcceptanceRadius = 40.f;

    /*Extent used to project the computed orbit point on the NavMesh*/
    UPROPERTY(EditAnywhere, Category = "ACF|Movement")
    FVector NavProjectionExtent = FVector(200.f, 200.f, 300.f);

    /*If true, a goal from which the agent would lose sight of his target is discarded. This is what
    keeps the orbit from sliding behind a pillar or a wall, and replaces the visibility test the
    EQS based Study Target used to do*/
    UPROPERTY(EditAnywhere, Category = "ACF|Sight")
    bool bRequireLineOfSightToTarget = true;

    /*Channel used by the line of sight trace*/
    UPROPERTY(EditAnywhere, meta = (EditCondition = "bRequireLineOfSightToTarget"), Category = "ACF|Sight")
    TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;

    /*Height at which the line of sight trace is done, both on the candidate goal and on the target,
    so that it is not blocked by the ground itself*/
    UPROPERTY(EditAnywhere, meta = (EditCondition = "bRequireLineOfSightToTarget", ClampMin = "0.0"), Category = "ACF|Sight")
    float LineOfSightHeightOffset = 60.f;

    UPROPERTY(EditAnywhere, Category = "ACF|Debug")
    bool bShowDebug = false;

private:
    /*Places the next goal on the orbit and requests the move. Returns false if no valid point
    could be found in the current orbit direction*/
    bool TryMoveToNextOrbitPoint(AACFAIController& aiController, const APawn& pawnOwner, const AActor& target, FACFStudyTargetMemory& memory) const;

    /*Computes the orbit point lookAheadAngle degrees ahead of the agent and projects it on the NavMesh.
    Returns false when there is no navigable ground there*/
    bool TryGetOrbitPoint(const APawn& pawnOwner, const AActor& target, const FACFStudyTargetMemory& memory, float lookAheadAngle, FVector& outGoal) const;

    bool HasLineOfSightToTarget(const APawn& pawnOwner, const AActor& target, const FVector& fromLocation) const;

    float GetOrbitRadius(const AACFAIController& aiController, const APawn& pawnOwner, const AActor& target) const;

    void StopStudying(UBehaviorTreeComponent& OwnerComp, FACFStudyTargetMemory& memory) const;

    UACFDefenseStanceComponent* TryGetDefenseStance(const APawn* pawnOwner) const;
};
