// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Actions/ACFAttackAction.h"
#include "Actions/ACFActionAbility.h"
#include "CoreMinimal.h"

#include "ACFComboAttackAction.generated.h"

class UACFComboGraph;
class UACFComboNode;
class UACFComboComponent;

/**
 *
 */
UCLASS()
class ASCENTCOMBOGRAPH_API UACFComboAttackAction : public UACFAttackAction {
	GENERATED_BODY()

public:
	UACFComboAttackAction();

protected:
	// [Multi-input combo fix] gate + cooldown-bypass for the combo's own authorized continuation
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void OnActionStarted_Implementation() override;

	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnActionEnded_Implementation() override;

	// [Combo fix] an interrupted node montage (e.g. hit reaction) marks the step as cancelled
	virtual void HandleMontageInterrupted() override;

	void ProcessComboStepEnd();

	/*The combo graph that needs to be executed*/
	UPROPERTY(EditAnywhere, Category = ACF)
	UACFComboGraph* Combo;

	/*if we need to consider the Input Buffer opened for the whole animation.
	If this is false, you'd need to add an OpenBufferNotify within the animation*/
	UPROPERTY(EditAnywhere, Category = ACF)
	bool bAutoOpenBuffer;


	TObjectPtr<UACFComboNode> node;
	TObjectPtr<UACFComboComponent> comboComponent;

	FActiveGameplayEffectHandle geHandle;

	bool bComboStepEndProcessed = false;

	// [Combo fix] true when this node ended cancelled/interrupted: the combo must stop, not continue
	bool bComboWasCancelled = false;
};
