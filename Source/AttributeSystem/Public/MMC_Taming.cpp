#include "MMC_Taming.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Taming::UMMC_Taming()
{
	const FStructProperty* IntelligenceStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Intelligence));

	const FStructProperty* PersuasionStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Persuasion));

	const FStructProperty* InstinctStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Instinct));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(IntelligenceStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(PersuasionStruct));
	FGameplayAttribute Inttr(const_cast<FStructProperty*>(InstinctStruct));


	
	IntelligenceCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	PersuasionCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	InstinctCapture = FGameplayEffectAttributeCaptureDefinition(
		Inttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(IntelligenceCapture);
	RelevantAttributesToCapture.Add(PersuasionCapture);
	RelevantAttributesToCapture.Add(InstinctCapture);
}

float UMMC_Taming::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Intelligence = 0.f;
	float Persuasion = 0.f;
	float Instinct = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(IntelligenceCapture, Spec,  Params, Intelligence);
	GetCapturedAttributeMagnitude(PersuasionCapture, Spec, Params, Persuasion);
	GetCapturedAttributeMagnitude(InstinctCapture, Spec, Params, Instinct);

	
	return (Intelligence * 0.33f) + (Persuasion * 0.33f) + (Instinct * 0.33f);
}

