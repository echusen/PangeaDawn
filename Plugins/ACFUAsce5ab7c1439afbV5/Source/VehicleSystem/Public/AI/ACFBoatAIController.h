// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFBaseAIController.h"
#include "CoreMinimal.h"
#include <Perception/AIPerceptionTypes.h>

#include "ACFBoatAIController.generated.h"

class UACFThreatManagerComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;

/**
 * AI controller for physics-driven water vehicles (any pawn carrying a UACFWaterVehicleComponent).
 * Movement is NOT NavMesh based: the assigned behavior tree must use the Boat BT tasks that steer
 * the hull through AddThrust/AddSteering. Engagement is perception driven — hearing (primary,
 * fed by the water vehicle movement noise) plus sight — filtered by team attitude and routed
 * through the threat manager, which sets the TargetActor blackboard key.
 */
UCLASS()
class VEHICLESYSTEM_API AACFBoatAIController : public AACFBaseAIController {
    GENERATED_BODY()

public:
    AACFBoatAIController(const FObjectInitializer& ObjectInitializer);

    /*Sets the AI's target actor in the blackboard*/
    UFUNCTION(BlueprintCallable, Category = "ACF|Blackboard")
    void SetTargetActorBK(AActor* target);

    /*Retrieves the AI's target actor from the blackboard*/
    UFUNCTION(BlueprintPure, Category = "ACF|Blackboard")
    AActor* GetTargetActorBK() const;

    /*Sets the distance to the target actor in the blackboard*/
    UFUNCTION(BlueprintCallable, Category = "ACF|Blackboard")
    void SetTargetActorDistanceBK(float distance);

    // Returns the threat manager component, which tracks and prioritizes threats
    UFUNCTION(BlueprintPure, Category = "ACF|Getter")
    FORCEINLINE UACFThreatManagerComponent* GetThreatManager() const { return ThreatComponent; }

    // Returns the distance threshold at which the AI loses its current target
    UFUNCTION(BlueprintPure, Category = "ACF|Getter")
    FORCEINLINE float GetLoseTargetDistance() const { return LoseTargetDistance; }

    // Returns the behavior tree component managing the AI logic
    UFUNCTION(BlueprintPure, Category = "ACF|Getter")
    FORCEINLINE UBehaviorTreeComponent* GetBehaviorThreeComponent() const { return BehaviorTreeComponent; }

protected:
    virtual void OnPossess(APawn* possPawn) override;
    virtual void OnUnPossess() override;

    virtual void HandlePawnDamaged(const FACFDamageEvent& damageReceived) override;
    virtual void HandlePawnDeath() override;

    UFUNCTION()
    void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    UFUNCTION()
    void HandleMaxThreatUpdated(AActor* newMax);

    // The default behavior tree assigned to the AI boat
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF")
    TObjectPtr<UBehaviorTree> BehaviorTree;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ACF")
    TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ACF")
    TObjectPtr<UBlackboardComponent> BlackboardComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ACF")
    TObjectPtr<UACFThreatManagerComponent> ThreatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ACF|Perception")
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ACF|Perception")
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    /*Indicates if this boat attacks an enemy on perception*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|Combat")
    bool bIsAggressive = true;

    /*Distance over which the boat stops fighting its target*/
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|Combat")
    float LoseTargetDistance = 15000.f;
};
