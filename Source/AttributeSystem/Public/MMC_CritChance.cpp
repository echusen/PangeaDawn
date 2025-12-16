#include "MMC_CritChance.h"

#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_CritChance::UMMC_CritChance()
{
	const FStructProperty* StrengthStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Strength));

	const FStructProperty* InstinctStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Instinct));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(StrengthStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(InstinctStruct));

	
	StrengthCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	InstinctCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(StrengthCapture);
	RelevantAttributesToCapture.Add(InstinctCapture);
}

float UMMC_CritChance::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Strength = 0.f;
	float Instinct = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(StrengthCapture, Spec,  Params, Strength);
	GetCapturedAttributeMagnitude(InstinctCapture, Spec, Params, Instinct);
	
	return (Strength * 0.5f) + (Instinct * 0.5f);
}

