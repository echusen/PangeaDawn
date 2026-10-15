// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "SACFAIToolsWindow.h"
#include "ThreeDBackend.h"
#include "AscentAIToolsSettings.h"
#include "AscentAIToolsSubsystem.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "AscentAITools"

namespace
{
	const FString ProviderGemini = TEXT("Gemini");
	const FString ProviderGptImage = TEXT("GPT Image");
	const FString ArtStyleRealistic = TEXT("Realistic");
	const FString ArtStyleSculpture = TEXT("Sculpture");
	const FString TopologyTriangle = TEXT("Triangle");
	const FString TopologyQuad = TEXT("Quad");
}

void SACFAIToolsWindow::Construct(const FArguments& InArgs)
{
	PreviewBrush = MakeShared<FSlateBrush>();
	PreviewBrush->DrawAs = ESlateBrushDrawType::NoDrawType;
	PreviewBrush->ImageSize = FVector2D(256.f, 256.f);

	ImageProviderOptions.Add(MakeShared<FString>(ProviderGemini));
	ImageProviderOptions.Add(MakeShared<FString>(ProviderGptImage));
	ArtStyleOptions.Add(MakeShared<FString>(ArtStyleRealistic));
	ArtStyleOptions.Add(MakeShared<FString>(ArtStyleSculpture));
	TopologyOptions.Add(MakeShared<FString>(TopologyTriangle));
	TopologyOptions.Add(MakeShared<FString>(TopologyQuad));

	if (const UEnum* ProviderEnum = StaticEnum<EACF3DProvider>())
	{
		for (int32 Index = 0; Index < ProviderEnum->NumEnums() - 1; ++Index)
		{
			ThreeDProviderOptions.Add(MakeShared<FString>(ProviderEnum->GetDisplayNameTextByIndex(Index).ToString()));
		}
	}
	if (ThreeDProviderOptions.IsEmpty())
	{
		ThreeDProviderOptions.Add(MakeShared<FString>(ACF3D::GetActiveBackend().GetDisplayName().ToString()));
	}

	const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
	const int32 ProviderIdx = (Settings && Settings->ImageProvider == EACFImageProvider::GptImage) ? 1 : 0;
	const int32 ThreeDIdx = (Settings && ThreeDProviderOptions.IsValidIndex(static_cast<int32>(Settings->ThreeDProvider))) ? static_cast<int32>(Settings->ThreeDProvider) : 0;
	const int32 ArtIdx = (Settings && Settings->MeshyArtStyle == EACFMeshyArtStyle::Sculpture) ? 1 : 0;
	const int32 TopoIdx = (Settings && Settings->MeshyTopology == EACFMeshyTopology::Quad) ? 1 : 0;

	// Listen to the subsystem so the UI reflects generation progress/results.
	if (UACFAIToolsSubsystem* Sub = GetSubsystem())
	{
		StatusHandle = Sub->OnStatusChangedNative.AddSP(this, &SACFAIToolsWindow::HandleStatus);
		ImageHandle = Sub->OnImageGeneratedNative.AddSP(this, &SACFAIToolsWindow::HandleImageGenerated);
		MeshHandle = Sub->OnMeshGeneratedNative.AddSP(this, &SACFAIToolsWindow::HandleMeshGenerated);
	}

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(12.f)
		[
			SNew(SVerticalBox)

			// Header
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				.Text(LOCTEXT("Title", "Ascent AI Tools - Image & 3D Generation"))
			]

			// Preview (black by default)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 10)
			[
				SNew(SBox)
				.WidthOverride(256.f)
				.HeightOverride(256.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SColorBlock).Color(FLinearColor::Black)
					]
					+ SOverlay::Slot()
					[
						SNew(SImage).Image(PreviewBrush.Get())
					]
				]
			]

			// Prompt label
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 2)
			[
				SNew(STextBlock).Text(LOCTEXT("PromptLabel", "Prompt"))
			]

			// Prompt box
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SBox).MinDesiredHeight(70.f)
				[
					SAssignNew(PromptBox, SMultiLineEditableTextBox)
					.HintText(LOCTEXT("PromptHint", "Describe what you want to generate..."))
					.AutoWrapText(true)
				]
			]

			// File name
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
			[
				SNew(STextBlock).Text(LOCTEXT("FileNameLabel", "File Name"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SAssignNew(FileNameBox, SEditableTextBox)
				.HintText(LOCTEXT("FileNameHint", "Asset name (leave empty to use the prompt)"))
			]

			// Image provider selector
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("ImageProvider", "Image Provider"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).MinDesiredWidth(160.f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&ImageProviderOptions)
						.InitiallySelectedItem(ImageProviderOptions[ProviderIdx])
						.OnGenerateWidget(this, &SACFAIToolsWindow::MakeComboItem)
						.OnSelectionChanged(this, &SACFAIToolsWindow::OnImageProviderChanged)
						[
							SNew(STextBlock).Text(this, &SACFAIToolsWindow::GetImageProviderText)
						]
					]
				]
			]

			// 3D provider selector
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("ThreeDProvider", "3D Provider"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).MinDesiredWidth(160.f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&ThreeDProviderOptions)
						.InitiallySelectedItem(ThreeDProviderOptions[ThreeDIdx])
						.OnGenerateWidget(this, &SACFAIToolsWindow::MakeComboItem)
						.OnSelectionChanged(this, &SACFAIToolsWindow::OnThreeDProviderChanged)
						[
							SNew(STextBlock).Text(this, &SACFAIToolsWindow::GetThreeDProviderText)
						]
					]
				]
			]

			// ---- Generation options ----
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.Text(this, &SACFAIToolsWindow::GetMeshOptionsTitle)
					]

					// Checkboxes row
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
						[
							SNew(SCheckBox)
							.IsChecked(this, &SACFAIToolsWindow::IsGenerateTextures)
							.OnCheckStateChanged(this, &SACFAIToolsWindow::OnGenerateTexturesChanged)
							[
								SNew(STextBlock).Text(LOCTEXT("OptTextures", "Generate Textures"))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
						[
							SNew(SCheckBox)
							.IsChecked(this, &SACFAIToolsWindow::IsEnablePBR)
							.OnCheckStateChanged(this, &SACFAIToolsWindow::OnEnablePBRChanged)
							.IsEnabled(this, &SACFAIToolsWindow::AreTexturesEnabled)
							[
								SNew(STextBlock).Text(LOCTEXT("OptPBR", "PBR"))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
						[
							SNew(SCheckBox)
							.IsChecked(this, &SACFAIToolsWindow::IsAutoRefine)
							.OnCheckStateChanged(this, &SACFAIToolsWindow::OnAutoRefineChanged)
							.Visibility_Lambda([]
							{
								return ACF3D::GetActiveBackend().GetCapabilities().bRefine ? EVisibility::Visible : EVisibility::Collapsed;
							})
							[
								SNew(STextBlock).Text(LOCTEXT("OptRefine", "Auto Refine (higher quality)"))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SCheckBox)
							.IsChecked(this, &SACFAIToolsWindow::IsRigHumanoid)
							.OnCheckStateChanged(this, &SACFAIToolsWindow::OnRigHumanoidChanged)
							.IsEnabled_Lambda([]
							{
								return ACF3D::GetActiveBackend().GetCapabilities().bRigging;
							})
							.ToolTipText_Lambda([]
							{
								return ACF3D::GetActiveBackend().GetCapabilities().bRigging ? FText::GetEmpty() : LOCTEXT("OptRigNoRiggingTip", "This provider cannot rig characters. Switch to Meshy or Tripo.");
							})
							[
								SNew(STextBlock).Text(LOCTEXT("OptRig", "Rig humanoid"))
							]
						]
					]

					// Grid: art style / topology / polycount
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FMargin(4.f))

						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("OptArtStyle", "Art Style"))
							.Visibility_Lambda([]
							{
								return ACF3D::GetActiveBackend().GetCapabilities().bArtStyle ? EVisibility::Visible : EVisibility::Collapsed;
							})
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&ArtStyleOptions)
							.InitiallySelectedItem(ArtStyleOptions[ArtIdx])
							.OnGenerateWidget(this, &SACFAIToolsWindow::MakeComboItem)
							.OnSelectionChanged(this, &SACFAIToolsWindow::OnArtStyleChanged)
							.Visibility_Lambda([]
							{
								return ACF3D::GetActiveBackend().GetCapabilities().bArtStyle ? EVisibility::Visible : EVisibility::Collapsed;
							})
							[
								SNew(STextBlock).Text(this, &SACFAIToolsWindow::GetArtStyleText)
							]
						]

						+ SUniformGridPanel::Slot(0, 1)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("OptTopology", "Topology"))
							.Visibility_Lambda([]
							{
								return ACF3D::GetActiveBackend().GetCapabilities().bTopology ? EVisibility::Visible : EVisibility::Collapsed;
							})
						]
						+ SUniformGridPanel::Slot(1, 1)
						[
							SNew(SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&TopologyOptions)
							.InitiallySelectedItem(TopologyOptions[TopoIdx])
							.OnGenerateWidget(this, &SACFAIToolsWindow::MakeComboItem)
							.OnSelectionChanged(this, &SACFAIToolsWindow::OnTopologyChanged)
							.Visibility_Lambda([]
							{
								return ACF3D::GetActiveBackend().GetCapabilities().bTopology ? EVisibility::Visible : EVisibility::Collapsed;
							})
							[
								SNew(STextBlock).Text(this, &SACFAIToolsWindow::GetTopologyText)
							]
						]

						+ SUniformGridPanel::Slot(0, 2)
						[
							SNew(STextBlock).Text(LOCTEXT("OptPolycount", "Target Polycount"))
						]
						+ SUniformGridPanel::Slot(1, 2)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(true)
							.MinValue(1000)
							.MaxValue(300000)
							.MinSliderValue(1000)
							.MaxSliderValue(300000)
							.Value(this, &SACFAIToolsWindow::GetTargetPolycount)
							.OnValueChanged(this, &SACFAIToolsWindow::OnTargetPolycountChanged)
						]
					]
				]
			]

			// ---- Buttons ----
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 8)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("GenImage", "Generate Image"))
					.ToolTipText(LOCTEXT("GenImageTip", "Generate an image from the prompt and import it as a Texture."))
					.IsEnabled(this, &SACFAIToolsWindow::IsImageIdle)
					.OnClicked(this, &SACFAIToolsWindow::OnGenerateImageClicked)
				]

				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0, 4, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("TextToMesh", "Generate 3D Mesh\nfrom Text"))
					.ToolTipText(LOCTEXT("TextToMeshTip", "Generate a 3D mesh from the text prompt and import it."))
					.IsEnabled(this, &SACFAIToolsWindow::IsTextToMeshEnabled)
					.OnClicked(this, &SACFAIToolsWindow::OnTextToMeshClicked)
				]

				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("ImageToMesh", "Generate 3D Mesh\nfrom Image"))
					.ToolTipText(LOCTEXT("ImageToMeshTip", "Use the last generated image as input for the 3D mesh."))
					.IsEnabled(this, &SACFAIToolsWindow::IsImageToMeshEnabled)
					.OnClicked(this, &SACFAIToolsWindow::OnImageToMeshClicked)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenSettings", "Open Settings (API Keys)"))
				.ToolTipText(LOCTEXT("OpenSettingsTip", "Each 3D provider keeps its own API key. Keys are configured in Project Settings."))
				.OnClicked(this, &SACFAIToolsWindow::OnOpenSettingsClicked)
			]

			// Inline status / error message
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.Visibility(this, &SACFAIToolsWindow::GetStatusVisibility)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.ColorAndOpacity(this, &SACFAIToolsWindow::GetStatusColor)
					.Text(this, &SACFAIToolsWindow::GetStatusText)
				]
			]
		]
	];
}

