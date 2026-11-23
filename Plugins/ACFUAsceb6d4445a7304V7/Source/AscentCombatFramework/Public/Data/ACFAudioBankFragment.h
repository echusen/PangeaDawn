// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/ACFCharacterFragment.h"

#include "ACFAudioBankFragment.generated.h"

class UACFAudioBankDataAsset;

/**
 * Applies an Audio Bank DataAsset to the pawn's Effects Manager at initialization.
 * Add it to UACFCharacterDataAsset::Fragments so ability sounds are assigned from data.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, DisplayName = "ACF Audio Bank")
class ASCENTCOMBATFRAMEWORK_API UACFAudioBankFragment : public UACFCharacterFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
	TObjectPtr<UACFAudioBankDataAsset> AudioBank;

	virtual void ApplyFragment_Implementation(APawn* pawnOwner) override;
};
