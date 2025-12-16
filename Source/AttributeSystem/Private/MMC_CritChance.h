#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_CritChance.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_CritChance : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_CritChance();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition StrengthCapture;
	FGameplayEffectAttributeCaptureDefinition InstinctCapture;
};
