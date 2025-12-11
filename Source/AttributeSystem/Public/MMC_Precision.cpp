#include "MMC_Precision.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Precision::UMMC_Precision()
{
	const FStructProperty* StrengthStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Strength));

	const FStructProperty* AgilityStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Agility));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(StrengthStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(AgilityStruct));

	
	IntelligenceCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	AgilityCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(IntelligenceCapture);
	RelevantAttributesToCapture.Add(AgilityCapture);
}

float UMMC_Precision::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Intelligence = 0.f;
	float Agility = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(IntelligenceCapture, Spec,  Params, Intelligence);
	GetCapturedAttributeMagnitude(AgilityCapture, Spec, Params, Agility);
	
	return (Intelligence * 0.2f) + (Agility * 0.8f);
}

