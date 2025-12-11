#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Precision.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Precision : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Precision();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition AgilityCapture;
};
