// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Components/ACFCombatBehaviourComponent.h"
#include "ACFAIController.h"
#include "ACFActionCondition.h"
#include "ACFCombatBehaviorDataAsset.h"
#include "ATSTargetingFunctionLibrary.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFAIManagerComponent.h"
#include "Components/ACFCharacterMovementComponent.h"
#include "Components/ACFEquipmentComponent.h"
#include "Game/ACFFunctionLibrary.h"
#include "Game/ACFTypes.h"
#include <GameFramework/GameStateBase.h>
#include <Kismet/GameplayStatics.h>
#include <Logging.h>

UACFCombatBehaviourComponent::UACFCombatBehaviourComponent()
{
}

bool UACFCombatBehaviourComponent::TryExecuteActionByCombatState(EAICombatState combatState)
{
    if (!CombatBehaviour) {
        return false;
    }
    if (CheckEquipment()) {
        TryEquipWeapon();
        return false;
    }

    FActionsChances* actions = CombatBehaviour->ActionByCombatState.Find(combatState);
    if (actions) {
        TArray<float> weights;
        TArray<FActionChances> executableActions;
        for (const auto& elem : actions->PossibleActions) {
            if (elem.ActionTag == FGameplayTag() || UACFFunctionLibrary::ShouldExecuteAction(elem, characterOwner)) {
                executableActions.Add(elem);
                weights.Add(elem.Weight);
            }
        }
        const int32 index = UACFFunctionLibrary::ExtractIndexWithProbability(weights);
        if (executableActions.IsValidIndex(index)) {
            const auto& elem = executableActions[index];
            aiController->SetWaitDurationTimeBK(elem.BTWaitTime);
            if (elem.bRequiresTicket) {
                return EvaluateTicket(elem);

            } else {
                characterOwner->TriggerAction(elem.ActionTag, elem.Priority);
                return true;
            }
        }
    }
    return false;
}

bool UACFCombatBehaviourComponent::EvaluateTicket(const FActionChances& elem)
{
	// Verify if we have a ticket or if we can request one
	TObjectPtr<AGameStateBase> gameState = UGameplayStatics::GetGameState(this);
	if (!gameState) {
		return false;
	}

	TObjectPtr<UACFAIManagerComponent> aiManager = gameState->FindComponentByClass<UACFAIManagerComponent>();
	if (!aiManager) {
		UE_LOG(ACFAILog, Error, TEXT("No AI Manager found! - UACFCombatBehaviourComponent::EvaluateTicket"));
		return false;
	}
	if (aiManager->HasTicket(aiController) || aiManager->RequestTicket(UATSTargetingFunctionLibrary::GetTargetedActor(aiController->GetPawn()), aiController, elem.TicketDuration)) {
		characterOwner->TriggerAction(elem.ActionTag, elem.Priority);
		return true;
	}

	return false;
}



bool UACFCombatBehaviourComponent::TryGetBestConditionalAction(FActionChances& outAction)  {

	if (!CombatBehaviour) {
		UE_LOG(ACFAILog, Error, TEXT("No Combat Behavior found! - UACFCombatBehaviourComponent"));
		return false;
	}

	TArray<float> weights;
	TArray<FActionChances> executableActions;
	for (const FConditions& actionCond : CombatBehaviour->ActionByCondition) {
		const FActionChances action = actionCond;
		if (actionCond.ActionTag == FGameplayTag() || (VerifyCondition(actionCond) && UACFFunctionLibrary::ShouldExecuteAction(action, characterOwner))) {
			executableActions.Add(actionCond);
			weights.Add(actionCond.Weight);
		}
	}
	const int32 index = UACFFunctionLibrary::ExtractIndexWithProbability(weights);
	if (executableActions.IsValidIndex(index)) {
		outAction = executableActions[index];
		return true;
	}
	return false;
}

