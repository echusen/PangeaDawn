// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "ADSEditorSubsystem.generated.h"

class UADSDialogue;
class UADSGraphNode;
class USoundCue;
class USoundWave;
class UAnimSequence;
class UAnimMontage;
class USkeleton;
struct FADSARKitCurveMapping;

/** Everything a writer (human or AI) needs to author import-ready screenplay text for a dialogue asset. */
USTRUCT(BlueprintType)
struct ASCENTDIALOGUESYSTEMEDITOR_API FADSDialogueWritingBrief
{
    GENERATED_BODY()

    /** Format and branching rules the screenplay text must follow (the AI system prompt). */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Instructions;

    /** Conversation description, dialogue context, target length and participant personalities. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Request;

    /** Speaker names allowed in the screenplay, resolved from the dialogue's AI Participant Tags. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> ParticipantNames;

    /** Speaker name used for player lines and "(Option N)" choices. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> Warnings;
};

/** Result of checking screenplay text against the importer before it is shown for confirmation. */
USTRUCT(BlueprintType)
struct ASCENTDIALOGUESYSTEMEDITOR_API FADSDialogueTextValidation
{
    GENERATED_BODY()

    /** False when the importer would reject or misbuild the text. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    int32 EntryCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    int32 OptionCount = 0;

    /** True when the text carries [Node:GUID] lines matching existing nodes, so import only updates them. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bUpdatesExistingNodes = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> Errors;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> Warnings;
};

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

    /**
     * Generates TTS audio for every dialogue node in the given dialogue (batch).
     * Nodes are processed sequentially to avoid hammering the API. This can be
     * expensive (one API call per node), so callers should confirm with the user first.
     *
     * @param bForceRegenerate If false (default), nodes that already have a sound assigned are
     *                         skipped. If true, every node with text is regenerated.
     */
    void GenerateTTSAudioForAllNodes(UADSDialogue* Dialogue, bool bForceRegenerate = false);

    /**
     * Generates MetaHuman facial animation for every dialogue node in the given dialogue (batch).
     * Twin of GenerateTTSAudioForAllNodes for facial animations. Nodes are processed sequentially
     * because the MetaHuman Animator pipeline is blocking. Only nodes that already have a SoundWave
     * are processed (facial animation is audio-driven), so generate TTS audio first.
     *
     * @param bForceRegenerate If false (default), nodes that already have a facial animation are
     *                         skipped. If true, every eligible node is regenerated.
     */
    void GenerateAnimationsForAllNodes(UADSDialogue* Dialogue, bool bForceRegenerate = false);

    /**
     * Generates TTS audio for every dialogue node across all of the supplied dialogues (batch).
     * Used by the Content Browser context menu when multiple Dialogue assets are selected.
     * Nodes are processed sequentially (one API call at a time) across the whole selection.
     *
     * @param bForceRegenerate If false (default), nodes that already have a sound assigned are
     *                         skipped. If true, every node with text is regenerated.
     */
    void GenerateTTSForDialogues(const TArray<UADSDialogue*>& Dialogues, bool bForceRegenerate = false);

    /**
     * Generates MetaHuman facial animation for every dialogue node (that already has a SoundWave
     * assigned) across all of the supplied dialogues. Processing is sequential because the
     * MetaHuman Animator pipeline is blocking and cannot run concurrently.
     *
     * @param bForceRegenerate If false (default), nodes that already have a facial animation are
     *                         skipped. If true, every eligible node is regenerated.
     */
    void GenerateFacialAnimationForDialogues(const TArray<UADSDialogue*>& Dialogues, bool bForceRegenerate = false);

    /** Returns true while a batch voice generation pass is running. */
    bool IsBatchGeneratingVoices() const { return bIsBatchGeneratingVoices; }

    /** Returns true while a batch facial animation pass is running. */
    bool IsBatchGeneratingFacialAnims() const { return bIsBatchGeneratingFacialAnims; }

    void PreviewSelectedVoice(const FString& voiceId);
    void RefreshVoicesFromAPI();
    void ClearVoiceCache();

    /**
     * Requests one page of voices and, while the API reports more, chains the following pages before
     * committing the whole catalog. ElevenLabs returns only 10 voices per page unless asked otherwise.
     */
    void FetchVoicePage(const FString& APIKey, const FString& NextPageToken, TSharedRef<TArray<FString>> AccumulatedNames,
        TSharedRef<TMap<FString, FString>> AccumulatedNameToID, int32 PageIndex);

    /** Stores the fetched catalog in the runtime cache and persists it on the VoiceConfig DataAsset. */
    void CommitVoiceCatalog(const TArray<FString>& VoiceNames, const TMap<FString, FString>& VoiceNameToID, const FString& APIKey);

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

    /**
     * Convert a MetaHuman control-curve AnimSequence into an ARKit blendshape-curve AnimSequence.
     * Reads the source rig control curves, remaps them to the 52 standard ARKit shapes using the
     * configured (or built-in) mapping, and bakes a new AnimSequence saved under the ARKit output path.
     * @return The generated ARKit AnimSequence, or nullptr on failure.
     */
    UAnimSequence* ConvertMetaHumanAnimToARKit(UAnimSequence* SourceSequence, const FString& BaseName, const FString& PackagePathOverride = FString());

