// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Actions/ACFActionAbility.h"
#include "CoreMinimal.h"
#include "Engine/HitResult.h"

#include "ACFDirectionalDodgeAction.generated.h"

/**
 *
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API UACFDirectionalDodgeAction : public UACFActionAbility {
	GENERATED_BODY()

protected:
	UACFDirectionalDodgeAction();

	virtual void OnActionStarted_Implementation() override;

	virtual void OnActionEnded_Implementation() override;

	virtual FName GetMontageSectionName_Implementation() override;

	virtual FTransform GetWarpTransform_Implementation() override;

	virtual void GetWarpInfo_Implementation(FACFWarpReproductionInfo& outWarpInfo) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION(BlueprintPure, Category = ACF)
	EACFDirection GetDodgeDirection() const
	{
		return finalDirection;
	}

	/*The montage section name to select the animation be played depending on the dodge direction*/
	UPROPERTY(EditDefaultsOnly, Category = ACF)
	TMap<EACFDirection, FName> DodgeDirectionToMontageSectionMap;

	/*The length of the dodge in centimeters*/
	UPROPERTY(EditDefaultsOnly, Category = "ACF")
	float DodgeLength = 600.f;

	/*The direction of the dodge when no directional input is found*/
	UPROPERTY(EditDefaultsOnly, Category = "ACF")
	EACFDirection defaultDodgeDirection;

	/*If true, ramming into an actor with a UACFDestructableComponent while dodging destroys it*/
	UPROPERTY(EditDefaultsOnly, Category = "ACF")
	bool bDestroyDestructablesOnContact = true;

	UFUNCTION()
	void HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleOwnerBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	/*If the dodge shuould be constrained around current target (useful for lock on dodges)
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = bShouldWarp), Category = "ACF| Warp")
	bool bConstraintRotationAroundTarged;*/

private:
	FVector dodgeDirection;

	EACFDirection finalDirection;

	TObjectPtr<AActor> TryGetCurrentTarget();

	void TryDestroyDestructable(AActor* OtherActor, const FHitResult& Hit);

public:

	UFUNCTION(BlueprintPure, Category = ACF)
	float GetDodgeLength() const { return DodgeLength; }

	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetDodgeLength(float val) { DodgeLength = val; }

	/* True if this location can be used as a dodge warp target. World origin (0,0,0) is never valid. */
	UFUNCTION(BlueprintPure, Category = ACF, meta = (DisplayName = "Is Valid Dodge Warp Location", Keywords = "dodge warp origin"))
	bool IsValidDodgeWarpLocation(const FVector& WarpLocation) const;

	/* Fallback dodge destination when a warp point is missing or is world origin. Never returns 0,0,0 while the owner exists. */
	UFUNCTION(BlueprintPure, Category = ACF, meta = (DisplayName = "Get Safe Dodge Warp Location", Keywords = "dodge warp origin fallback"))
	FVector GetSafeDodgeWarpLocation() const;

};
