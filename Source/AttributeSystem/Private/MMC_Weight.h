#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Weight.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Weight : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Weight();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition StrengthCapture;
	FGameplayEffectAttributeCaptureDefinition EnduranceCapture;
};
