#include "MMC_Crafting.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Crafting::UMMC_Crafting()
{
	const FStructProperty* IntelligenceStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Intelligence));

	const FStructProperty* InstinctStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Instinct));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(IntelligenceStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(InstinctStruct));

	
	IntelligenceCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	InstinctCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(IntelligenceCapture);
	RelevantAttributesToCapture.Add(InstinctCapture);
}

float UMMC_Crafting::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Intelligence = 0.f;
	float Instinct = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(IntelligenceCapture, Spec,  Params, Intelligence);
	GetCapturedAttributeMagnitude(InstinctCapture, Spec, Params, Instinct);
	
	return (Intelligence * 0.8f) + (Instinct * 0.2f);
}

