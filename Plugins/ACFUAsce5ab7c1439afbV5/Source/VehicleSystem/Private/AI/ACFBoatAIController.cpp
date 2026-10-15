// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AI/ACFBoatAIController.h"
#include "ACFAIManagerSubsystem.h"
#include "ACFWaterVehicleComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/ACFThreatManagerComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Interfaces/ACFEntityInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"

namespace
{
const FName TargetActorKeyName = TEXT("TargetActor");
const FName TargetActorDistanceKeyName = TEXT("TargetActorDistance");
}

AACFBoatAIController::AACFBoatAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComp"));
    BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
    ThreatComponent = CreateDefaultSubobject<UACFThreatManagerComponent>(TEXT("ThreatManager"));

    UAIPerceptionComponent* perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
    SetPerceptionComponent(*perception);

    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 12000.f;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    perception->ConfigureSense(*HearingConfig);

    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 12000.f;
    SightConfig->LoseSightRadius = 15000.f;
    SightConfig->PeripheralVisionAngleDegrees = 180.f;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    perception->ConfigureSense(*SightConfig);

    perception->SetDominantSense(UAISense_Hearing::StaticClass());
}

void AACFBoatAIController::OnPossess(APawn* possPawn)
{
    Super::OnPossess(possPawn);

    if (!possPawn || !possPawn->FindComponentByClass<UACFWaterVehicleComponent>()) {
        UE_LOG(LogTemp, Error, TEXT("AACFBoatAIController possessed a pawn without a UACFWaterVehicleComponent: %s"), possPawn ? *possPawn->GetName() : TEXT("null"));
        return;
    }

    if (!BehaviorTree) {
        UE_LOG(LogTemp, Warning, TEXT("AACFBoatAIController on %s has no BehaviorTree assigned"), *possPawn->GetName());
        return;
    }

    UBlackboardData* const bbData = BehaviorTree->BlackboardAsset;
    if (!bbData) {
        UE_LOG(LogTemp, Warning, TEXT("Behavior tree %s has no Blackboard assigned"), *BehaviorTree->GetName());
        return;
    }

    BlackboardComponent->InitializeBlackboard(*bbData);
    Blackboard = BlackboardComponent;

    BehaviorTreeComponent->StartTree(*BehaviorTree);

    if (PerceptionComponent) {
        if (!PerceptionComponent->OnTargetPerceptionUpdated.IsAlreadyBound(this, &AACFBoatAIController::HandlePerceptionUpdated)) {
            PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AACFBoatAIController::HandlePerceptionUpdated);
        }
        PerceptionComponent->RequestStimuliListenerUpdate();
    }

    if (ThreatComponent && !ThreatComponent->OnNewMaxThreateningActor.IsAlreadyBound(this, &AACFBoatAIController::HandleMaxThreatUpdated)) {
        ThreatComponent->OnNewMaxThreateningActor.AddDynamic(this, &AACFBoatAIController::HandleMaxThreatUpdated);
    }
}

void AACFBoatAIController::OnUnPossess()
{
    if (BehaviorTreeComponent) {
        BehaviorTreeComponent->StopTree(EBTStopMode::Safe);
    }
    if (UACFAIManagerSubsystem* aiManager = GetWorld() ? GetWorld()->GetSubsystem<UACFAIManagerSubsystem>() : nullptr) {
        aiManager->RemoveAIFromBattle(this);
    }

    if (PerceptionComponent) {
        PerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(this, &AACFBoatAIController::HandlePerceptionUpdated);
    }

    if (ThreatComponent) {
        ThreatComponent->OnNewMaxThreateningActor.RemoveDynamic(this, &AACFBoatAIController::HandleMaxThreatUpdated);
    }

    Super::OnUnPossess();
}

void AACFBoatAIController::SetTargetActorBK(AActor* target)
{
    if (BlackboardComponent) {
        if (target) {
            BlackboardComponent->SetValueAsObject(TargetActorKeyName, target);
        } else {
            BlackboardComponent->ClearValue(TargetActorKeyName);
        }
    }

    // Keep the global battle state (music, encounter systems) in sync with this boat's combat
    if (UACFAIManagerSubsystem* aiManager = GetWorld() ? GetWorld()->GetSubsystem<UACFAIManagerSubsystem>() : nullptr) {
        if (target) {
            aiManager->AddAIToBattle(this);
        } else {
            aiManager->RemoveAIFromBattle(this);
        }
    }
}

AActor* AACFBoatAIController::GetTargetActorBK() const
{
    return BlackboardComponent ? Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetActorKeyName)) : nullptr;
}

void AACFBoatAIController::SetTargetActorDistanceBK(float distance)
{
    if (BlackboardComponent) {
        BlackboardComponent->SetValueAsFloat(TargetActorDistanceKeyName, distance);
    }
}

void AACFBoatAIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!bIsAggressive || !Actor || !GetPawn() || Actor == GetPawn()) {
        return;
    }

    if (GetPawn()->GetDistanceTo(Actor) > LoseTargetDistance) {
        return;
    }

    const IACFEntityInterface* entity = Cast<IACFEntityInterface>(Actor);
    if (entity && PossessedEntity && IACFEntityInterface::Execute_IsEntityAlive(Actor)
        && UACFFunctionLibrary::AreEnemyTeams(GetWorld(), IACFEntityInterface::Execute_GetEntityCombatTeam(GetPawn()), IACFEntityInterface::Execute_GetEntityCombatTeam(Actor))) {
        if (!ThreatComponent->IsThreatening(Actor) && ThreatComponent->IsActorAPotentialThreat(Actor)) {
            const float threat = ThreatComponent->GetDefaultThreatForActor(Actor);
            if (threat == 0.f) {
                return;
            }
            ThreatComponent->AddThreat(Actor, threat);
        }
    }
}

void AACFBoatAIController::HandleMaxThreatUpdated(AActor* newMax)
{
    SetTargetActorBK(newMax);
}

void AACFBoatAIController::HandlePawnDamaged(const FACFDamageEvent& damageReceived)
{
    Super::HandlePawnDamaged(damageReceived);

    // Retaliate even without perception: whoever shot us becomes a threat
    if (damageReceived.DamageDealer && damageReceived.DamageDealer != GetPawn() && ThreatComponent) {
        ThreatComponent->AddThreat(damageReceived.DamageDealer, damageReceived.FinalDamage);
    }
}

void AACFBoatAIController::HandlePawnDeath()
{
    Super::HandlePawnDeath();

    SetTargetActorBK(nullptr);
    if (ThreatComponent) {
        ThreatComponent->RemoveAllThreatenings();
    }
    if (BehaviorTreeComponent) {
        BehaviorTreeComponent->StopTree(EBTStopMode::Safe);
    }
}