SACFAIToolsWindow::~SACFAIToolsWindow()
{
	if (UACFAIToolsSubsystem* Sub = GetSubsystem())
	{
		Sub->OnStatusChangedNative.Remove(StatusHandle);
		Sub->OnImageGeneratedNative.Remove(ImageHandle);
		Sub->OnMeshGeneratedNative.Remove(MeshHandle);
	}
}

UACFAIToolsSubsystem* SACFAIToolsWindow::GetSubsystem() const
{
	return GEditor ? GEditor->GetEditorSubsystem<UACFAIToolsSubsystem>() : nullptr;
}

FString SACFAIToolsWindow::GetPromptText() const
{
	return PromptBox.IsValid() ? PromptBox->GetText().ToString() : FString();
}

FString SACFAIToolsWindow::GetFileName() const
{
	return FileNameBox.IsValid() ? FileNameBox->GetText().ToString() : FString();
}

bool SACFAIToolsWindow::IsImageIdle() const
{
	const UACFAIToolsSubsystem* Sub = GetSubsystem();
	return !Sub || !Sub->IsImageBusy();
}

bool SACFAIToolsWindow::IsMeshIdle() const
{
	const UACFAIToolsSubsystem* Sub = GetSubsystem();
	return !Sub || !Sub->IsMeshBusy();
}

