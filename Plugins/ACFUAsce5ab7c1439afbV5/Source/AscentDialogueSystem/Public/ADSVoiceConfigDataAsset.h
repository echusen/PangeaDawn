// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ADSDialogueSubsystem.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include <Engine/Engine.h>

#include "ADSVoiceConfigDataAsset.generated.h"

class UADSDialogueSubsystem;

/**
 *
 */
UCLASS()
class ASCENTDIALOGUESYSTEM_API UADSVoiceConfigDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()

public:
    /**
     * Returns the list of available voice names retrieved from the Dialogue Subsystem.
     * @return Array of available voice names.
     */
    UFUNCTION(BlueprintPure, Category = "ADS")
    TArray<FString> GetAvailableVoiceNames() const
    {
        return GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()->GetAvailableVoiceNames();
    }

    /**
     * Resolves the display name of a voice from its id, using the persisted voice catalog.
     * @param inVoiceID  The ElevenLabs voice id to look up.
     * @param outName    The display name of that voice, if known.
     * @return True when the id exists in the catalog, false otherwise.
     */
    UFUNCTION(BlueprintPure, Category = "ADS", meta = (DisplayName = "Try Get Voice Name By ID", Keywords = "voice tts elevenlabs name"))
    bool TryGetVoiceNameByID(const FString& inVoiceID, FString& outName) const;

    /**
     * Attempts to find the voice configuration for a given participant tag.
     * @param ParticipantTag  The gameplay tag identifying the participant.
     * @param outConfig       The voice configuration found, if any.
     * @return True if a configuration exists for the provided tag, false otherwise.
     */
    UFUNCTION(BlueprintPure, Category = "ADS")
    bool TryGetVoiceConfigForParticipant(FADSVoiceSettings& outConfig, const FGameplayTag& participantTag) const;

    /** Persisted voice catalog from the last successful API refresh. Source of truth for name→ID resolution. */
    const TArray<FString>& GetCachedVoiceNames() const { return CachedVoiceNames; }

    bool TryGetCachedVoiceIDByName(const FString& VoiceName, FString& OutID) const;

    /** Writes the voice catalog to this asset (editor / refresh path). Marks the package dirty. */
    void PersistVoiceCatalog(const TArray<FString>& NewVoiceNames, const TMap<FString, FString>& NewVoiceNameToID);

    bool HasPersistedVoiceCatalog() const { return CachedVoiceNames.Num() > 0 && CachedVoiceNameToID.Num() > 0; }

    /**
     * Voice played by the "Preview Voice" button: paste any voice id copied from elevenlabs.io.
     * Editing it rewrites Preview Voice Name, and the id always wins when the two disagree.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarin|Voice Generation", meta = (DisplayName = "Preview Voice ID"))
    FString VoiceID = "lUTamkMw7gOzZbFIwmq4";

    /** Convenience picker over the voices fetched by "Refresh Voices". Choosing one rewrites Preview Voice ID. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarin|Voice Generation", meta = (DisplayName = "Preview Voice Name", GetOptions = "GetAvailableVoiceNames"))
    FString SelectedVoiceName = "James - Professional British Male";

    /** Deprecated: merged into VoiceID. Kept unexposed so existing assets can be migrated on load. */
    UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use VoiceID instead."))
    FString ManualVoiceID;

    UPROPERTY(EditAnywhere, meta = (Categories = "Character"), BlueprintReadWrite, Category = "Yarin|Voice Config")
    TMap<FGameplayTag, FADSVoiceSettings> VoiceConfigs;

protected:
    virtual void PostLoad() override;

#if WITH_EDITOR

    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif

    /** Rewrites the display name from the voice id. No-op when the id is not in the catalog. */
    void SyncVoiceNameFromID(const FString& InVoiceID, FString& OutVoiceName) const;

    /** Rewrites the voice id from the display name. No-op when the name is not in the catalog. */
    void SyncVoiceIDFromName(FString& OutVoiceID, const FString& InVoiceName) const;

    /** Rewrites the display name of every voice entry from its current voice id. */
    void RefreshAllVoiceNames();

    /** Moves any legacy ManualVoiceID override onto VoiceID. Returns true when something was migrated. */
    bool MigrateLegacyManualVoiceIDs();

    /** Last refreshed voice display names (disk-persistent; survives editor restart) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarin|Voice Generation|Cached Catalog")
    TArray<FString> CachedVoiceNames;

    /** Last refreshed name→voice_id map (disk-persistent; source of truth for resolving SelectedVoiceName) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarin|Voice Generation|Cached Catalog")
    TMap<FString, FString> CachedVoiceNameToID;
};
