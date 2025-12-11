#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Bravery.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Bravery : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Bravery();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition EnduranceCapture;
	FGameplayEffectAttributeCaptureDefinition InstinctCapture;
};
