#include "MMC_Stealth.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Stealth::UMMC_Stealth()
{
	const FStructProperty* IntelligenceStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Intelligence));

	const FStructProperty* AgilityStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Agility));

	

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(IntelligenceStruct));
	FGameplayAttribute Attr(const_cast<FStructProperty*>(AgilityStruct));
	

	
	IntelligenceCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	AgilityCapture = FGameplayEffectAttributeCaptureDefinition(
		Attr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	
	RelevantAttributesToCapture.Add(IntelligenceCapture);
	RelevantAttributesToCapture.Add(AgilityCapture);
	

}

float UMMC_Stealth::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Intelligence = 0.f;
	float Agility = 0.f;
	

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(IntelligenceCapture, Spec,  Params, Intelligence);
	GetCapturedAttributeMagnitude(AgilityCapture, Spec, Params, Agility);

	
	
	return (Intelligence * 0.5f) + (Agility * 0.5f);
}

