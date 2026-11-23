// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ACFProceduralMotionComponent.h"
#include "ACFProceduralOffsetMotionComponent.generated.h"

/** Static resting offset for the equipment, authored separately for first and third person. */
UCLASS(ClassGroup=(ACFFirstPerson), meta=(BlueprintSpawnableComponent, DisplayName="Offset Component"))
class ASCENTFIRSTPERSON_API UACFProceduralOffsetMotionComponent : public UACFProceduralMotionComponent
{
	GENERATED_BODY()

public:
	UACFProceduralOffsetMotionComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UACFProceduralMotionComponent
	virtual TSubclassOf<UACFProceduralPreset> GetPresetClass() const override;
	//~ End of UACFProceduralMotionComponent

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Offset", meta=(ClampMin="0.0", ClampMax="1.0"))
	float OffsetAlpha = 1.f;

protected:
	//~ Begin UACFProceduralMotionComponent
	virtual void ComputeOutputs(float DeltaSeconds, const UAnimInstance* AnimInstance, const FACFProceduralMotionInput& Input, TArray<FACFProceduralMotionOutput>& Outs) override;
	//~ End UACFProceduralMotionComponent
};
