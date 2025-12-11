#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Crafting.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Crafting : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_Crafting();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition InstinctCapture;
};
