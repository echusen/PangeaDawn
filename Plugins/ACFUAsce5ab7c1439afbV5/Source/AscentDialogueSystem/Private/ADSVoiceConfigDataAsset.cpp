// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSVoiceConfigDataAsset.h"

namespace {
/** Shown when a voice id has been pasted by hand and is not part of the fetched catalog. */
const FString UnknownVoiceName = TEXT("<custom voice id>");
}

bool UADSVoiceConfigDataAsset::TryGetVoiceNameByID(const FString& inVoiceID, FString& outName) const
{
    if (inVoiceID.IsEmpty()) {
        return false;
    }

    for (const TPair<FString, FString>& Entry : CachedVoiceNameToID) {
        if (Entry.Value == inVoiceID) {
            outName = Entry.Key;
            return true;
        }
    }

    return false;
}

bool UADSVoiceConfigDataAsset::TryGetVoiceConfigForParticipant(FADSVoiceSettings& outConfig, const FGameplayTag& participantTag) const
{
    if (const FADSVoiceSettings* FoundConfig = VoiceConfigs.Find(participantTag)) {
        outConfig = *FoundConfig;
        return true;
    }
    return false;
}

bool UADSVoiceConfigDataAsset::TryGetCachedVoiceIDByName(const FString& VoiceName, FString& OutID) const
{
    if (const FString* FoundID = CachedVoiceNameToID.Find(VoiceName)) {
        if (!FoundID->IsEmpty()) {
            OutID = *FoundID;
            return true;
        }
    }
    return false;
}

void UADSVoiceConfigDataAsset::PersistVoiceCatalog(const TArray<FString>& NewVoiceNames, const TMap<FString, FString>& NewVoiceNameToID)
{
    CachedVoiceNames = NewVoiceNames;
    CachedVoiceNameToID = NewVoiceNameToID;

    RefreshAllVoiceNames();

#if WITH_EDITOR
    Modify();
    MarkPackageDirty();
#endif
}

void UADSVoiceConfigDataAsset::PostLoad()
{
    Super::PostLoad();

    MigrateLegacyManualVoiceIDs();
    RefreshAllVoiceNames();
}

#if WITH_EDITOR
void UADSVoiceConfigDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

    static const FName VoiceIDName = GET_MEMBER_NAME_CHECKED(UADSVoiceConfigDataAsset, VoiceID);
    static const FName VoiceNameName = GET_MEMBER_NAME_CHECKED(UADSVoiceConfigDataAsset, SelectedVoiceName);

    const bool bEditedID = PropertyName == VoiceIDName;
    const bool bEditedName = PropertyName == VoiceNameName;

    if (!bEditedID && !bEditedName) {
        return;
    }

    // The details panel does not tell us which map entry was edited, so every entry is synced in the
    // direction of the edited field. That is safe because syncing an already consistent entry is a no-op,
    // and unresolvable values are left untouched instead of being overwritten.
    if (MemberName == GET_MEMBER_NAME_CHECKED(UADSVoiceConfigDataAsset, VoiceConfigs)) {
        for (TPair<FGameplayTag, FADSVoiceSettings>& ConfigPair : VoiceConfigs) {
            FADSVoiceSettings& Config = ConfigPair.Value;
            if (bEditedID) {
                SyncVoiceNameFromID(Config.VoiceID, Config.SelectedVoiceName);
            } else {
                SyncVoiceIDFromName(Config.VoiceID, Config.SelectedVoiceName);
            }
        }
    } else if (bEditedID) {
        SyncVoiceNameFromID(VoiceID, SelectedVoiceName);
    } else {
        SyncVoiceIDFromName(VoiceID, SelectedVoiceName);
    }
}
#endif

void UADSVoiceConfigDataAsset::SyncVoiceNameFromID(const FString& InVoiceID, FString& OutVoiceName) const
{
    FString ResolvedName;
    if (TryGetVoiceNameByID(InVoiceID, ResolvedName)) {
        OutVoiceName = ResolvedName;
    } else if (HasPersistedVoiceCatalog()) {
        // The catalog is there and this id is not in it, so the id was pasted straight from the website.
        OutVoiceName = UnknownVoiceName;
    }
}

void UADSVoiceConfigDataAsset::SyncVoiceIDFromName(FString& OutVoiceID, const FString& InVoiceName) const
{
    FString ResolvedID;
    if (TryGetCachedVoiceIDByName(InVoiceName, ResolvedID)) {
        OutVoiceID = ResolvedID;
    }
}

void UADSVoiceConfigDataAsset::RefreshAllVoiceNames()
{
    SyncVoiceNameFromID(VoiceID, SelectedVoiceName);

    for (TPair<FGameplayTag, FADSVoiceSettings>& ConfigPair : VoiceConfigs) {
        SyncVoiceNameFromID(ConfigPair.Value.VoiceID, ConfigPair.Value.SelectedVoiceName);
    }
}

bool UADSVoiceConfigDataAsset::MigrateLegacyManualVoiceIDs()
{
    bool bMigrated = false;

    if (!ManualVoiceID.IsEmpty()) {
        VoiceID = ManualVoiceID;
        ManualVoiceID.Empty();
        bMigrated = true;
    }

    for (TPair<FGameplayTag, FADSVoiceSettings>& ConfigPair : VoiceConfigs) {
        FADSVoiceSettings& Config = ConfigPair.Value;
        if (!Config.ManualVoiceID.IsEmpty()) {
            Config.VoiceID = Config.ManualVoiceID;
            Config.ManualVoiceID.Empty();
            bMigrated = true;
        }
    }

    return bMigrated;
}