bool UACFCombatBehaviourComponent::TryExecuteConditionAction()
{
	if (CheckEquipment()) {
		TryEquipWeapon();
		return false;
	}

	FActionChances elem;
	if (TryGetBestConditionalAction(elem)) {
		
		aiController->SetWaitDurationTimeBK(elem.BTWaitTime);
		if (elem.bRequiresTicket) {
			return EvaluateTicket(elem);
		}
		else {
			characterOwner->TriggerAction(elem.ActionTag, elem.Priority);
			return true;
		}
	}
	return false;
}

bool UACFCombatBehaviourComponent::VerifyCondition(const FConditions& condition)
{
	return condition.Condition && condition.Condition->IsConditionMet(characterOwner);
}

bool UACFCombatBehaviourComponent::IsTargetInMeleeRange(AActor* target)
{
	if (!CombatBehaviour) {
		UE_LOG(ACFAILog, Error, TEXT("No Combat Behavior found! - UACFCombatBehaviourComponent"));
		return false;
	}

	const FAICombatStateConfig* meleeDist = CombatBehaviour->CombatStatesConfig.FindByKey(EAICombatState::EMeleeCombat);

	const ACharacter* targetChar = Cast<ACharacter>(target);
	const float meleeDistance = GetIdealDistanceByCombatState(EAICombatState::EMeleeCombat);
	if (meleeDist) {
		if (targetChar) {
			const float dist = UACFFunctionLibrary::CalculateDistanceBetweenCharactersExtents(characterOwner, targetChar);
			return meleeDistance >= dist;
		}
		else if (target) {
			return characterOwner->GetDistanceTo(target) <= meleeDistance;
		}
	}
	return false;
}

EAICombatState UACFCombatBehaviourComponent::GetBestCombatStateByTargetDistance(float targetDistance)
{
	if (!CombatBehaviour) {
		UE_LOG(ACFAILog, Error, TEXT("No Combat Behavior found! - UACFCombatBehaviourComponent"));
		return EAICombatState::EMeleeCombat;
	}
	for (const FAICombatStateConfig& state : CombatBehaviour->CombatStatesConfig) {
		if (EvaluateCombatState(state.CombatState)) {
			return state.CombatState;
		}
	}

	return CombatBehaviour->DefaultCombatState;
}

float UACFCombatBehaviourComponent::GetIdealDistanceByCombatState(EAICombatState combatState) const
{
	const FAICombatStateConfig* aiState = CombatBehaviour->CombatStatesConfig.FindByKey(combatState);
	if (aiState) {
		const UACFDistanceActionCondition* distanceCond = aiState->GetDistanceBasedCondition();
		if (distanceCond) {
			return distanceCond->GetDistance();
		}
	}

	UE_LOG(ACFAILog, Warning, TEXT("Unallowed Combat State! - UACFCombatBehaviourComponent::GetIdealDistanceByCombatState"));
	return -1.f;
}

void UACFCombatBehaviourComponent::InitBehavior(class AACFAIController* controller)
{
	if (controller) {
		aiController = controller;

		characterOwner = Cast<AACFCharacter>(controller->GetPawn());

		if (!characterOwner) {
			UE_LOG(ACFAILog, Error, TEXT("No ACFCharacter for combat behaviour! - UACFCombatBehaviourComponent::InitBehavior"));
			return;
		}
		if (characterOwner->GetACFCharacterMovementComponent()) {
			characterOwner->GetACFCharacterMovementComponent()->ResetStrafeMovement();
		}
		characterOwner->TriggerAction(EngagingAction, EActionPriority::EHigh);
		if (CheckEquipment()) {
			TryEquipWeapon();
		}
	}
}

