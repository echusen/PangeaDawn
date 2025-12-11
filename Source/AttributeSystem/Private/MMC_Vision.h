#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Vision.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Vision : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

	
public:
	UMMC_Vision();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	
	FGameplayEffectAttributeCaptureDefinition InstinctCapture;
};
