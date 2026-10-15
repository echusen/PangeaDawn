// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ATSBaseTargetComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewTarget, AActor*, newTarget);


class AActor;

UCLASS(ClassGroup = (ATS), Blueprintable, meta = (BlueprintSpawnableComponent))
class ASCENTTARGETINGSYSTEM_API UATSBaseTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UATSBaseTargetComponent();

	/*Forces to target the selected actor*/
	UFUNCTION(BlueprintCallable, Category = "ATS")
	virtual	void SetCurrentTarget(class AActor* target) {
		SetTarget(target);
	}

	UFUNCTION(Server, Reliable, WithValidation, Category = "ATS")
	void SetTarget(class AActor* target);

	UFUNCTION(Server, Reliable, WithValidation, Category = "ATS")
	void SetTargetPoint(class UATSTargetPointComponent* targetPoint);

	/*Return the currently focused target*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ATS")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	/*Returns if there is a valid target*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ATS")
	bool HasTarget() const { return CurrentTarget != nullptr; }

	/*Return the TargetPointComponent focused of the Current target*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ATS")
	FORCEINLINE	class UATSTargetPointComponent* GetCurrentTargetPoint() const { return CurrentTargetPoint; }

	/*Returns true if the target is in sight with the actual pawn owned by the controller*/
	UFUNCTION(BlueprintPure, Category = ATS)
	bool IsTargetInSight() const;

	/*Returns true if the potential target is in sight with the actual pawn owned by the controller*/
	UFUNCTION(BlueprintPure, Category = ATS)
	bool IsPotentialTargetInSight(class AActor* potentialTarget) const;

	/*Get the world location of current target point*/
	UFUNCTION(BlueprintCallable, Category = ATS)
	FVector GetCurrentTargetPointLocation() const;

	/**
	 * World location a look-at / aim should point at this frame, with the optional eye-height
	 * correction already applied. Game Thread only: it reads the target actor's transform.
	 * @param OutLocation World location of the look-at point.
	 * @return False when there is no target, so callers keep their own fallback.
	 */
	UFUNCTION(BlueprintPure, Category = ATS, meta = (DisplayName = "Get Look At Location", Keywords = "look at target aim lock on head"))
	bool GetLookAtLocation(FVector& OutLocation) const;

	/**
	 * Finds the targeting component that owns this actor's current target: on the possessing
	 * controller first, then on the actor itself. Lives on the Game Thread.
	 */
	UFUNCTION(BlueprintPure, Category = ATS, meta = (DisplayName = "Find Target Component For Actor", Keywords = "look at targeting lock on"))
	static UATSBaseTargetComponent* FindForActor(const AActor* Actor);

	/*Aim level at the opponent instead of at the height its target point sits at. A target point
	is placed to frame the camera, usually on the chest, and aiming a ray from the eyes down to it
	makes the character stare at the ground, more and more the closer the opponent gets. With this
	on, only the point's horizontal direction is used and the pitch comes from the difference in
	height between the two characters, so an equally tall opponent is aimed at dead level.
	Turn it off to aim exactly at the target point, e.g. up at a big creature's head.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ATS)
	bool bAimAtTargetEyeHeight = true;

	/*Returns true only while a lock-on mode is engaged, which is NOT the same as having a target:
	AI combat targets and scripted SetCurrentTarget calls set a target with no lock-on at all.
	The base component has no lock-on mode, so subclasses that implement one override this.*/
	UFUNCTION(BlueprintPure, Category = ATS, meta = (DisplayName = "Is Lock On Active", Keywords = "lock on target look at"))
	virtual bool GetIsLockOnActive() const { return false; }

	UPROPERTY(BlueprintAssignable, Category = ATS)
	FOnNewTarget OnNewTarget;

protected:

	/*Checks if there is any blocking actor in the line sight to current target*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ATS")
	bool bCheckLineSight = false;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentTarget)
	class AActor* CurrentTarget;

	UPROPERTY(Replicated)
	class UATSTargetPointComponent* CurrentTargetPoint;

	UFUNCTION()
	void OnRep_CurrentTarget();

	virtual void OnTargetChanged();

private:
	FVector ResolveLookAtPoint() const;
	class APawn* GetViewerPawn() const;
};
