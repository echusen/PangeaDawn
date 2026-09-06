#include "ADSGraphNodeDetailCustomization.h"
#include "ADSEditorSubsystem.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Graph/ADSDialogue.h"
#include "Graph/ADSGraphNode.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include <Editor/EditorEngine.h>
#include "ADSVoiceConfigDataAsset.h"

TSharedRef<IDetailCustomization> FADSGraphNodeDetailCustomization::MakeInstance()
{
    return MakeShareable(new FADSGraphNodeDetailCustomization());
}

void FADSGraphNodeDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    // Get the object being customized
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);

    if (Objects.Num() == 1) {
        CachedNode = Cast<UADSGraphNode>(Objects[0].Get());
    }

    if (!CachedNode.IsValid()) {
        return;
    }

    IDetailCategoryBuilder& AIGenCategory = DetailBuilder.EditCategory(
        "AI Generation",
        FText::FromString("AI Generation"),
        ECategoryPriority::Default);

    AIGenCategory.AddCustomRow(FText::FromString("AI Generation Actions"))
        .WholeRowContent()
            [SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Generate TTS"))
                                .ToolTipText(FText::FromString("Generate audio from this node's dialogue text using ElevenLabs TTS."))
                                .OnClicked_Lambda([this]() {
                                    if (CachedNode.IsValid())
                                    {
                                        UADSEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();
                                        EditorSubsystem->GenerateTTSAudio(CachedNode.Get());
                                    }
                                    return FReply::Handled();
                                })]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Generate Facial Animation"))
                                .ToolTipText(FText::FromString(
                                    "Generate facial animation using MetaHuman Animator from this node's audio.\n"
                                    "Uses the node's GenerateBlink and Mood settings.\n"
                                    "Requires a SoundWave assigned to this node."))
                                .OnClicked_Lambda([this]() {
                                    if (CachedNode.IsValid())
                                    {
                                        UADSEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();
                                        EditorSubsystem->GenerateFacialAnimation(CachedNode.Get());
                                    }
                                    return FReply::Handled();
                                })]
            ];
}

TSharedRef<IDetailCustomization> FADSAIVoiceGeneratorComponentDetails::MakeInstance()
{
    return MakeShareable(new FADSAIVoiceGeneratorComponentDetails());
}

void FADSAIVoiceGeneratorComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    // Get the object being customized
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);

    // Find or create the ADS TTS category
    IDetailCategoryBuilder& TTSCategory = DetailBuilder.EditCategory("Voice Generation");

    if (Objects.Num() == 1) {
        CachedComponent = Cast<UADSVoiceConfigDataAsset>(Objects[0].Get());
    }

    if (!CachedComponent.IsValid()) {
        return;
    }

    // Add buttons row
    TTSCategory.AddCustomRow(FText::FromString("TTS Actions"))
        .WholeRowContent()
            [SNew(SHorizontalBox)

                // Preview Voice Button
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Preview Voice"))
                                .ToolTipText(FText::FromString("Preview the selected voice with sample text"))
                                .OnClicked_Lambda([this]() {
                                    UADSEditorSubsystem* editorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();

                                    editorSubsystem->PreviewSelectedVoice(CachedComponent->VoiceID);

                                    return FReply::Handled();
                                })]

    ];

    // Add second row for utility buttons
    TTSCategory.AddCustomRow(FText::FromString("TTS Utils"))
        .WholeRowContent()
            [SNew(SHorizontalBox)

                // Refresh Voices Button
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Refresh Voices"))
                                .ToolTipText(FText::FromString("Fetch latest voices from ElevenLabs API"))
                                .OnClicked_Lambda([this]() {
                                    UADSEditorSubsystem* editorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();
                                    editorSubsystem->RefreshVoicesFromAPI();
                                    return FReply::Handled();
                                })]

                // Clear Cache Button
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Clear Cache"))
                                .ToolTipText(FText::FromString("Clear cached voice data"))
                                .OnClicked_Lambda([this]() {
                                    UADSEditorSubsystem* editorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();
                                    editorSubsystem->ClearVoiceCache();
                                    return FReply::Handled();
                                })]];
}

// ============================================================================
// FADSDialogueDetailCustomization - Export/Import buttons for UADSDialogue
// ============================================================================

TSharedRef<IDetailCustomization> FADSDialogueDetailCustomization::MakeInstance()
{
    return MakeShareable(new FADSDialogueDetailCustomization());
}

void FADSDialogueDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    // Get the dialogue object being customized
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);

    if (Objects.Num() == 1)
    {
        CachedDialogue = Cast<UADSDialogue>(Objects[0].Get());
    }

    if (!CachedDialogue.IsValid())
    {
        return;
    }

    // Create the Export/Import category
    IDetailCategoryBuilder& ExportImportCategory = DetailBuilder.EditCategory(
        "Export Import",
        FText::FromString("Export / Import"),
        ECategoryPriority::Important);

    // Export button row
    ExportImportCategory.AddCustomRow(FText::FromString("Dialogue Export/Import Actions"))
        .WholeRowContent()
            [SNew(SHorizontalBox)

                // Export Button
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4.0f, 2.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Export Dialogue to Text"))
                                .ToolTipText(FText::FromString(
                                    "Export all dialogue text to a screenplay-formatted .txt file.\n"
                                    "The file will be saved in the Export/Import Directory with the dialogue asset name."))
                                .OnClicked_Lambda([this]() {
                                    if (CachedDialogue.IsValid())
                                    {
                                        UADSEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();
                                        EditorSubsystem->ExportDialogueToFile(CachedDialogue.Get());
                                    }
                                    return FReply::Handled();
                                })]

                // Import Button
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4.0f, 2.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Import Dialogue from Text"))
                                .ToolTipText(FText::FromString(
                                    "Import a screenplay-formatted .txt file and build the dialogue graph.\n"
                                    "Creates all dialogue nodes, response options, and connections.\n"
                                    "Participant names are auto-matched to GameplayTags (e.g., Character.Name).\n"
                                    "WARNING: This will replace the current dialogue graph."))
                                .OnClicked_Lambda([this]() {
                                    if (CachedDialogue.IsValid())
                                    {
                                        UADSEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();
                                        EditorSubsystem->ImportDialogueFromFile(CachedDialogue.Get());
                                    }
                                    return FReply::Handled();
                                })
                        ]
            ];
}