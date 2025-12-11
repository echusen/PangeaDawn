#include "MMC_Weight.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Weight::UMMC_Weight()
{
	const FStructProperty* StrengthStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Strength));

	const FStructProperty* EnduranceStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Agility));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(StrengthStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(EnduranceStruct));

	
	StrengthCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	EnduranceCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(StrengthCapture);
	RelevantAttributesToCapture.Add(EnduranceCapture);
}

float UMMC_Weight::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Strength = 0.f;
	float Endurance = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(StrengthCapture, Spec,  Params, Strength);
	GetCapturedAttributeMagnitude(EnduranceCapture, Spec, Params, Endurance);
	
	return (Strength * 0.6f) + (Endurance * 0.4f);
}