bool SACFAIToolsWindow::HasGeneratedImage() const
{
	const UACFAIToolsSubsystem* Sub = GetSubsystem();
	return Sub && Sub->HasGeneratedImage() && !Sub->IsMeshBusy();
}

void SACFAIToolsWindow::HandleStatus(const FText& Status)
{
	SetStatus(Status, EACFAIToolsStatusLevel::Info);
}

void SACFAIToolsWindow::HandleImageGenerated(UTexture2D* Texture, bool bSuccess, const FString& Error)
{
	if (bSuccess && Texture)
	{
		SetPreviewTexture(Texture);
		SetStatus(LOCTEXT("ImgOkMsg", "Image generated and imported."), EACFAIToolsStatusLevel::Success);
	}
	else
	{
		SetStatus(FText::Format(LOCTEXT("ImgFailUI", "Image generation failed: {0}"), FText::FromString(Error)), EACFAIToolsStatusLevel::Error);
	}
}

void SACFAIToolsWindow::HandleMeshGenerated(UObject* Mesh, bool bSuccess, const FString& Error)
{
	if (bSuccess && Mesh)
	{
		if (GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Mesh);
		}
		SetStatus(LOCTEXT("MeshOkMsg", "3D mesh generated and imported."), EACFAIToolsStatusLevel::Success);
	}
	else
	{
		SetStatus(FText::Format(LOCTEXT("MeshFailUI", "3D generation failed: {0}"), FText::FromString(Error)), EACFAIToolsStatusLevel::Error);
	}
}

