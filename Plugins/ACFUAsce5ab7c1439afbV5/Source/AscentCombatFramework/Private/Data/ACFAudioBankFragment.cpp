// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Data/ACFAudioBankFragment.h"
#include "Components/ACFEffectsManagerComponent.h"
#include "Config/ACFAudioBankDataAsset.h"
#include "GameFramework/Pawn.h"

void UACFAudioBankFragment::ApplyFragment_Implementation(APawn* pawnOwner)
{
	if (!pawnOwner || !AudioBank)
	{
		return;
	}

	UACFEffectsManagerComponent* EffectsComp = pawnOwner->FindComponentByClass<UACFEffectsManagerComponent>();
	if (!EffectsComp)
	{
		return;
	}

	EffectsComp->SetAudioBank(AudioBank);
}
