#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Taming.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Taming : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Taming();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition PersuasionCapture;
	FGameplayEffectAttributeCaptureDefinition InstinctCapture;

};