void SACFAIToolsWindow::SetPreviewTexture(UTexture2D* Texture)
{
	PreviewTexture = Texture;
	if (PreviewBrush.IsValid())
	{
		PreviewBrush->SetResourceObject(Texture);
		if (Texture)
		{
			PreviewBrush->DrawAs = ESlateBrushDrawType::Image;
			PreviewBrush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		}
		else
		{
			PreviewBrush->DrawAs = ESlateBrushDrawType::NoDrawType;
		}
	}
}

// ----------------------------------------------------------------------------
// Generation options
// ----------------------------------------------------------------------------

UACFAIToolsSettings* SACFAIToolsWindow::MutableSettings()
{
	return GetMutableDefault<UACFAIToolsSettings>();
}

void SACFAIToolsWindow::SaveSettings()
{
	MutableSettings()->SaveConfig();
}

void SACFAIToolsWindow::SetStatus(const FText& Message, EACFAIToolsStatusLevel Level)
{
	StatusMessage = Message;
	StatusLevel = Level;
}

FSlateColor SACFAIToolsWindow::GetStatusColor() const
{
	switch (StatusLevel)
	{
	case EACFAIToolsStatusLevel::Success:	return FSlateColor(FLinearColor(0.25f, 0.8f, 0.35f));
	case EACFAIToolsStatusLevel::Error:	return FSlateColor(FLinearColor(0.92f, 0.32f, 0.32f));
	default:							return FSlateColor::UseForeground();
	}
}

EVisibility SACFAIToolsWindow::GetStatusVisibility() const
{
	return StatusMessage.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedRef<SWidget> SACFAIToolsWindow::MakeComboItem(TSharedPtr<FString> Item)
{
	return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
}

void SACFAIToolsWindow::OnImageProviderChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type /*SelectInfo*/)
{
	if (!NewValue.IsValid())
	{
		return;
	}
	MutableSettings()->ImageProvider = (*NewValue == ProviderGptImage) ? EACFImageProvider::GptImage : EACFImageProvider::Gemini;
	SaveSettings();
}

FText SACFAIToolsWindow::GetImageProviderText() const
{
	return FText::FromString(UACFAIToolsSettings::Get()->ImageProvider == EACFImageProvider::GptImage ? ProviderGptImage : ProviderGemini);
}

void SACFAIToolsWindow::OnThreeDProviderChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type /*SelectInfo*/)
{
	if (!NewValue.IsValid())
	{
		return;
	}
	const int32 Index = ThreeDProviderOptions.IndexOfByKey(NewValue);
	if (Index == INDEX_NONE)
	{
		return;
	}
	MutableSettings()->ThreeDProvider = static_cast<EACF3DProvider>(Index);
	SaveSettings();
}

FText SACFAIToolsWindow::GetThreeDProviderText() const
{
	return ACF3D::GetActiveBackend().GetDisplayName();
}

FText SACFAIToolsWindow::GetMeshOptionsTitle() const
{
	return FText::FromString(TEXT("3D Mesh Options (") + ACF3D::GetActiveBackend().GetDisplayName().ToString() + TEXT(")"));
}

bool SACFAIToolsWindow::IsTextToMeshEnabled() const
{
	return IsMeshIdle() && ACF3D::GetActiveBackend().GetCapabilities().bTextTo3D;
}

bool SACFAIToolsWindow::IsImageToMeshEnabled() const
{
	return HasGeneratedImage() && ACF3D::GetActiveBackend().GetCapabilities().bImageTo3D;
}

