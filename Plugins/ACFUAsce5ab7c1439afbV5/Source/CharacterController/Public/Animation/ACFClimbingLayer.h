// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/ACFAnimLayer.h"
#include "ACFClimbingLayer.generated.h"

class UACFCharacterMovementComponent;
class UACFClimbingComponent;

UCLASS()
class CHARACTERCONTROLLER_API UACFClimbingLayer : public UACFAnimLayer {
	GENERATED_BODY()

public:
	UACFClimbingLayer();

protected:
	UPROPERTY(BlueprintReadOnly, Category = ACF)
	TObjectPtr<UACFCharacterMovementComponent> MovementComp;

	UPROPERTY(BlueprintReadOnly, Category = ACF)
	TObjectPtr<UACFClimbingComponent> ClimbingComp;

	UPROPERTY(BlueprintReadOnly, Category = "ACF|Climbing")
	float NormalizedClimbingSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "ACF|Climbing")
	FVector LocalClimbingVelocity;

	UPROPERTY(BlueprintReadOnly, Category = "ACF|Climbing")
	FVector ClimbingSurfaceNormal;

	virtual void NativeInitializeAnimation() override;

	/* Runs on the Game Thread. Resolves MovementComp/ClimbingComp if they aren't set yet -
	 * NativeThreadSafeUpdateAnimation runs on a parallel worker thread and must never call
	 * FindComponentByClass, since that can race with components being added/removed on the
	 * Game Thread. */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void OnActivated_Implementation() override;

private:
	void SetReferences();
};
