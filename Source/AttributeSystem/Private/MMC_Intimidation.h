#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Intimidation.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Intimidation : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Intimidation();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition PersuasionCapture;
};
