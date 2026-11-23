// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ADSEditorSubsystem.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "ADSDialogueToolset.generated.h"

/** One dialogue participant and the voice the ADS Voice Config currently assigns to it. */
USTRUCT(BlueprintType)
struct ASCENTDIALOGUESYSTEMEDITOR_API FADSVoiceCastingEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString ParticipantTag;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString ParticipantName;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Personality;

    /** False when the Voice Config has no entry for this participant, so TTS falls back to the default voice. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bHasVoiceConfig = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString VoiceName;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString VoiceID;
};

/** Participants of a dialogue with their current voices, plus the voices fetched by "Refresh Voices". */
USTRUCT(BlueprintType)
struct ASCENTDIALOGUESYSTEMEDITOR_API FADSVoiceCasting
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FADSVoiceCastingEntry> Participants;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> AvailableVoiceNames;
};

/** Outcome of UADSDialogueToolset::SaveDialogue: where the asset is and what it holds after the call. */
USTRUCT(BlueprintType)
struct ASCENTDIALOGUESYSTEMEDITOR_API FADSDialogueSaveResult
{
    GENERATED_BODY()

    /** Object path of the saved dialogue, e.g. "/Game/MyProject/Dialogues/DA_Guard.DA_Guard". */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString ObjectPath;

    /** True when the call created the asset, false when it updated an existing one. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bCreated = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString DialogueClass;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> ParticipantTags;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString ConversationDescription;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString PlayerResponseMode;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    int32 TreeLength = 0;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> Warnings;
};

/**
 * Ascent Dialogue System tools for AI assistants. Create the dialogue asset with SaveDialogue, then author
 * its graph as screenplay text: get the writing brief, write (or export and revise) the text, validate it,
 * then open the preview so the user confirms the import. Tools never build the graph without that confirmation.
 */
UCLASS(BlueprintType, Hidden)
class ASCENTDIALOGUESYSTEMEDITOR_API UADSDialogueToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Returns the rules and request the dialogue editor's "Generate with AI" button sends to its model:
     * screenplay format, branching rules for the dialogue's player response mode, allowed speakers,
     * personalities from the Voice Config, and the target length. Write the screenplay from it.
     * @param DialoguePath Dialogue asset path, e.g. "/Game/MyProject/Dialogues/DA_GuardDialogue".
     * @param Description What the conversation is about. Pass an empty string to use the asset's
     *   AI Conversation Description. Raises a script error if neither is set, or the asset has no
     *   AI Participant Tags.
     * @return The writing brief.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FADSDialogueWritingBrief GetWritingBrief(const FString& DialoguePath, const FString& Description);

    /**
     * Returns the dialogue's current graph as screenplay text, with [Node:GUID] lines. Keep those lines
     * when revising: PreviewText then updates the matching nodes instead of replacing the graph.
     * @param DialoguePath Dialogue asset path.
     * @return The screenplay text.
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
    static FString ExportText(const FString& DialoguePath);

    /**
     * Checks screenplay text with the importer's parser without opening anything or changing the asset.
     * @param DialoguePath Dialogue asset path.
     * @param Text Screenplay text: "NAME:" line, dialogue text below it, one blank line between entries,
     *   and "NAME (Option N):" for player choices.
     * @return Entry and option counts, whether existing nodes would be updated, errors and warnings.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FADSDialogueTextValidation ValidateText(const FString& DialoguePath, const FString& Text);

    /**
     * Opens the editable dialogue preview window with the given screenplay text. The graph is only
     * changed when the user reviews the text and clicks Import; the tool returns immediately.
     * Raises a script error, and opens nothing, if the text fails validation.
     * @param DialoguePath Dialogue asset path.
     * @param Text Screenplay text, in the same format as ValidateText.
     * @return The validation result for the text that was shown.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FADSDialogueTextValidation PreviewText(const FString& DialoguePath, const FString& Text);

    /**
     * Starts TTS generation for every dialogue node with text, using the voices configured in the ADS
     * Voice Config. Spends TTS API credits: call only after the user approves. Runs in the background.
     * @param DialoguePath Dialogue asset path.
     * @param bForceRegenerate When true, also regenerates nodes that already have a sound assigned.
     * @return A short status message.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FString GenerateVoices(const FString& DialoguePath, bool bForceRegenerate);

    /**
     * Lists the dialogue's participants (AI Participant Tags and node speakers) with their Personality and
     * current voice from the ADS Voice Config, plus every voice name available to assign. Use it to propose
     * a voice per participant to the user. If no voices are cached yet, starts "Refresh Voices" and raises a
     * script error asking to call again once the refresh notification appears.
     * @param DialoguePath Dialogue asset path.
     * @return Participants and available voice names.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "ACF")
    static FADSVoiceCasting GetVoiceCasting(const FString& DialoguePath);

    /**
     * Assigns a voice to a participant in the ADS Voice Config and saves it. Applies to every dialogue
     * using that participant. Call only after the user approves the voice.
     * @param ParticipantTag Participant gameplay tag, e.g. "Character.Guard".
     * @param Voice A name from GetVoiceCasting's AvailableVoiceNames, or a raw voice id.
     * @return The participant's voice entry after the change.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FADSVoiceCastingEntry SetParticipantVoice(const FString& ParticipantTag, const FString& Voice);

    /**
     * Creates an ADS dialogue asset, or updates the writing settings of an existing one, and saves it.
     * This is the step before GetWritingBrief: the other tools need an existing dialogue with participants.
     * It never builds the graph; the graph still comes from PreviewText and the user's Import click.
     * Every input is checked before anything is written, so an invalid call changes nothing.
     * @param DialoguePath Where the dialogue lives, e.g. "/Game/MyProject/Dialogues/DA_GuardDialogue". The
     *   asset is created when missing. Engine content and paths outside a mounted content root are refused.
     * @param ParticipantTags Registered Character.* tags of every speaker, the player's included (its last
     *   segment must contain "Player" for the player to get choices). Pass an empty list to leave unchanged.
     * @param ConversationDescription What the conversation is about. Pass "" to leave unchanged.
     * @param PlayerResponseMode "OptionButtons", "DialogueNodes" or "Mixed". Pass "" to leave unchanged (a new
     *   dialogue starts on Mixed, a new world dialogue on DialogueNodes, the only mode a world dialogue accepts).
     * @param TreeLength Approximate number of speaker turns. Pass 0 to leave unchanged (a new asset starts at 8).
     * @param DialogueKind "Dialogue" (default) or "WorldDialogue" for a world-space dialogue. Only used when
     *   the asset is created; naming a different kind than the existing asset is refused. Pass "" for Dialogue.
     * @return The saved asset path and the settings it holds after the call.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FADSDialogueSaveResult SaveDialogue(const FString& DialoguePath, const TArray<FString>& ParticipantTags,
        const FString& ConversationDescription, const FString& PlayerResponseMode,
        int32 TreeLength, const FString& DialogueKind);
};
