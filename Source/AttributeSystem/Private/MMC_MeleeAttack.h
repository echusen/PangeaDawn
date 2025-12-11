#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_MeleeAttack.generated.h"

UCLASS()
class ATTRIBUTESYSTEM_API UMMC_MeleeAttack : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_MeleeAttack();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	FGameplayEffectAttributeCaptureDefinition StrengthCapture;
	FGameplayEffectAttributeCaptureDefinition AgilityCapture;
};
