#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Charm.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Charm : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Charm();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition PersuasionCapture;
	FGameplayEffectAttributeCaptureDefinition InstinctCapture;
};
