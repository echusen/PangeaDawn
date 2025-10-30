// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ADSDialogueSubsystem.h"
#include "ADSDialogueDeveloperSettings.h"
#include "ADSDialoguePartecipantComponent.h"
#include "ADSVoiceConfigDataAsset.h"


/**
 * Global voice cache populated by async TTS API calls.
 * Stores voice names, mappings, and last request state to avoid redundant API requests.
 */

void UADSDialogueSubsystem::RegisterParticipant(UADSDialoguePartecipantComponent* participant)
{
    if (participant) {
        Participants.Add(participant->GetParticipantTag(), participant);
    }
}

void UADSDialogueSubsystem::UnregisterParticipant(const FGameplayTag& participant)
{
    if (Participants.Contains(participant)) {
        Participants.Remove(participant);
    }
}

UADSDialoguePartecipantComponent* UADSDialogueSubsystem::FindParticipant(const FGameplayTag& participant) const
{
    if (Participants.Contains(participant)) {
        return *(Participants.Find(participant));
    }
    return nullptr;
}

FGameplayTag UADSDialogueSubsystem::GetDefaultPlayerResponseTag() const
{

    UADSDialogueDeveloperSettings* settings = GetMutableDefault<UADSDialogueDeveloperSettings>();

    if (settings) {
        return settings->GetDefaultPlayerResponseTag();
    }

    return FGameplayTag();
}

TArray<FString> UADSDialogueSubsystem::GetAvailableVoiceNames() const
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString CurrentAPIKey = Settings->GetTTSVoiceGenAPIKey();

    if (CurrentAPIKey.IsEmpty()) {
        return { TEXT("Set API key in Project Settings") };
    }

    if (!IsCacheValid(CurrentAPIKey)) {
        return { TEXT("Click 'Refresh Voices' to load from API") };
    }

    // Ritorna una copia della lista di voci dalla cache
    return TTSVoiceCache.VoiceNames;
}

bool UADSDialogueSubsystem::TryGetParticipantVoiceConfig(FADSVoiceSettings& outVoiceConfig, const FGameplayTag& participantTag) const
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const UADSVoiceConfigDataAsset* voiceData = Settings->GetVoiceConfigDataAsset();
    if (voiceData) {
        return voiceData->TryGetVoiceConfigForParticipant(outVoiceConfig, participantTag);
    }
    return false;
}
