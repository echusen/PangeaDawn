#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Defense.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Defense : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Defense();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition StrengthCapture;
	FGameplayEffectAttributeCaptureDefinition AgilityCapture;
};
