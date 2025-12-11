#include "MMC_Taunting.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Taunting::UMMC_Taunting()
{
	const FStructProperty* IntelligenceStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Intelligence));

	const FStructProperty* PersuasionStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Persuasion));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(IntelligenceStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(PersuasionStruct));

	
	IntelligenceCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	PersuasionCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(IntelligenceCapture);
	RelevantAttributesToCapture.Add(PersuasionCapture);
}

float UMMC_Taunting::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Intelligence = 0.f;
	float Persuasion = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(IntelligenceCapture, Spec,  Params, Intelligence);
	GetCapturedAttributeMagnitude(PersuasionCapture, Spec, Params, Persuasion);
	
	return (Intelligence * 0.5f) + (Persuasion * 0.5f);
}

