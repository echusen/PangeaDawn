// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ACFCoreTypes.h"
#include "AIController.h"
#include "Components/ACFTeamComponent.h"
#include "CoreMinimal.h"
#include "Game/ACFDamageType.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/ACFEntityInterface.h"

#include "ACFBaseAIController.generated.h"

class UACFTeamComponent;
class UACFDamageHandlerComponent;

/**
 * AI Controller base class used for all ACF AI characters.
 * Manages behavior logic and assigns the combat team for AI-controlled pawns.
 */
UCLASS()
class AIFRAMEWORK_API AACFBaseAIController : public AAIController, public IACFEntityInterface {
    GENERATED_BODY()

public:
    AACFBaseAIController(const FObjectInitializer& ObjectInitializer);
  

    /// <summary>
    /// /Combat Team Interface
    /// </summary>

    // IACFEntityInterface - Main implementation (source of truth)
    virtual FGameplayTag GetEntityCombatTeam_Implementation() const override;
    virtual void AssignTeamToEntity_Implementation(FGameplayTag inCombatTeam) override;

    // IGenericTeamAgentInterface - Read from TeamComponent
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(const FGenericTeamId& TeamID) override;
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& OtherTeam) const override;

    /// <summary>
    /// /End Combat Team Entity
    /// </summary>

protected:
    virtual void OnPossess(APawn* _possPawn) override;

    virtual void OnUnPossess() override;

    /*Called when the possessed pawn receives damage, bound to the pawn's DamageHandlerComponent*/
    UFUNCTION()
    virtual void HandlePawnDamaged(const FACFDamageEvent& damageReceived);

    /*Called when the possessed pawn dies, bound to the pawn's DamageHandlerComponent*/
    UFUNCTION()
    virtual void HandlePawnDeath();

    IACFEntityInterface* PossessedEntity;

private:
    /*Damage handler of the possessed pawn, cached to unbind on unpossess*/
    UPROPERTY(Transient)
    TObjectPtr<UACFDamageHandlerComponent> PossessedDamageHandler;



private:
};
