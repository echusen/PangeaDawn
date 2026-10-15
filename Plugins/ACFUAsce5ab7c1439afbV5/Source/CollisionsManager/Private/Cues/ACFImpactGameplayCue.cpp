// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Cues/ACFImpactGameplayCue.h"
#include "ACMCollisionsFunctionLibrary.h"
#include "ACMEffectsDispatcherComponent.h"
#include "ACMTypes.h"
#include "Logging.h"
#include <Engine/HitResult.h>
#include <GameFramework/DamageType.h>

UACFImpactGameplayCue::UACFImpactGameplayCue()
{
}

bool UACFImpactGameplayCue::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	if (!Parameters.EffectContext.IsValid()) {
		UE_LOG(ACFCollisionsLog, Error,
			TEXT("UACFImpactGameplayCue: invalid EffectContext. Target=%s Instigator=%s"),
			*GetNameSafe(Target),
			*GetNameSafe(Parameters.GetInstigator()));
		return false;
	}

	const FHitResult* HitResult = Parameters.EffectContext.GetHitResult();
	if (!HitResult) {
		UE_LOG(ACFCollisionsLog, Error, TEXT("UACFImpactGameplayCue: missing HitResult on EffectContext. Target=%s"), *GetNameSafe(Target));
		return false;
	}

	UACMEffectsDispatcherComponent* EffComp = UACMCollisionsFunctionLibrary::GetEffectDispatcher(Target);
	if (!EffComp || !Parameters.GetInstigator()) {
		return false;
	}

	TSubclassOf<UDamageType> DamageTypeClass = nullptr;
	if (const UClass* SourceClass = Cast<const UClass>(Parameters.SourceObject.Get())) {
		if (SourceClass->IsChildOf(UDamageType::StaticClass())) {
			DamageTypeClass = const_cast<UClass*>(SourceClass);
		}
	}

	UPhysicalMaterial* PhysMat = HitResult->PhysMaterial.Get();
	FBaseFX OutFX;
	if (!EffComp->TryGetImpactFX(DamageTypeClass, PhysMat, OutFX)) {
		return false;
	}

	FImpactFX Effect = FImpactFX(OutFX, HitResult->ImpactPoint);
	FComponentFX OutComps;
	UACMCollisionsFunctionLibrary::SpawnSoundAndParticleAtLocation(Target, Effect, OutComps);
	return true;
}
