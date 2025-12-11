#include "MMC_Bravery.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Bravery::UMMC_Bravery()
{
	const FStructProperty* EnduranceStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Endurance));

	const FStructProperty* InstinctStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Instinct));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(EnduranceStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(InstinctStruct));

	
	EnduranceCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	InstinctCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(EnduranceCapture);
	RelevantAttributesToCapture.Add(InstinctCapture);
}

float UMMC_Bravery::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Endurance = 0.f;
	float Instinct = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(EnduranceCapture, Spec,  Params, Endurance);
	GetCapturedAttributeMagnitude(InstinctCapture, Spec, Params, Instinct);
	
	return (Endurance * 0.2f) + (Instinct * 0.8f);
}

