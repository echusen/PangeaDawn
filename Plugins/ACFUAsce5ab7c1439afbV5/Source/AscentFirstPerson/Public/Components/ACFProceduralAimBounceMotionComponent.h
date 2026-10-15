// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFProceduralMotionComponent.h"
#include "ACFProceduralAimBounceMotionComponent.generated.h"

/** Kick applied as ADS starts and ends, with separate offsets per direction and per view. */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Aim Bounce Component"))
class ASCENTFIRSTPERSON_API UACFProceduralAimBounceMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralAimBounceMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
	//~ End of UACFProceduralMotionComponent

protected:
	//~ Begin UACFProceduralMotionComponent
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs) override;
	//~ End UACFProceduralMotionComponent

	/** First and third person are driven independently, so each keeps its own direction latch. */
	struct FAimBounceState
	{
		float LastAimingState = 0.f;
		bool bIsAimingIn = false;
		FRotator CurrentRotationOffset = FRotator::ZeroRotator;
		FVector CurrentTranslationOffset = FVector::ZeroVector;
	};

	FAimBounceState StateFP;
	FAimBounceState StateTP;
};
