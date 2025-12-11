#pragma once
 
#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_Stealth.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_Stealth : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public: 
	UMMC_Stealth();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition IntelligenceCapture;
	FGameplayEffectAttributeCaptureDefinition AgilityCapture;
};
