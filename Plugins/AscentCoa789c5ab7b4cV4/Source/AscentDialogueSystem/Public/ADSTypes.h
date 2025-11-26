// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "ADSCameraConfigDataAsset.h"
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <Engine/DataTable.h>

#include "ADSTypes.generated.h"

class UADSCameraConfigDataAsset;
class ULevelSequence;

/**
 *
 */

 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueEnded, UADSDialogue*, dialogue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueStarted, UADSDialogue*, dialogue);

UENUM(BlueprintType)
enum class EDialogueCinematicType : uint8 {
    None, // No cinematic
    CameraConfig, // Procedural system with shot asset
    Sequencer // Level Sequence
};

UENUM(BlueprintType)
enum class EDialogueNodeDurationType : uint8
{
    // Automatically calculate duration from audio length
    Auto UMETA(DisplayName = "Auto (From Audio)"),
    
    // Use custom duration value in seconds
    Custom UMETA(DisplayName = "Custom Duration"),
    
    // Calculate based on text length (reading speed)
    TextBased UMETA(DisplayName = "Text-Based (Reading Speed)")
};

USTRUCT(BlueprintType)
struct FDialogueCinematic {
    GENERATED_BODY()

public:
    FDialogueCinematic()
    {
        CameraConfig = nullptr;
        LevelSequence = nullptr;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
    EDialogueCinematicType CinematicType = EDialogueCinematicType::None; // None = use participant default

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "CinematicType == EDialogueCinematicType::CameraConfig"))
    UADSCameraConfigDataAsset* CameraConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "CinematicType == EDialogueCinematicType::Sequencer"))
    ULevelSequence* LevelSequence;
};

/**
 * Cache used to store voices retrieved from TTS API.
 * Accessible globally via the Dialogue Subsystem.
 */
USTRUCT()
struct FADSVoiceSettings : public FTableRowBase {
    GENERATED_BODY()

public:
    FADSVoiceSettings() { };



    /** Voice display name selected from the available ElevenLabs voices */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS", meta = (GetOptions = "GetAvailableVoiceNames"))
    FString SelectedVoiceName = "James - Professional British Male";

    /** Voice ID automatically retrieved from the ElevenLabs service for the selected voice */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ADS")
    FString VoiceID = "lUTamkMw7gOzZbFIwmq4";

    /** Optional manual override of the Voice ID */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS")
    FString ManualVoiceID;

    TArray<FString> GetAvailableVoiceNames() const;

    FString GetSelectedVoiceID() const
    {
        return !ManualVoiceID.IsEmpty() ? ManualVoiceID : VoiceID;
    }
};

/**
 * Cache used to store voices retrieved from TTS API.
 * Accessible globally via the Dialogue Subsystem.
 */
USTRUCT()
struct FADSVoiceCache {
    GENERATED_BODY()

    /** Cached list of available voice display names */
    TArray<FString> VoiceNames;

    /** Mapping from voice display name to internal voice ID */
    TMap<FString, FString> VoiceNameToID;

    /** Last API key used for fetching voices */
    FString LastAPIKey;

    /** Time when the cache was last updated */
    FDateTime LastCacheTime = FDateTime::MinValue();

    /** Cache expiry threshold in hours */
    static constexpr double CACHE_EXPIRY_HOURS = 1.0;

    bool IsValid(const FString& CurrentAPIKey) const
    {
        return LastAPIKey == CurrentAPIKey && (FDateTime::Now() - LastCacheTime).GetTotalHours() < CACHE_EXPIRY_HOURS && VoiceNames.Num() > 0;
    }

    void Clear()
    {
        VoiceNames.Empty();
        VoiceNameToID.Empty();
        LastAPIKey.Empty();
        LastCacheTime = FDateTime::MinValue();
    }

    void Update(const TArray<FString>& NewVoiceNames, const TMap<FString, FString>& NewVoiceNameToID, const FString& APIKey)
    {
        VoiceNames = NewVoiceNames;
        VoiceNameToID = NewVoiceNameToID;
        LastAPIKey = APIKey;
        LastCacheTime = FDateTime::Now();
    }
};

UCLASS()
class ASCENTDIALOGUESYSTEM_API UADSTypes : public UObject {
    GENERATED_BODY()
};