void UACFCombatBehaviourComponent::TryEquipWeapon()
{
	const UACFEquipmentComponent* equipComp = characterOwner->GetEquipmentComponent();

	ensure(equipComp);

	if (!CombatBehaviour) {
		UE_LOG(ACFAILog, Error, TEXT("No Combat Behavior found! - UACFCombatBehaviourComponent"));
		return;
	}

	if (CombatBehaviour->DefaultCombatBehaviorType == ECombatBehaviorType::EMelee) {
		characterOwner->TriggerAction(EquipMeleeAction, EActionPriority::EMedium);
		aiController->SetCombatStateBK(EAICombatState::EMeleeCombat);
	}
	else if (CombatBehaviour->DefaultCombatBehaviorType == ECombatBehaviorType::ERanged) {
		characterOwner->TriggerAction(EquipRangedAction, EActionPriority::EMedium);
		aiController->SetCombatStateBK(EAICombatState::ERangedCombat);
	}
}

void UACFCombatBehaviourComponent::UninitBehavior()
{
	if (!CombatBehaviour) {
		UE_LOG(ACFAILog, Error, TEXT("No Combat Behavior found! - UACFCombatBehaviourComponent"));
		return;
	}
	if (CombatBehaviour->bNeedsWeapon && characterOwner && CombatBehaviour && characterOwner->GetCombatType() != ECombatType::EUnarmed) {
		const FGameplayTag unequipAction = CombatBehaviour->DefaultCombatBehaviorType == ECombatBehaviorType::EMelee ? EquipMeleeAction : EquipRangedAction;
		characterOwner->TriggerAction(unequipAction, EActionPriority::EHigh);
	}
}

bool UACFCombatBehaviourComponent::CheckEquipment()
{
	if (!characterOwner || !CombatBehaviour) {
		return false;
	}
	return (characterOwner->GetCombatType() != ECombatType::EMelee && CombatBehaviour->bNeedsWeapon && CombatBehaviour->DefaultCombatBehaviorType == ECombatBehaviorType::EMelee) ||
		(characterOwner->GetCombatType() != ECombatType::ERanged && CombatBehaviour->bNeedsWeapon && CombatBehaviour->DefaultCombatBehaviorType == ECombatBehaviorType::ERanged);
}

void UACFCombatBehaviourComponent::UpdateCombatLocomotion(EAICombatState combatState)
{
	if (!CombatBehaviour) {
		return;
	}
	const FAICombatStateConfig* locstate = CombatBehaviour->CombatStatesConfig.FindByKey(combatState);
	if (locstate && characterOwner->GetACFCharacterMovementComponent()) {
		characterOwner->GetACFCharacterMovementComponent()->SetLocomotionState(locstate->LocomotionState);
	}
}

bool UACFCombatBehaviourComponent::EvaluateCombatState(EAICombatState combatState)
{
	if (!CombatBehaviour) {
		return false;
	}
	if (!CombatBehaviour->CombatStatesConfig.Contains(combatState)) {
		return false;
	}

	const FAICombatStateConfig* chance = CombatBehaviour->CombatStatesConfig.FindByKey(combatState);

	if (chance) {
		for (auto condition : chance->Conditions) {
			if (!condition) {
				UE_LOG(ACFAILog, Error, TEXT("INVALID ACTION CONDITION IN COMBAT CONFIG! - UACFCombatBehaviourComponent"));
				continue;
			}
			if (condition && !condition->IsConditionMet(characterOwner)) {
				return false;
			}
		}
		return FMath::RandRange(0.f, 100.f) <= chance->TriggerChancePercentage;
	}
	return false;
}

// void UACFCombatBehaviourComponent::UpdateBehaviorType()
// {
// 	if (IdealDistanceByCombatState.Contains(ECombatBehaviorType::ERanged))
// 	{
// 		float* randegdist = IdealDistanceByCombatState.Find(ECombatBehaviorType::ERanged);
// 		if (randegdist &&  aiController->GetTargetActorDistanceBK() > *randegdist)
// 		{
// 			CurrentCombatBehaviorType = ECombatBehaviorType::ERanged;
// 			return;
// 		}
// 	}
// 	CurrentCombatBehaviorType = ECombatBehaviorType::EMelee;
// }