    /**
     * Bake an ARKit blendshape animation from the facial animation already generated on a dialogue node.
     * Extracts the source AnimSequence from the node's facial-animation montage and remaps it to ARKit curves,
     * so no MetaHuman re-solve is required. Editor-only, on-demand action.
     */
    void GenerateARKitAnimationForNode(UADSGraphNode* DialogueNode);

    /** Populate the active MetaHuman -> ARKit curve mapping (DataTable override if set, otherwise the built-in default). */
    void GetARKitCurveMappings(TArray<FADSARKitCurveMapping>& OutMappings) const;

    /** Wrap an AnimSequence in a single-slot AnimMontage asset saved alongside it. */
    UAnimMontage* CreateMontageWrappingSequence(UAnimSequence* Sequence, const FString& MontageName, const FString& PackagePath);

    /** Export the dialogue to a screenplay-formatted text file */
    void ExportDialogueToFile(UADSDialogue* Dialogue);

    /** Build the screenplay text written by ExportDialogueToFile, including [Node:GUID] lines. */
    FString ExportDialogueToText(UADSDialogue* Dialogue) const;

    /** Import dialogue text from a screenplay-formatted text file */
    void ImportDialogueFromFile(UADSDialogue* Dialogue);

    /**
     * Recreate missing UADSGraphEdge transitions from existing editor wires / ChildrenNodes.
     * Does not replace nodes or overwrite conditions already authored on valid edges.
     */
    void RegenerateDialogueTransitions(UADSDialogue* Dialogue);

    /** Generate an entire dialogue with AI from the dialogue description + participant personalities, then preview it */
    void GenerateDialogueWithAI(UADSDialogue* Dialogue);

    /**
     * Build the writing rules and request used by GenerateDialogueWithAI from the dialogue's AI fields
     * (description, participant tags, response mode, tree length) and the Voice Config personalities.
     * @param DescriptionOverride Used instead of the asset's AI Conversation Description when not empty.
     */
    bool BuildDialogueWritingBrief(UADSDialogue* Dialogue, const FString& DescriptionOverride, FADSDialogueWritingBrief& OutBrief, FString& OutError) const;

    /** Parse screenplay text with the importer's parser and report what importing it would do. */
    FADSDialogueTextValidation ValidateDialogueText(UADSDialogue* Dialogue, const FString& ScreenplayText) const;

    /**
     * Open the editable screenplay preview. The dialogue graph is only changed when the user clicks Import.
     * @param SourceName Shown in the window title and import log.
     */
    void ShowGeneratedDialoguePreview(UADSDialogue* Dialogue, const FString& GeneratedText, const FString& SourceName = TEXT("AI Generated"));

private:
    /**
     * Core TTS request for a single node: validates inputs, calls the TTS API and (on success)
     * creates and assigns the resulting SoundWave asset.
     *
     * Validation failures invoke OnComplete synchronously; API results invoke it asynchronously.
     * OnComplete receives (bSuccess, Message) where Message is a human-readable result or error
     * suitable for a notification/tooltip. Every failure path also writes a UE_LOG entry.
     */
    void RequestTTSForNode(UADSGraphNode* DialogueNode, TFunction<void(bool /*bSuccess*/, const FString& /*Message*/)> OnComplete);

    /**
     * Drives a sequential voice-generation pass over the supplied nodes: shows a progress
     * notification, processes one node at a time (chaining each request from the previous
     * completion callback) and reports an aggregate summary at the end.
     */
    void StartVoiceBatch(TArray<TWeakObjectPtr<UADSGraphNode>> Nodes);

    /**
     * Collects every dialogue node with text from the supplied dialogues (deduplicated).
     * @param bSkipAlreadySet When true, nodes that already have a sound assigned are excluded.
     * @return The number of nodes that were skipped because they already had a sound assigned.
     */
    static int32 CollectVoiceNodes(const TArray<UADSDialogue*>& Dialogues, TArray<TWeakObjectPtr<UADSGraphNode>>& OutNodes, bool bSkipAlreadySet);

#if WITH_ADS_METAHUMAN
    /**
     * Core MetaHuman facial-animation generation for a single node. Blocking. Does not touch
     * notifications; returns success and a human-readable message via OutMessage instead, so it
     * can be reused by both the single-node and batch entry points.
     */
    bool GenerateFacialAnimationForNode(UADSGraphNode* DialogueNode, FString& OutMessage);
#endif // WITH_ADS_METAHUMAN

    /** True while GenerateTTSAudioForAllNodes is processing, used to drive the "In Progress" UI state. */
    bool bIsBatchGeneratingVoices = false;

    /** True while a batch facial-animation pass is processing. */
    bool bIsBatchGeneratingFacialAnims = false;
    /** Build the dialogue graph from screenplay-formatted text (shared by file import and AI generation) */
    void ImportDialogueFromText(UADSDialogue* Dialogue, const FString& ScreenplayText, const FString& SourceName, bool bConfirm = true);
};
