#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Breeding.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Breeding : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Breeding();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition InstinctCapture;
};
