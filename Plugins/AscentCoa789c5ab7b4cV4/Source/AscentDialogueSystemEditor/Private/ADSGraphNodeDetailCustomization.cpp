#include "ADSGraphNodeDetailCustomization.h"
#include "ADSEditorSubsystem.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
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

    // Find or create the ADS TTS category
    IDetailCategoryBuilder& TTSCategory = DetailBuilder.EditCategory("Voice Generation");

    // Add buttons row
    TTSCategory.AddCustomRow(FText::FromString("TTS Actions"))
        .WholeRowContent()
            [SNew(SHorizontalBox)

                // Generate TTS Button
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 0.0f)
                        [SNew(SButton)
                                .Text(FText::FromString("Generate TTS"))
                                .ToolTipText(FText::FromString("Generate audio file using ADS TTS"))
                                .OnClicked_Lambda([this]() {
                                    UADSEditorSubsystem* editorSubsystem = GEditor->GetEditorSubsystem<UADSEditorSubsystem>();
                                    editorSubsystem->GenerateTTSAudio(CachedNode.Get());

                                    return FReply::Handled();
                                })]];
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