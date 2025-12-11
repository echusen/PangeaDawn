#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Taunting.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Taunting : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Taunting();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition PersuasionCapture;
};
