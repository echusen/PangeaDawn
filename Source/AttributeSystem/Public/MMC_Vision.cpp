#include "MMC_Vision.h"
#include "ACFPrimaryAttributeSet.h"
#include "PangeaAttributeSet.h"
#include "GameplayEffectTypes.h"


UMMC_Vision::UMMC_Vision()
{
	const FStructProperty* InstinctStruct =
		FindFProperty<FStructProperty>(UPangeaAttributeSet::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPangeaAttributeSet, Instinct));
	
	FGameplayAttribute IAttr(const_cast<FStructProperty*>(InstinctStruct));
	

	InstinctCapture = FGameplayEffectAttributeCaptureDefinition(
		IAttr,
		EGameplayEffectAttributeCaptureSource::Source,
		false);

	RelevantAttributesToCapture.Add(InstinctCapture);
}

float UMMC_Vision::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	float Instinct = 0.f;

	FAggregatorEvaluateParameters Params;

	GetCapturedAttributeMagnitude(InstinctCapture, Spec, Params, Instinct);
	
	return Instinct;
}

