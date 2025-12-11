#include "MMC_Footholding.h"
#include "ACFPrimaryAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Footholding::UMMC_Footholding()
{
	const FStructProperty* StrengthStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Strength));

	const FStructProperty* IntelligenceStruct =
		FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Intelligence));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(StrengthStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(IntelligenceStruct));

	
	StrengthCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	IntelligenceCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(StrengthCapture);
	RelevantAttributesToCapture.Add(IntelligenceCapture);
}

float UMMC_Footholding::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Strength = 0.f;
	float Intelligence = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(StrengthCapture, Spec,  Params, Strength);
	GetCapturedAttributeMagnitude(IntelligenceCapture, Spec, Params, Intelligence);
	
	return (Strength * 0.2f) + (Intelligence * 0.8f);
}