ECheckBoxState SACFAIToolsWindow::IsGenerateTextures() const
{
	return UACFAIToolsSettings::Get()->bMeshyGenerateTextures ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SACFAIToolsWindow::OnGenerateTexturesChanged(ECheckBoxState NewState)
{
	MutableSettings()->bMeshyGenerateTextures = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

bool SACFAIToolsWindow::AreTexturesEnabled() const
{
	return UACFAIToolsSettings::Get()->bMeshyGenerateTextures;
}

ECheckBoxState SACFAIToolsWindow::IsEnablePBR() const
{
	return UACFAIToolsSettings::Get()->bMeshyEnablePBR ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SACFAIToolsWindow::OnEnablePBRChanged(ECheckBoxState NewState)
{
	MutableSettings()->bMeshyEnablePBR = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

ECheckBoxState SACFAIToolsWindow::IsAutoRefine() const
{
	return UACFAIToolsSettings::Get()->bMeshyAutoRefine ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SACFAIToolsWindow::OnAutoRefineChanged(ECheckBoxState NewState)
{
	MutableSettings()->bMeshyAutoRefine = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

ECheckBoxState SACFAIToolsWindow::IsRigHumanoid() const
{
	return UACFAIToolsSettings::Get()->bMeshyRigHumanoid ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SACFAIToolsWindow::OnRigHumanoidChanged(ECheckBoxState NewState)
{
	MutableSettings()->bMeshyRigHumanoid = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

TOptional<int32> SACFAIToolsWindow::GetTargetPolycount() const
{
	return UACFAIToolsSettings::Get()->MeshyTargetPolycount;
}

void SACFAIToolsWindow::OnTargetPolycountChanged(int32 NewValue)
{
	MutableSettings()->MeshyTargetPolycount = NewValue;
	SaveSettings();
}

void SACFAIToolsWindow::OnArtStyleChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type /*SelectInfo*/)
{
	if (!NewValue.IsValid())
	{
		return;
	}
	MutableSettings()->MeshyArtStyle = (*NewValue == ArtStyleSculpture) ? EACFMeshyArtStyle::Sculpture : EACFMeshyArtStyle::Realistic;
	SaveSettings();
}

FText SACFAIToolsWindow::GetArtStyleText() const
{
	return FText::FromString(UACFAIToolsSettings::Get()->MeshyArtStyle == EACFMeshyArtStyle::Sculpture ? ArtStyleSculpture : ArtStyleRealistic);
}

void SACFAIToolsWindow::OnTopologyChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type /*SelectInfo*/)
{
	if (!NewValue.IsValid())
	{
		return;
	}
	MutableSettings()->MeshyTopology = (*NewValue == TopologyQuad) ? EACFMeshyTopology::Quad : EACFMeshyTopology::Triangle;
	SaveSettings();
}

FText SACFAIToolsWindow::GetTopologyText() const
{
	return FText::FromString(UACFAIToolsSettings::Get()->MeshyTopology == EACFMeshyTopology::Quad ? TopologyQuad : TopologyTriangle);
}

// ----------------------------------------------------------------------------
// Generation actions
// ----------------------------------------------------------------------------

FReply SACFAIToolsWindow::OnGenerateImageClicked()
{
	if (UACFAIToolsSubsystem* Sub = GetSubsystem())
	{
		Sub->GenerateImage(GetPromptText(), GetFileName());
	}
	return FReply::Handled();
}

FReply SACFAIToolsWindow::OnTextToMeshClicked()
{
	if (UACFAIToolsSubsystem* Sub = GetSubsystem())
	{
		const bool bRig = UACFAIToolsSettings::Get()->bMeshyRigHumanoid && ACF3D::GetActiveBackend().GetCapabilities().bRigging;
		Sub->GenerateMeshFromText(GetPromptText(), GetFileName(), bRig, 0.0f);
	}
	return FReply::Handled();
}

FReply SACFAIToolsWindow::OnImageToMeshClicked()
{
	if (UACFAIToolsSubsystem* Sub = GetSubsystem())
	{
		const bool bRig = UACFAIToolsSettings::Get()->bMeshyRigHumanoid && ACF3D::GetActiveBackend().GetCapabilities().bRigging;
		Sub->GenerateMeshFromImage(GetFileName(), bRig, 0.0f);
	}
	return FReply::Handled();
}

FReply SACFAIToolsWindow::OnOpenSettingsClicked()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		const UACFAIToolsSettings* S = UACFAIToolsSettings::Get();
		SettingsModule->ShowViewer(S->GetContainerName(), S->GetCategoryName(), S->GetSectionName());
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
