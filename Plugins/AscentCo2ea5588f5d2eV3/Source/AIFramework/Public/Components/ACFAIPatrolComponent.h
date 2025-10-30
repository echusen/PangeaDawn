// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ACFAITypes.h"

#include "ACFAIPatrolComponent.generated.h"


UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class AIFRAMEWORK_API UACFAIPatrolComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFAIPatrolComponent();


protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    EPatrolType PatrolType;

    UPROPERTY(EditAnywhere, meta = (EditCondition = "PatrolType == EPatrolType::EFollowSpline"), BlueprintReadWrite, Category = ACF)
    class AACFPatrolPath* PathToFollow;

    UPROPERTY(EditAnywhere, meta = (EditCondition = "PatrolType == EPatrolType::ERandomPoint"), BlueprintReadWrite, Category = ACF)
    float RandomPatrolRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    float WaitTimeAtPoint;

public:
    UFUNCTION(BlueprintCallable, Category = ACF)
    class AACFPatrolPath* GetPathToFollow() const { return PathToFollow; }

    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetPathToFollow(class AACFPatrolPath* val) { PathToFollow = val; }

    UFUNCTION(BlueprintCallable, Category = ACF)
    bool TryGetNextWaypoint(FVector& outLocation);

    UFUNCTION(BlueprintPure, Category = ACF)
    EPatrolType GetPatrolType() const { return PatrolType; }

    UFUNCTION(BlueprintCallable, Category = ACF)
    void SetPatrolType(EPatrolType val) { PatrolType = val; }


    UFUNCTION(BlueprintPure, Category = ACF)
    float GetWaitTime() const;

private:

    int32 patrolIndex = 0;
};
