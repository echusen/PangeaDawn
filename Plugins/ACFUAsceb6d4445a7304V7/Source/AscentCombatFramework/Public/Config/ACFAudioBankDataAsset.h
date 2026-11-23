// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Sound/SoundCue.h"

#include "ACFAudioBankDataAsset.generated.h"

/**
 * Maps ACF ability tags to SoundCues played locally from a single attach bone.
 */
UCLASS(BlueprintType)
class ASCENTCOMBATFRAMEWORK_API UACFAudioBankDataAsset : public UPrimaryDataAsset {
	GENERATED_BODY()

protected:
	/** Ability tag → SoundCue. Looked up when an ACF ability starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF, meta = (Categories = "Actions,Ability"))
	TMap<FGameplayTag, TObjectPtr<USoundCue>> AbilitySounds;

	/** Mesh socket/bone the sounds attach to. Shared by every entry in this bank. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ACF)
	FName SoundBoneName = TEXT("head");

public:
	UFUNCTION(BlueprintPure, Category = ACF)
	FName GetSoundBoneName() const { return SoundBoneName; }

	/** Returns the SoundCue mapped to AbilityTag, if any. */
	UFUNCTION(BlueprintPure, Category = ACF)
	bool TryGetAbilitySound(FGameplayTag AbilityTag, USoundCue*& OutSound) const
	{
		if (const TObjectPtr<USoundCue>* Found = AbilitySounds.Find(AbilityTag)) {
			OutSound = Found->Get();
			return OutSound != nullptr;
		}
		OutSound = nullptr;
		return false;
	}
};
