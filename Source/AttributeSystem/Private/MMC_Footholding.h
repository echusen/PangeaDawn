#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Footholding.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Footholding : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Footholding();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition StrengthCapture;
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
};
