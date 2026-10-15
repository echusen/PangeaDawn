// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/CollisionProfile.h"
#include "ACFProceduralLeaningComponent.generated.h"

class ACharacter;

/**
 * Tracks a lean angle from input, clamped by a wall sweep.
 *
 * Not a motion component. The angle is consumed by FAnimNode_ACFProceduralAimOffset when
 * bAutoFillRotationFromLeaning is set.
 */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent))
class ASCENTFIRSTPERSON_API UACFProceduralLeaningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACFProceduralLeaningComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UActorComponent
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	UFUNCTION(BlueprintCallable, Category="Leaning")
	void InputLeanLeft(bool bPressed);

	UFUNCTION(BlueprintCallable, Category="Leaning")
	void InputLeanRight(bool bPressed);

	UFUNCTION(BlueprintPure, Category="Leaning")
	float GetCurrentLeanAngle() const { return CurrentLeanAngle; }

	UFUNCTION(BlueprintPure, Category="Leaning")
	ACharacter* GetOwnerCharacter() const;

protected:
	/** Last press wins; releasing falls back to whichever side is still held. */
	void ResolveLeanState(bool bIsLeftInput, bool bPressed);

	UFUNCTION(Server, Reliable)
	void Server_SetLeanLeft(bool bPressed);

	UFUNCTION(Server, Reliable)
	void Server_SetLeanRight(bool bPressed);

	/** Scales the desired angle down by how far the head can travel before hitting geometry. */
	float GetWallClampedTargetAngle(float DesiredTargetAngle) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Leaning")
	float MaxLeanAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Leaning")
	float LeanInterpSpeed;

	/** Sideways head travel in cm at MaxLeanAngle. Drives the wall sweep length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Leaning|Collision")
	float MaxLeanDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Leaning|Collision")
	TEnumAsByte<enum ECollisionChannel> TraceChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Leaning|Collision")
	FName HeadBoneName = FName("head");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Leaning|Collision")
	float CameraTraceRadius = 15.0f;

protected:
	/** -1 left, 0 centre, 1 right. */
	UPROPERTY(Replicated)
	int8 TargetLeanDirection;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State|Leaning")
	float CurrentLeanAngle;

	bool bIsHoldingLeft;
	bool bIsHoldingRight;
};
