// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ADSTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include <GameplayTagContainer.h>
#include <Subsystems/EngineSubsystem.h>

#include "ADSDialogueSubsystem.generated.h"

/**
 *
 */
class UADSDialoguePartecipantComponent;

/**
 * Dialogue Subsystem that manages participants and provides integration with TTS.
 * Runs as a UEngineSubsystem, so it's available globally in the project.
 */
UCLASS()
class ASCENTDIALOGUESYSTEM_API UADSDialogueSubsystem : public UEngineSubsystem {
    GENERATED_BODY()

public:
    /**
     * Returns the dialogue component of the provided participant.
     * @param participant - GameplayTag of the participant to search for
     * @return Pointer to the participant component if found, nullptr otherwise
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    UADSDialoguePartecipantComponent* FindParticipant(const FGameplayTag& participant) const;

    /**
     * Returns the default tag used for player responses in dialogues.
     * @return Default player response GameplayTag
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    FGameplayTag GetDefaultPlayerResponseTag() const;

    /**
     * Registers a participant so it can be referenced in dialogue graphs.
     * @param participant - Dialogue participant component to register
     */
    void RegisterParticipant(UADSDialoguePartecipantComponent* participant);

    /**
     * Unregisters a participant by tag, removing it from the subsystem.
     * @param participant - GameplayTag of the participant to remove
     */
    void UnregisterParticipant(const FGameplayTag& participant);

    /**
     * Returns all available TTS voice names currently cached or retrieved from API.
     * @return Array of available voice names
     */
    TArray<FString> GetAvailableVoiceNames() const;

    /**
     * Updates the selected voice ID in the participant from the cached names.
     * Should be triggered when the user selects a voice name in the editor.
     */
    void UpdateVoiceIDFromName();

    /** Checks if the cache is valid for the given API key */
    bool IsCacheValid(const FString& CurrentAPIKey) const { return TTSVoiceCache.IsValid(CurrentAPIKey); }

    /** Clears the voice cache */
    void ClearVoiceCache() { TTSVoiceCache.Clear(); }

    /** Updates the voice cache with new values */
    void UpdateVoiceCache(const TArray<FString>& NewVoiceNames, const TMap<FString, FString>& NewVoiceNameToID, const FString& APIKey)
    {
        TTSVoiceCache.Update(NewVoiceNames, NewVoiceNameToID, APIKey);
    }

    /**
     * Attempts to find the voice configuration for a given participant tag.
     * @param ParticipantTag  The gameplay tag identifying the participant.
     * @param outConfig       The voice configuration found, if any.
     * @return True if a configuration exists for the provided tag, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = ADS)
    bool TryGetParticipantVoiceConfig(FADSVoiceSettings& outVoiceConfig, const FGameplayTag& participantTag) const;

    /** Tries to resolve a VoiceID from a given VoiceName */
    bool GetVoiceIDByName(const FString& VoiceName, FString& OutID) const
    {
        if (const FString* FoundID = TTSVoiceCache.VoiceNameToID.Find(VoiceName)) {
            OutID = *FoundID;
            return true;
        }
        return false;
    }

private:
    /** Map of registered participants indexed by their GameplayTag */
    TMap<FGameplayTag, UADSDialoguePartecipantComponent*> Participants;

    /** Global TTS voice cache (accessible from both runtime and editor) */
    FADSVoiceCache TTSVoiceCache;
};
