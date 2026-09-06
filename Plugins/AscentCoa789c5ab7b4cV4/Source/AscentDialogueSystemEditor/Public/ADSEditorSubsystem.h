// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "ADSEditorSubsystem.generated.h"

class UADSDialogue;
class UADSGraphNode;
class USoundCue;

/**
 *
 */
UCLASS()
class ASCENTDIALOGUESYSTEMEDITOR_API UADSEditorSubsystem : public UEditorSubsystem {
    GENERATED_BODY()

public:
    UADSEditorSubsystem();

    // TTS Action Functions (called by Detail Panel buttons)
    void GenerateTTSAudio(UADSGraphNode* dialogueNode);
    void PreviewSelectedVoice(const FString& voiceId);
    void RefreshVoicesFromAPI();
    void ClearVoiceCache();

    void FetchVoicesFromAPI(TArray<FString>& OutVoiceNames, TMap<FString, FString>& OutVoiceNameToID) const;
    void FetchVoicesAsync() const;
  //  void GetCachedVoiceNameToID(TMap<FString, FString>& OutVoiceNameToID) const;
    void PlayAudioFileInUnreal(const FString& FilePath);
    void DownloadAndPlayPreviewAudio(const FString& PreviewURL, const FString& voiceID);

    USoundWave* CreateSoundWaveFromAudioData(const TArray<uint8>& AudioData, const FString& SoundName);
    USoundWave* CreateSoundWaveAsset(const TArray<uint8>& AudioData, const FString& AssetName, const FString& PackagePath);
    USoundCue* CreateSoundCueFromWave(USoundWave* SoundWave, const FString& CueName);

    void ShowNotification(const FString& Message, bool bSuccess = true) const;

    /** Generate facial animation via MetaHuman Animator from the node's audio */
    void GenerateFacialAnimation(UADSGraphNode* DialogueNode);

    /** Export the dialogue to a screenplay-formatted text file */
    void ExportDialogueToFile(UADSDialogue* Dialogue);

    /** Import dialogue text from a screenplay-formatted text file */
    void ImportDialogueFromFile(UADSDialogue* Dialogue);
};
