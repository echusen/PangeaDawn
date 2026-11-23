// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Cue/ACFHitEffectGameplayCue.h"
#include "ACMCollisionsFunctionLibrary.h"
#include "ACMTypes.h"
#include "Logging.h"
#include <Engine/HitResult.h>
#include "Components/ACFEffectsManagerComponent.h"
#include <GameFramework/Character.h>
#include <GameFramework/DamageType.h>

bool UACFHitEffectGameplayCue::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	if (!Parameters.EffectContext.IsValid()) {
		UE_LOG(ACFLog, Error,
			TEXT("UACFHitEffectGameplayCue: invalid EffectContext. Target=%s Instigator=%s"),
			*GetNameSafe(Target),
			*GetNameSafe(Parameters.GetInstigator()));
		return false;
	}

	const FHitResult* HitResult = Parameters.EffectContext.GetHitResult();
	if (!HitResult) {
		UE_LOG(ACFLog, Error, TEXT("UACFHitEffectGameplayCue: missing HitResult on EffectContext. Target=%s"), *GetNameSafe(Target));
		return false;
	}

	ACharacter* InstigatorChar = Cast<ACharacter>(Parameters.GetInstigator());
	if (!InstigatorChar) {
		InstigatorChar = Cast<ACharacter>(Target);
	}
	if (!InstigatorChar) {
		UE_LOG(ACFLog, Error, TEXT("UACFHitEffectGameplayCue: no Character instigator/target. Target=%s"), *GetNameSafe(Target));
		return false;
	}

	UACFEffectsManagerComponent* EffComp = InstigatorChar->FindComponentByClass<UACFEffectsManagerComponent>();
	if (!EffComp) {
		UE_LOG(ACFLog, Error, TEXT("UACFHitEffectGameplayCue: missing UACFEffectsManagerComponent on %s"), *GetNameSafe(InstigatorChar));
		return false;
	}

	FGameplayTag HitReactionTag;
	TArray<FGameplayTag> SourceTags;
	Parameters.AggregatedSourceTags.GetGameplayTagArray(SourceTags);
	if (SourceTags.Num() > 0) {
		HitReactionTag = SourceTags[0];
	}

	TSubclassOf<UDamageType> DamageTypeClass = nullptr;
	if (const UClass* SourceClass = Cast<const UClass>(Parameters.SourceObject.Get())) {
		if (SourceClass->IsChildOf(UDamageType::StaticClass())) {
			DamageTypeClass = const_cast<UClass*>(SourceClass);
		}
	}

	FImpactFX OutFX;
	if (!EffComp->TryGetDamageFX(HitReactionTag, DamageTypeClass, OutFX)) {
		UE_LOG(ACFLog, Warning,
			TEXT("UACFHitEffectGameplayCue: no DamageFX for HitReaction=%s DamageType=%s on %s"),
			*HitReactionTag.ToString(),
			*GetNameSafe(DamageTypeClass.Get()),
			*GetNameSafe(InstigatorChar));
		return false;
	}

	FActionEffect Effect(OutFX, ESpawnFXLocation::ESpawnAttachedToSocketOrBone, HitResult->BoneName);
	UACMCollisionsFunctionLibrary::SpawnSoundAndParticleAttached(Effect, InstigatorChar);
	return true;
}

UACFHitEffectGameplayCue::UACFHitEffectGameplayCue()
{
}
