#include "MMC_MeleeAttack.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_MeleeAttack::UMMC_MeleeAttack()
{
	const FStructProperty* StrengthStruct =
	FindFProperty<FStructProperty>(UACFPrimaryAttributeSet::StaticClass(),
	GET_MEMBER_NAME_CHECKED(UACFPrimaryAttributeSet, Strength));

	const FStructProperty* AgilityStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Agility));

	
	FGameplayAttribute SAttr(const_cast<FStructProperty*>(StrengthStruct));
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(AgilityStruct));

	
	StrengthCapture = FGameplayEffectAttributeCaptureDefinition(
		SAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	AgilityCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(StrengthCapture);
	RelevantAttributesToCapture.Add(AgilityCapture);
}

float UMMC_MeleeAttack::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Strength = 0.f;
	float Agility = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(StrengthCapture, Spec,  Params, Strength);
	GetCapturedAttributeMagnitude(AgilityCapture, Spec, Params, Agility);
	
	return (Strength * 0.2f) + (Agility * 0.8f);
}

