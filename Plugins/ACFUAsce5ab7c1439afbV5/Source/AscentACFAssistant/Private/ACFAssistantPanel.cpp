// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantClientInternal.h"
#include "ACFAssistantOwnPlugin.h"
#include "ACFAssistantPhoneAccess.h"
#include "ACFAssistantMesh.h"
#include "ACFAssistantMarkdown.h"
#include "SACFAssistantMeshCard.h"
#include "ACFAssistantSettings.h"
#include "AscentAIToolsSettings.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "SACFContextMeter.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"

namespace
{
	TSharedPtr<SWindow> SettingsWindow;
	TSharedPtr<FSlateDynamicImageBrush> YarinLongBrush;
	TSharedPtr<FSlateDynamicImageBrush> YarinLogoBrush;
	TSharedPtr<FSlateVectorImageBrush> SidebarBrush;
	TSharedPtr<FSlateVectorImageBrush> AttachImageBrush;
	TSharedPtr<FSlateVectorImageBrush> SendBrush;
	TSharedPtr<FSlateVectorImageBrush> StopBrush;
	constexpr float AssistantCornerRadius = 8.f;

	// Dusty parchment gold from the Ascent Combat Framework Ultimate splash: sRGB 182, 176, 154, a low-saturation khaki rather than a bright yellow.
	FLinearColor AccentLinear() { return FLinearColor::FromSRGBColor(FColor(182, 176, 154)); }
	FSlateColor Accent() { return AccentLinear(); }
	FSlateColor Surface() { return FLinearColor(0.08f, 0.085f, 0.1f); }
	FSlateColor SurfaceRaised() { return FLinearColor(0.11f, 0.115f, 0.14f); }
	FSlateColor ConversationIdle() { return FLinearColor(0.04f, 0.042f, 0.05f); }
	FSlateColor Muted() { return FLinearColor(0.55f, 0.58f, 0.64f); }
	FSlateColor WorkDim()
	{
		FLinearColor Color(0.55f, 0.58f, 0.64f);
		Color.A = 0.78f;
		return Color;
	}
	FSlateColor UserBubble() { return FLinearColor(0.16f, 0.22f, 0.32f); }
	FSlateColor AssistantBubble() { return FLinearColor(0.13f, 0.13f, 0.15f); }
	FSlateColor ApprovalCard() { return FLinearColor(0.22f, 0.30f, 0.46f); }

	const FSlateBrush* SidebarIcon()
	{
		if (!SidebarBrush.IsValid())
		{
			if (const TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner())
			{
				SidebarBrush = MakeShared<FSlateVectorImageBrush>(Plugin->GetBaseDir() / TEXT("Resources/Sidebar.svg"), FVector2D(20.0f, 20.0f));
			}
		}
		return SidebarBrush.Get();
	}

	const FSlateBrush* ComposerIcon(TSharedPtr<FSlateVectorImageBrush>& Brush, const TCHAR* Resource)
	{
		if (!Brush.IsValid())
		{
			if (const auto Plugin = ACFAssistantPlugin::Owner())
			{
				Brush = MakeShared<FSlateVectorImageBrush>(Plugin->GetBaseDir() / Resource, FVector2D(20, 20));
			}
		}
		return Brush.Get();
	}

	const FSlateBrush* ComposerBackground()
	{
		// The editor's own panel gray (the Details and Outliner body), so the composer follows the active editor theme.
		static const FSlateRoundedBoxBrush Brush(FStyleColors::Panel, AssistantCornerRadius);
		return &Brush;
	}

	const FSlateBrush* CardBackground(bool bPanelTint = true)
	{
		static const FSlateRoundedBoxBrush PanelBrush(FAppStyle::GetBrush("ToolPanel.GroupBorder")->TintColor, AssistantCornerRadius);
		static const FSlateRoundedBoxBrush SelectionBrush(FLinearColor::White, AssistantCornerRadius);
		return bPanelTint ? &PanelBrush : &SelectionBrush;
	}

	const FEditableTextBoxStyle& ComposerTextStyle()
	{
		static const FEditableTextBoxStyle Style = []
		{
			FEditableTextBoxStyle Result = FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			const FSlateBrush NoBrush = *FCoreStyle::Get().GetBrush("NoBrush");
			Result.SetBackgroundImageNormal(NoBrush).SetBackgroundImageHovered(NoBrush)
				.SetBackgroundImageFocused(NoBrush).SetBackgroundImageReadOnly(NoBrush);
			return Result;
		}();
		return Style;
	}

	const FButtonStyle& AssistantButtonStyle()
	{
		static const FButtonStyle Style = []
		{
			FButtonStyle Result = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton");
			Result.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, AssistantCornerRadius))
				.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Dropdown, AssistantCornerRadius))
				.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Input, AssistantCornerRadius))
				.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::Transparent, AssistantCornerRadius));
			return Result;
		}();
		return Style;
	}

	const FButtonStyle& FilledButtonStyle()
	{
		static const FButtonStyle Style = []
		{
			FButtonStyle Result = AssistantButtonStyle();
			// The editor's tab-well color (behind the Details / World Settings tabs): pills read as part of the editor, not as a second theme.
			Result.SetNormal(FSlateRoundedBoxBrush(FStyleColors::Background, AssistantCornerRadius))
				.SetDisabled(FSlateRoundedBoxBrush(FStyleColors::Background, AssistantCornerRadius));
			return Result;
		}();
		return Style;
	}

	const FButtonStyle& PrimaryButtonStyle()
	{
		static const FButtonStyle Style = []
		{
			FButtonStyle Result = AssistantButtonStyle();
			const auto Gold = [](uint8 R, uint8 G, uint8 B) { return FLinearColor::FromSRGBColor(FColor(R, G, B)); };
			const FLinearColor Edge = Gold(0xA4, 0x8F, 0x52);
			Result.SetNormal(FSlateRoundedBoxBrush(Gold(0xC2, 0xAD, 0x6C), AssistantCornerRadius, Edge, 1.0f))
				.SetHovered(FSlateRoundedBoxBrush(Gold(0xD6, 0xC4, 0x8C), AssistantCornerRadius, Edge, 1.0f))
				.SetPressed(FSlateRoundedBoxBrush(Gold(0xB4, 0xA0, 0x61), AssistantCornerRadius, Edge, 1.0f))
				.SetDisabled(FSlateRoundedBoxBrush(Gold(0x7A, 0x70, 0x52), AssistantCornerRadius, Edge, 1.0f));
			return Result;
		}();
		return Style;
	}

	const FSlateBrush* YarinLongImage()
	{
		if (!YarinLongBrush.IsValid())
		{
			if (TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner())
			{
				const FString Path = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Resources/YarinLong.png"));
				// Native pixel size is 1672x941; keep the aspect so the wordmark stays readable in the header.
				YarinLongBrush = MakeShareable(new FSlateDynamicImageBrush(FName(*Path), FVector2D(142.0f, 80.0f)));
			}
		}
		return YarinLongBrush.Get();
	}

	const FSlateBrush& ProviderDotBrush(EACFAssistantProvider Provider)
	{
		static const FSlateRoundedBoxBrush ClaudeDot(FLinearColor::FromSRGBColor(FColor(0xD9, 0x77, 0x57)), 5.0f);
		static const FSlateRoundedBoxBrush CodexDot(FLinearColor::FromSRGBColor(FColor(0x10, 0xA3, 0x7F)), 5.0f);
		static const FSlateRoundedBoxBrush YarinDot(AccentLinear(), 5.0f);
		switch (Provider)
		{
		case EACFAssistantProvider::ClaudeCode: return ClaudeDot;
		case EACFAssistantProvider::Codex: return CodexDot;
		default: return YarinDot;
		}
	}

	TSharedRef<SWidget> MakeProviderDot(EACFAssistantProvider Provider, float Diameter)
	{
		if (Provider == EACFAssistantProvider::Yarin)
		{
			if (!YarinLogoBrush.IsValid())
			{
				if (TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner())
				{
					const FString Path = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Resources/YarinIcon.png"));
					YarinLogoBrush = MakeShareable(new FSlateDynamicImageBrush(FName(*Path), FVector2D(16.0f, 16.0f)));
				}
			}
			return SNew(SImage).Image(YarinLogoBrush.Get());
		}
		return SNew(SBox).WidthOverride(Diameter).HeightOverride(Diameter).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(&ProviderDotBrush(Provider)).Padding(0)
			[
				SNew(SBox).WidthOverride(10.0f).HeightOverride(10.0f)
			]
		];
	}

	TSharedRef<SWidget> MakeProviderDotForModel(const TSharedPtr<FACFAssistantModel>& Model, float Diameter)
	{
		if (!Model.IsValid()) { return StaticCastSharedRef<SWidget>(SNew(SSpacer)); }
		return MakeProviderDot(Model->Provider, Diameter);
	}

	// Dot then model name, for the dropdown rows and the closed combo.
	TSharedRef<SWidget> MakeModelLabel(const TSharedPtr<FACFAssistantModel>& Model, const FString& MissingLabel = FString())
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(SBox).WidthOverride(14).HeightOverride(14).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					MakeProviderDotForModel(Model, 14.0f)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[
				SNew(STextBlock).OverflowPolicy(ETextOverflowPolicy::Ellipsis).Text(FText::FromString(Model.IsValid() ? (Model->Label.IsEmpty() ? Model->Id : Model->Label) : (MissingLabel.IsEmpty() ? FString(TEXT("Choose a model")) : MissingLabel)))
			];
	}

	bool IsWorkRole(const FString& Role)
	{
		return Role == TEXT("Tool") || Role == TEXT("ToolOutput") || Role == TEXT("Reasoning");
	}

	FSlateFontInfo ChatLabelFont()
	{
		FSlateFontInfo Font = UACFAssistantSettings::Get().ResolvedChatFont();
		Font.TypefaceFontName = TEXT("Bold");
		Font.Size = FMath::Max(8, Font.Size - 2);
		return Font;
	}


	struct FACFAssistantWorkModeOption
	{
		EACFAssistantWorkMode Value = EACFAssistantWorkMode::Plan;
		FString Label;
	};

	// Grabs a bitmap from the Windows clipboard, writes a PNG into Directory, and returns an absolute path, or empty when the clipboard holds no image.
	FString TryPasteClipboardImage(const FString& Directory)
	{
#if PLATFORM_WINDOWS
		if (Directory.IsEmpty() || !::IsClipboardFormatAvailable(CF_DIB) || !::OpenClipboard(nullptr)) { return FString(); }
		FString SavedPath;
		if (HANDLE Data = ::GetClipboardData(CF_DIB))
		{
			if (const uint8* Raw = static_cast<const uint8*>(::GlobalLock(Data)))
			{
				const BITMAPINFOHEADER* Header = reinterpret_cast<const BITMAPINFOHEADER*>(Raw);
				const int32 Width = Header->biWidth;
				const int32 Height = FMath::Abs(Header->biHeight);
				const bool bBottomUp = Header->biHeight > 0;
				const int32 BitCount = Header->biBitCount;
				if (Width > 0 && Height > 0 && Width * Height <= 64 * 1024 * 1024 && (BitCount == 32 || BitCount == 24)
					&& (Header->biCompression == BI_RGB || Header->biCompression == BI_BITFIELDS))
				{
					const uint8* Bits = Raw + Header->biSize + (Header->biCompression == BI_BITFIELDS ? 12 : 0) + Header->biClrUsed * 4;
					const int32 Pitch = ((Width * BitCount + 31) / 32) * 4;
					TArray<uint8> Pixels;
					Pixels.SetNumUninitialized(Width * Height * 4);
					for (int32 Row = 0; Row < Height; ++Row)
					{
						const uint8* Source = Bits + (bBottomUp ? Height - 1 - Row : Row) * Pitch;
						uint8* Target = Pixels.GetData() + Row * Width * 4;
						for (int32 Column = 0; Column < Width; ++Column)
						{
							Target[Column * 4 + 0] = Source[Column * (BitCount / 8) + 0];
							Target[Column * 4 + 1] = Source[Column * (BitCount / 8) + 1];
							Target[Column * 4 + 2] = Source[Column * (BitCount / 8) + 2];
							Target[Column * 4 + 3] = 255;
						}
					}
					IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
					const TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
					if (Wrapper && Wrapper->SetRaw(Pixels.GetData(), Pixels.Num(), Width, Height, ERGBFormat::BGRA, 8))
					{
						const TArray64<uint8> Png = Wrapper->GetCompressed();
						IFileManager::Get().MakeDirectory(*Directory, true);
						const FString Path = Directory / FString::Printf(TEXT("pasted_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_%s")));
						if (FFileHelper::SaveArrayToFile(TArray<uint8>(Png.GetData(), Png.Num()), *Path))
						{
							SavedPath = FPaths::ConvertRelativePathToFull(Path);
							FPaths::MakePlatformFilename(SavedPath);
						}
					}
				}
				::GlobalUnlock(Data);
			}
		}
		::CloseClipboard();
		return SavedPath;
#else
		return FString();
#endif
	}

	bool TryReadPngSize(const TArray<uint8>& Bytes, int32& OutWidth, int32& OutHeight)
	{
		if (Bytes.Num() < 24 || Bytes[0] != 137 || Bytes[1] != TEXT('P') || Bytes[2] != TEXT('N') || Bytes[3] != TEXT('G')) { return false; }
		OutWidth = (static_cast<int32>(Bytes[16]) << 24) | (static_cast<int32>(Bytes[17]) << 16) | (static_cast<int32>(Bytes[18]) << 8) | Bytes[19];
		OutHeight = (static_cast<int32>(Bytes[20]) << 24) | (static_cast<int32>(Bytes[21]) << 16) | (static_cast<int32>(Bytes[22]) << 8) | Bytes[23];
		return OutWidth > 0 && OutHeight > 0;
	}

	FVector2D FitImageDisplaySize(const FString& FilePath, const float MaxEdge)
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *FilePath) && Bytes.Num() > 0)
		{
			IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
			const EImageFormat Format = Module.DetectImageFormat(Bytes.GetData(), Bytes.Num());
			const TSharedPtr<IImageWrapper> Wrapper = Format == EImageFormat::Invalid ? nullptr : Module.CreateImageWrapper(Format);
			if (Wrapper && Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
			{
				Width = Wrapper->GetWidth();
				Height = Wrapper->GetHeight();
			}
			if (Width <= 0 || Height <= 0) { TryReadPngSize(Bytes, Width, Height); }
		}
		if (Width <= 0 || Height <= 0) { return FVector2D(MaxEdge, MaxEdge * 0.56f); }
		const float Scale = FMath::Min(MaxEdge / static_cast<float>(Width), MaxEdge / static_cast<float>(Height));
		return FVector2D(FMath::Max(1.0f, Width * Scale), FMath::Max(1.0f, Height * Scale));
	}
}

class SACFAssistantConnectionWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFAssistantConnectionWindow) {}
		SLATE_ARGUMENT(TSharedPtr<FAscentACFAssistant>, Client)
	SLATE_END_ARGS()

	void Construct(const FArguments& Arguments)
	{
		Client = Arguments._Client;
		// Options come from the enums so new providers appear here automatically.
		if (const UEnum* ProviderEnum = StaticEnum<EACF3DProvider>())
		{
			for (int32 Index = 0; Index < ProviderEnum->NumEnums() - 1; ++Index)
			{
				ThreeDProviderOptions.Add(MakeShared<FString>(ProviderEnum->GetDisplayNameTextByIndex(Index).ToString()));
			}
		}
		ChildSlot
		[
			SNew(SBorder)
			.Padding(16)
			.BorderImage(CardBackground())
			.BorderBackgroundColor(Surface())
			[
				SNew(SVerticalBox)

				// Sign-in section: one row per provider. Both stay connected together; the model picked in the chat decides which one answers.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Sign-in")))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))
					.ColorAndOpacity(Accent())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					MakeAccountRow(EACFAssistantProvider::Yarin)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					MakeAccountRow(EACFAssistantProvider::Codex)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					MakeAccountRow(EACFAssistantProvider::ClaudeCode)
				]

				// Configuration section: everything that is not account related. Saved to the same values shown in Project Settings.
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
				[
					SNew(SBorder)
					.Padding(FMargin(0, 1))
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SNew(SBox).HeightOverride(1)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 8)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Configuration")))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))
					.ColorAndOpacity(Accent())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(SBox).WidthOverride(130)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Default content folder")))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1)
					[
						SNew(SEditableTextBox)
						.HintText(FText::FromString(TEXT("/Game/Yarin")))
						.ToolTipText(FText::FromString(TEXT("New assets the assistant creates are saved here unless your request names another folder.")))
						.Text_Lambda([] { return FText::FromString(UACFAssistantSettings::Get().DefaultContentFolder); })
						.OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type)
						{
							UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
							Settings.DefaultContentFolder = Text.ToString();
							Settings.SaveConfig();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(SBox).WidthOverride(130)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("3D provider")))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1)
					[
						SAssignNew(ThreeDProviderCombo, SComboBox<TSharedPtr<FString>>)
						.ButtonStyle(&FilledButtonStyle())
						.OptionsSource(&ThreeDProviderOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
						{
							return SNew(STextBlock).Text(FText::FromString(*Option));
						})
						.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type)
						{
							const int32 Index = Option ? ThreeDProviderOptions.IndexOfByKey(Option) : INDEX_NONE;
							if (Index == INDEX_NONE) { return; }
							UACFAIToolsSettings* Settings = GetMutableDefault<UACFAIToolsSettings>();
							Settings->ThreeDProvider = static_cast<EACF3DProvider>(Index);
							Settings->TryUpdateDefaultConfigFile();
						})
						[
							SNew(STextBlock).Text_Lambda([]
							{
								const UEnum* ProviderEnum = StaticEnum<EACF3DProvider>();
								return ProviderEnum ? ProviderEnum->GetDisplayNameTextByValue(static_cast<int64>(GetMutableDefault<UACFAIToolsSettings>()->ThreeDProvider)) : FText::GetEmpty();
							})
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(SBox).WidthOverride(130)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("3D API key")))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1)
					[
						SNew(SEditableTextBox)
						.IsPassword(true)
						.HintText(FText::FromString(TEXT("Key of the selected 3D provider; each provider keeps its own key")))
						.Text_Lambda([] { return FText::FromString(GetMutableDefault<UACFAIToolsSettings>()->GetActiveThreeDApiKey()); })
						.OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type)
						{
							UACFAIToolsSettings* Settings = GetMutableDefault<UACFAIToolsSettings>();
							Settings->SetActiveThreeDApiKey(Text.ToString());
							Settings->TryUpdateDefaultConfigFile();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
					.ColorAndOpacity(Muted())
					.Text(FText::FromString(
						TEXT("Chat images use your subscription on Codex, so no image key is needed. Manual image-provider settings remain under Yarin AI Tools.")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([] { return UACFAssistantSettings::Get().bShowWorkInChat ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
						Settings.bShowWorkInChat = State == ECheckBoxState::Checked;
						Settings.SaveConfig();
						if (Client.IsValid()) { Client->Changed.Broadcast(); }
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Show work in chat (collapsed behind an arrow; turn off to hide it)")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([] { return UACFAssistantSettings::Get().bAllowSourceFileEdits ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState State)
					{
						UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
						Settings.bAllowSourceFileEdits = State == ECheckBoxState::Checked;
						Settings.SaveConfig();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Allow source file edits (Claude Edit and Write, including C++). On by default; applies to the next message.")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([] { return UACFAssistantSettings::Get().bAllowShell ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState State)
					{
						UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
						Settings.bAllowShell = State == ECheckBoxState::Checked;
						Settings.SaveConfig();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Allow shell (Claude Bash and PowerShell). On by default; applies to the next message.")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([] { return UACFAssistantSettings::Get().bAllowWeb ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState State)
					{
						UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
						Settings.bAllowWeb = State == ECheckBoxState::Checked;
						Settings.SaveConfig();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Allow web (Claude WebSearch and WebFetch). On by default; applies to the next message.")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(&FilledButtonStyle())
						.Text(FText::FromString(TEXT("Open Project Settings...")))
						.ToolTipText(FText::FromString(TEXT("The same values are editable in Project Settings > Engine > Ascent Yarin.")))
						.OnClicked_Lambda([]
						{
							if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
							{
								Settings->ShowViewer(TEXT("Project"), TEXT("Engine"), TEXT("Ascent Yarin"));
							}
							return FReply::Handled();
						})
					]
				]
			]
		];
	}

private:
	TSharedPtr<FAscentACFAssistant> Client;
	TArray<TSharedPtr<FString>> ThreeDProviderOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ThreeDProviderCombo;

	// Name, live status and the one account action for one provider.
	TSharedRef<SWidget> MakeAccountRow(EACFAssistantProvider Provider)
	{
		using EState = FAscentACFAssistant::EConnectionState;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SBox).WidthOverride(18).HeightOverride(18).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					MakeProviderDot(Provider, 18.0f)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SBox).WidthOverride(120)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
					.ColorAndOpacity(Provider == EACFAssistantProvider::Yarin ? FSlateColor(FLinearColor::White) : ProviderDotBrush(Provider).TintColor)
					.Text(FText::FromString(Provider == EACFAssistantProvider::ClaudeCode ? TEXT("Claude Code") : Provider == EACFAssistantProvider::Yarin ? TEXT("Yarin gateway") : TEXT("ChatGPT (Codex)")))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([this, Provider]
				{
					if (!Client.IsValid()) { return FSlateColor(FLinearColor::White); }
					switch (Client->GetConnectionState(Provider))
					{
					case EState::Ready: return FSlateColor(FLinearColor(0.35f, 0.8f, 0.35f));
					case EState::Failed: return FSlateColor(FLinearColor(0.9f, 0.4f, 0.3f));
					case EState::Connecting:
					case EState::SigningIn: return FSlateColor(Accent());
					default: return Muted();
					}
				})
				.Text_Lambda([this, Provider]
				{
					return FText::FromString(Client.IsValid() ? Client->GetConnectionSummary(Provider) : FString());
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).MinDesiredWidth(150)
				[
					// The provider's one account action. Its label follows the state, so there is never a second button whose order matters.
					SNew(SButton)
					.ButtonStyle(&FilledButtonStyle())
					.HAlign(HAlign_Center)
					.Text_Lambda([this, Provider] { return AccountActionLabel(Provider); })
					.ToolTipText_Lambda([this, Provider] { return AccountActionTooltip(Provider); })
					.IsEnabled_Lambda([this, Provider]
					{
						return Client.IsValid() && FAscentACFAssistant::IsProviderAvailable(Provider) && Client->GetConnectionState(Provider) != EState::Connecting && !Client->IsAnyBusy(Provider);
					})
					.OnClicked_Lambda([this, Provider]
					{
						if (!Client.IsValid() || !FAscentACFAssistant::IsProviderAvailable(Provider)) { return FReply::Handled(); }
						switch (Client->GetConnectionState(Provider))
						{
						case EState::Ready: Client->SignOut(Provider); break;
						case EState::SigningIn: Client->CancelSignIn(Provider); break;
						default: Client->SignIn(Provider); break;
						}
						return FReply::Handled();
					})
				]
			];
	}

	FText AccountActionLabel(EACFAssistantProvider Provider) const
	{
		if (!FAscentACFAssistant::IsProviderAvailable(Provider)) { return FText::FromString(TEXT("Coming soon")); }
		if (!Client.IsValid()) { return FText::GetEmpty(); }
		switch (Client->GetConnectionState(Provider))
		{
		case FAscentACFAssistant::EConnectionState::Ready: return FText::FromString(TEXT("Sign out"));
		case FAscentACFAssistant::EConnectionState::Connecting: return FText::FromString(TEXT("Connecting..."));
		case FAscentACFAssistant::EConnectionState::SigningIn: return FText::FromString(TEXT("Cancel sign-in"));
		case FAscentACFAssistant::EConnectionState::Failed: return FText::FromString(TEXT("Try again"));
		default:
			return FText::FromString(TEXT("Sign in"));
		}
	}

	FText AccountActionTooltip(EACFAssistantProvider Provider) const
	{
		if (!FAscentACFAssistant::IsProviderAvailable(Provider)) { return FText::FromString(Client.IsValid() ? Client->GetConnectionSummary(Provider) : FString(TEXT("Coming soon."))); }
		if (!Client.IsValid()) { return FText::GetEmpty(); }
		const bool bClaude = Provider == EACFAssistantProvider::ClaudeCode;
		if (Provider == EACFAssistantProvider::Yarin)
		{
			switch (Client->GetConnectionState(Provider))
			{
			case FAscentACFAssistant::EConnectionState::Ready:
				return FText::FromString(TEXT("Signs Yarin out on this computer. ChatGPT and Claude stay connected. To revoke the 90-day session itself, remove this device from your Yarin account page."));
			case FAscentACFAssistant::EConnectionState::SigningIn:
				return FText::FromString(TEXT("Stop waiting for the browser sign-in."));
			case FAscentACFAssistant::EConnectionState::Failed:
				return FText::FromString(TEXT("Connect to Yarin again. Your sign-in is kept; only a rejected or expired one asks you to sign in again."));
			default:
				return FText::FromString(TEXT("Opens yarin.darktowerinteractive.com in your browser to approve this Editor with your Yarin account (Discord). Yarin models bill your Yarin credits, never ChatGPT or Claude."));
			}
		}
		switch (Client->GetConnectionState(Provider))
		{
		case FAscentACFAssistant::EConnectionState::Ready:
			return FText::FromString(bClaude
				? TEXT("Runs `claude auth logout`: Claude Code is signed out on this computer, including in your terminal. ChatGPT stays connected.")
				: TEXT("Signs Yarin's Codex out of ChatGPT. Codex used elsewhere on this computer, and Claude, stay connected."));
		case FAscentACFAssistant::EConnectionState::SigningIn:
			return FText::FromString(TEXT("Stop waiting for the browser sign-in."));
		case FAscentACFAssistant::EConnectionState::Failed:
			return FText::FromString(TEXT("Connect again and sign in if needed."));
		default:
			return FText::FromString(bClaude
				? TEXT("Uses Claude Code's existing sign-in on this computer if there is one: a Claude plan, a Console login, an API key or a cloud provider. Otherwise opens Claude's sign-in in the browser. For API billing, run `claude auth login --console` in a terminal instead.")
				: TEXT("Opens the ChatGPT sign-in in the browser, through Yarin's own Codex sign-in. Yarin continues on its own once you finish."));
		}
	}
};

class SACFAssistantPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFAssistantPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Arguments)
	{
		Client = GetSharedACFAssistant(true);
		RebuildConversationOptions();
		RebuildWorkModeOptions();
		Client->Changed.AddSP(this, &SACFAssistantPanel::Refresh);
		UACFAssistantSettings::ChatDisplayChanged.AddSP(this, &SACFAssistantPanel::RebuildForAppearance);

		ChildSlot
		[
			SNew(SBorder)
			.Padding(12)
			.BorderImage(CardBackground())
			.BorderBackgroundColor(Surface())
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.ButtonStyle(&AssistantButtonStyle())
						.ContentPadding(8)
						.ToolTipText_Lambda([this] { return FText::FromString(bSidebarOpen ? TEXT("Close sidebar") : TEXT("Open sidebar")); })
						.OnClicked_Lambda([this] { bSidebarOpen = !bSidebarOpen; return FReply::Handled(); })
						[
							SNew(SImage)
							.Image(SidebarIcon())
							.DesiredSizeOverride(FVector2D(20.0f, 20.0f))
							.ColorAndOpacity(Muted())
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
					[
						SNew(SBox).WidthOverride(142).HeightOverride(80)
						[
							SNew(SScaleBox).Stretch(EStretch::ScaleToFit).HAlign(HAlign_Left).VAlign(VAlign_Center)
							[
								SNew(SImage).Image(YarinLongImage())
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 16, 0)
					[
						SNew(SBorder)
						.Padding(FMargin(8, 5))
						.BorderImage(CardBackground(false))
						.BorderBackgroundColor(ConversationIdle())
						.ToolTipText_Lambda([this]
						{
							return FText::FromString(TEXT("ChatGPT: ") + Client->GetConnectionSummary(EACFAssistantProvider::Codex)
								+ TEXT("\nClaude Code: ") + Client->GetConnectionSummary(EACFAssistantProvider::ClaudeCode));
						})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
							SNew(SBox).WidthOverride(14).HeightOverride(14).HAlign(HAlign_Center).VAlign(VAlign_Center)
							.Visibility_Lambda([this] { return Client->IsProviderReady(EACFAssistantProvider::Codex) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							[
								MakeProviderDot(EACFAssistantProvider::Codex, 14.0f)
							]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(SBox).WidthOverride(14).HeightOverride(14).HAlign(HAlign_Center).VAlign(VAlign_Center)
								.Visibility_Lambda([this] { return Client->IsProviderReady(EACFAssistantProvider::ClaudeCode) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								[
									MakeProviderDot(EACFAssistantProvider::ClaudeCode, 14.0f)
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 5, 0)
							[
								SNew(SImage)
								.Image_Lambda([this] { return FAppStyle::GetBrush(Client->IsReady() ? "Icons.Link" : "Icons.Unlink"); })
								.DesiredSizeOverride(FVector2D(12, 12))
								.ColorAndOpacity_Lambda([this] { return Client->IsReady() ? FSlateColor(FLinearColor(0.35f, 0.8f, 0.35f)) : Muted(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
								.ColorAndOpacity(Accent())
								.Text_Lambda([this]
								{
									using EState = FAscentACFAssistant::EConnectionState;
									if (Client->IsReady()) { return FText::FromString(TEXT("Connected")); }
									bool bSigningIn = false;
									bool bConnecting = false;
									for (const EACFAssistantProvider Each : {EACFAssistantProvider::Codex, EACFAssistantProvider::ClaudeCode})
									{
										bSigningIn |= Client->GetConnectionState(Each) == EState::SigningIn;
										bConnecting |= Client->GetConnectionState(Each) == EState::Connecting;
									}
									if (bSigningIn) { return FText::FromString(TEXT("Signing in")); }
									if (bConnecting) { return FText::FromString(TEXT("Connecting")); }
									return FText::FromString(TEXT("Sign in"));
								})
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
						SNew(SSpacer)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(30).HeightOverride(26).VAlign(VAlign_Center).HAlign(HAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(&AssistantButtonStyle())
							.ContentPadding(FMargin(2))
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.ToolTipText(FText::FromString(TEXT("Phone access: open or close the tunnel and show its QR code.")))
							.OnClicked_Lambda([] { ShowACFAssistantPhoneWindow(); return FReply::Handled(); })
							[
								SNew(SBox).WidthOverride(18).HeightOverride(18)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("PlayWorld.PlayInMobilePreview"))
									.DesiredSizeOverride(FVector2D(18.0f, 18.0f))
									.ColorAndOpacity(Accent())
								]
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(30).HeightOverride(26).VAlign(VAlign_Center).HAlign(HAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(&AssistantButtonStyle())
							.ContentPadding(FMargin(2))
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.ToolTipText(FText::FromString(TEXT("Settings: connection, sign-in, and chat options")))
							.OnClicked_Lambda([this] { OpenConnectionWindow(); return FReply::Handled(); })
							[
								SNew(SBox).WidthOverride(18).HeightOverride(18)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Settings"))
									.DesiredSizeOverride(FVector2D(18.0f, 18.0f))
									.ColorAndOpacity(Accent())
								]
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SBox).HeightOverride(1)
					[
						SNew(SImage)
						.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.ColorAndOpacity(Accent())
						.Visibility(EVisibility::HitTestInvisible)
					]
				]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(220)
						.Visibility_Lambda([this] { return bSidebarOpen ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							SNew(SBorder)
							.Padding(8)
							.BorderImage(CardBackground())
							.BorderBackgroundColor(ConversationIdle())
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
								[
									SNew(SButton)
									.ButtonStyle(&PrimaryButtonStyle())
									.HAlign(HAlign_Center)
									.ContentPadding(FMargin(10, 9))
									.ToolTipText(FText::FromString(TEXT("Start a new conversation. Existing conversations keep running.")))
									.OnClicked_Lambda([this] { Client->NewConversation(); return FReply::Handled(); })
									[
										SNew(STextBlock)
										.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
										.ColorAndOpacity(FLinearColor::FromSRGBColor(FColor(0x08, 0x08, 0x0A)))
										.Text(FText::FromString(TEXT("+  New conversation")))
									]
								]
								+ SVerticalBox::Slot().FillHeight(1)
								[
									SNew(SScrollBox)
									+ SScrollBox::Slot()
									[
										SAssignNew(ConversationRows, SVerticalBox)
									]
								]
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					.Padding(TAttribute<FMargin>::CreateLambda([this] { return bSidebarOpen ? FMargin(0, 0, 10, 0) : FMargin(0); }))
					[
						SNew(SBox).WidthOverride(1)
						.Visibility_Lambda([this] { return bSidebarOpen ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SNew(SImage)
							.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.ColorAndOpacity(Accent())
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 0, 4, 12)
						[
							SNew(SVerticalBox)
							.Visibility_Lambda([this] { return IsNewConversation() ? EVisibility::Collapsed : EVisibility::Visible; })
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
								.ColorAndOpacity(Accent())
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Text_Lambda([this]
								{
									const auto Active = Client->GetActiveSession();
									return FText::FromString(Active && (!Active->ThreadId.IsEmpty() || !Active->Messages.IsEmpty())
										? Active->GetTitle() : TEXT("New conversation"));
								})
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
								.ColorAndOpacity(Muted())
								.Text(FText::FromString(FString::Printf(TEXT("Project: %s"), FApp::GetProjectName())))
							]
						]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					SNew(SBorder)
					.Padding(8)
					.BorderImage(CardBackground())
					.BorderBackgroundColor(SurfaceRaised())
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SAssignNew(ChatScroll, SScrollBox)
							.OnUserScrolled_Lambda([this](float Offset)
							{
								bFollowLatest = ChatScroll.IsValid() && Offset >= ChatScroll->GetScrollOffsetOfEnd() - 4.f;
							})
							+ SScrollBox::Slot()[SAssignNew(ChatRows, SVerticalBox)]
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24)
						[
							SNew(SVerticalBox)
							.Visibility_Lambda([this] { return IsNewConversation() ? EVisibility::Visible : EVisibility::Collapsed; })
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("What would you like to build?")))
								.Justification(ETextJustify::Center)
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))
								.ColorAndOpacity(Accent())
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
							[
								SNew(STextBlock)
								.AutoWrapText(true)
								.Justification(ETextJustify::Center)
								.Font(UACFAssistantSettings::Get().ResolvedChatFont())
								.ColorAndOpacity(Muted())
								.Text_Lambda([this]
								{
									return FText::FromString(Client->IsReady()
										? TEXT("Ask a question, explore this project, or describe a change.")
										: TEXT("Connect ChatGPT or Claude in Settings to start a conversation."));
								})
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12, 0, 0)
							[
								SNew(SButton)
								.ButtonStyle(&FilledButtonStyle())
								.Text(FText::FromString(TEXT("Open Settings")))
								.Visibility_Lambda([this] { return Client->IsReady() ? EVisibility::Collapsed : EVisibility::Visible; })
								.OnClicked_Lambda([this] { OpenConnectionWindow(); return FReply::Handled(); })
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4, 6, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(SBox).WidthOverride(14).HeightOverride(14)
						.Visibility_Lambda([this]
						{
							const auto Session = Client->GetActiveSession();
							return Session.IsValid() && Session->bBusy && Session->Approvals.IsEmpty() && !Session->UserInputRequestId.IsValid()
								? EVisibility::HitTestInvisible : EVisibility::Collapsed;
						})
						[
							SNew(SCircularThrobber)
							.Radius(6.f)
							.Period(0.9f)
							.ColorAndOpacity(Accent())
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
					[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 9))
					.ColorAndOpacity_Lambda([this] { return Client->IsBusy() ? Accent() : Muted(); })
					.Text_Lambda([this] { return FText::FromString(Client->GetStatus()); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
					[
						SNew(SButton)
						.ButtonStyle(&AssistantButtonStyle())
						.Text(FText::FromString(TEXT("Jump to latest")))
						.Visibility_Lambda([this] { return bFollowLatest ? EVisibility::Collapsed : EVisibility::Visible; })
						.OnClicked_Lambda([this]
						{
							bFollowLatest = true;
							ChatScroll->ScrollToEnd();
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					SNew(SBorder)
					.Padding(10)
					.BorderImage(CardBackground())
					.BorderBackgroundColor(ApprovalCard())
					.Visibility_Lambda([this]
					{
						return Client->IsWaitingForApproval() ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
							.Text_Lambda([this]
							{
								const TSharedPtr<FJsonObject> Preview = Client->GetApprovalPreview();
								FString Tool;
								FString Kind;
								double PendingCount = 0;
								if (Preview)
								{
									Preview->TryGetStringField(TEXT("tool"), Tool);
									Preview->TryGetStringField(TEXT("kind"), Kind);
									Preview->TryGetNumberField(TEXT("pending"), PendingCount);
								}
								FString Verb = TEXT("apply a change");
								if (Kind == TEXT("command")) { Verb = TEXT("run a command"); }
								else if (Kind == TEXT("file-change")) { Verb = TEXT("edit files"); }
								else if (Kind == TEXT("permissions")) { Verb = TEXT("use extra sandbox permissions"); }
								else if (Kind == TEXT("claude-tool")) { Verb = TEXT("use a tool"); }
								FString Text = FString::Printf(TEXT("Yarin wants to %s: %s"), *Verb, *Tool);
								if (PendingCount > 1) { Text += FString::Printf(TEXT("  (+%d more)"), static_cast<int32>(PendingCount) - 1); }
								return FText::FromString(Text);
							})
							.ToolTipText_Lambda([this]
							{
								const TSharedPtr<FJsonObject> Preview = Client->GetApprovalPreview();
								if (!Preview) { return FText::GetEmpty(); }
								FString Details;
								FJsonSerializer::Serialize(Preview.ToSharedRef(), TJsonWriterFactory<>::Create(&Details));
								return FText::FromString(Details.Left(4000));
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Approve")))
							.ToolTipText(FText::FromString(TEXT("Run this change.")))
							.OnClicked_Lambda([this] { Client->ResolveApproval(true); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Approve all")))
							.ToolTipText(FText::FromString(TEXT("Run this and every later change in this conversation without asking, until the work mode changes.")))
							.OnClicked_Lambda([this] { Client->ApproveConversation(); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton).Text(FText::FromString(TEXT("Deny")))
							.ToolTipText(FText::FromString(TEXT("Refuse this change. Yarin is told the user said no.")))
							.OnClicked_Lambda([this] { Client->ResolveApproval(false); return FReply::Handled(); })
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					SAssignNew(SuggestedRow, SVerticalBox)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
				[
					SNew(SBorder)
					.Padding(FMargin(12, 10))
					.BorderImage(ComposerBackground())
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2, 0, 2, 8)
							[
								SNew(SBox).MinDesiredHeight(40).MaxDesiredHeight(140).VAlign(VAlign_Center)
								[
									SAssignNew(InputBox, SMultiLineEditableTextBox)
									.Style(&ComposerTextStyle())
									.Font_Lambda([] { return UACFAssistantSettings::Get().ResolvedChatFont(); })
									.AutoWrapText(true)
									.BackgroundColor(FLinearColor::Transparent)
									.Padding(FMargin(0))
									.HintText_Lambda([this]
								{
									if (!Client->IsReady()) { return FText::FromString(TEXT("Sign in to message Yarin")); }
									return FText::FromString(Client->IsBusy() ? TEXT("Steer Yarin...") : TEXT("Message Yarin..."));
								})
								.IsEnabled_Lambda([this] { return Client->IsReady(); })
								.OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& Event)
								{
									if (Event.GetKey() == EKeys::Enter && !Event.IsShiftDown())
									{
										SendCurrent();
										return FReply::Handled();
									}
									if (Event.GetKey() == EKeys::V && Event.IsControlDown())
									{
										const FString Path = TryPasteClipboardImage(Client->GetPastedImagesDirectory());
										if (!Path.IsEmpty())
										{
											Client->AttachImage(Path);
											return FReply::Handled();
										}
									}
									return FReply::Unhandled();
								})
								]
							]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(AttachmentsRow, SWrapBox).UseAllottedSize(true)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SWrapBox)
							.UseAllottedSize(true)
							.InnerSlotPadding(FVector2D(0, 4))
							+ SWrapBox::Slot()
							[
								SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
							[
								SNew(SButton)
								.ButtonStyle(&AssistantButtonStyle())
								.ContentPadding(4)
								.ToolTipText(FText::FromString(TEXT("Attach images (or paste a screenshot with Ctrl+V)")))
								.IsEnabled_Lambda([this] { return Client->IsReady() && Client->GetPendingImages().Num() < 8; })
								.OnClicked_Lambda([this]
								{
									if (IDesktopPlatform* Desktop = FDesktopPlatformModule::Get())
									{
										TArray<FString> Files;
										if (Desktop->OpenFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
											TEXT("Attach images"), FString(), FString(), TEXT("Images|*.png;*.jpg;*.jpeg;*.webp"),
											EFileDialogFlags::Multiple, Files))
										{
											for (const FString& File : Files) { Client->AttachImage(File); }
										}
									}
									return FReply::Handled();
								})
								[
									SNew(SImage).Image(ComposerIcon(AttachImageBrush, TEXT("Resources/AttachImage.svg")))
									.DesiredSizeOverride(FVector2D(20, 20)).ColorAndOpacity(Muted())
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
							[
								SAssignNew(WorkModeCombo, SComboBox<TSharedPtr<FACFAssistantWorkModeOption>>)
								.ButtonStyle(&FilledButtonStyle())
								.ContentPadding(FMargin(8, 4))
								.OptionsSource(&WorkModeOptions)
								.IsEnabled_Lambda([this] { return !Client->IsBusy(); })
								.OnGenerateWidget_Lambda([](TSharedPtr<FACFAssistantWorkModeOption> Option)
								{
									return SNew(STextBlock).Text(FText::FromString(Option->Label));
								})
								.OnSelectionChanged_Lambda([this](TSharedPtr<FACFAssistantWorkModeOption> Option, ESelectInfo::Type SelectInfo)
								{
									if (Option && SelectInfo != ESelectInfo::Direct) { Client->SetWorkMode(Option->Value); }
								})
								[
									SNew(STextBlock).Text_Lambda([this]
									{
										return FText::FromString(FACFAssistantPermissions::Label(Client->GetWorkMode()));
									})
								]
							]
							]
							+ SWrapBox::Slot().FillEmptySpace(true).HAlign(HAlign_Right)
							[
								SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1)
							[
								SNew(SSpacer)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 4, 0).VAlign(VAlign_Center)
							[
								SAssignNew(ContextMeter, SACFContextMeter)
								.Fraction_Lambda([this] { return static_cast<float>(Client->GetContextUsageFraction()); })
								.ToolTipText_Lambda([this]
								{
									return FText::FromString(FString::Printf(TEXT("Context: %.0f%% used\n"), Client->GetContextUsageFraction() * 100.0)
										+ Client->GetContextUsageText());
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SAssignNew(ModelCombo, SComboBox<TSharedPtr<FACFAssistantModel>>)
								.ButtonStyle(&FilledButtonStyle())
								.ContentPadding(FMargin(8, 4))
								.ToolTipText_Lambda([this]
								{
									const auto Model = Client->FindModel(Client->GetModelId());
									return FText::FromString(Model ? (Model->Label.IsEmpty() ? Model->Id : Model->Label) : TEXT("Choose a model"));
								})
								.OptionsSource(&Client->Models)
								.IsEnabled_Lambda([this] { return Client->IsReady() && !Client->IsBusy(); })
								.OnGenerateWidget_Lambda([this](TSharedPtr<FACFAssistantModel> Model)
								{
									// In a started conversation the other provider's models cannot continue it; say that picking one opens a new conversation.
									const TSharedPtr<FACFAssistantSession> Active = Client->GetActiveSession();
									const bool bOpensNew = Model.IsValid() && Active.IsValid() && !Active->ThreadId.IsEmpty() && Model->Provider != Active->Provider;
									return SNew(SBox)
										.ToolTipText(FText::FromString(bOpensNew
											? FString::Printf(TEXT("Answered by %s. Starts a new %s conversation; this one stays in the sidebar."),
												FAscentACFAssistant::ProviderName(Model->Provider), FAscentACFAssistant::ProviderName(Model->Provider))
											: FString::Printf(TEXT("Answered by %s."), Model.IsValid() ? FAscentACFAssistant::ProviderName(Model->Provider) : TEXT(""))))
										[
											MakeModelLabel(Model)
										];
								})
								.OnSelectionChanged_Lambda([this](TSharedPtr<FACFAssistantModel> Model, ESelectInfo::Type SelectInfo)
								{
									if (Model && SelectInfo != ESelectInfo::Direct)
									{
										Client->SelectModel(Model->Id);
										SyncConversationCombos();
									}
								})
								[
									// Rebuilt by Refresh when the model or its label changes, so the logo follows the pick.
									SAssignNew(ModelComboContent, SBox).MinDesiredWidth(90).MaxDesiredWidth(150)
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 4, 0)
							[
								SAssignNew(EffortCombo, SComboBox<TSharedPtr<FString>>)
								.ButtonStyle(&FilledButtonStyle())
								.ContentPadding(FMargin(8, 4))
								.OptionsSource(&EffortOptions)
								.IsEnabled_Lambda([this] { return Client->IsReady() && EffortOptions.Num() > 1; })
								.ToolTipText_Lambda([this]
								{
									const auto Selected = EffortCombo.IsValid() ? EffortCombo->GetSelectedItem() : nullptr;
									return FText::FromString(TEXT("Reasoning: ") + EffortLabel(Selected ? *Selected : FString(), Client->GetModelId())
										+ TEXT("\nLower is faster; higher gives the model more time to think. Applies to the next message."));
								})
								.OnGenerateWidget_Lambda([this](TSharedPtr<FString> Level)
								{
									return SNew(STextBlock).Text(FText::FromString(EffortLabel(*Level, Client->GetModelId())));
								})
								.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Level, ESelectInfo::Type SelectInfo)
								{
									if (Level && SelectInfo != ESelectInfo::Direct) { Client->SetReasoningEffort(*Level); }
								})
								[
									SNew(STextBlock)
									.Text_Lambda([this]
									{
										const auto Selected = EffortCombo.IsValid() ? EffortCombo->GetSelectedItem() : nullptr;
										return FText::FromString(Selected && !Selected->IsEmpty()
											? EffortLabel(*Selected, Client->GetModelId()) : TEXT("Auto"));
									})
									.ColorAndOpacity(Muted())
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								// Send and Stop share one fixed-size slot: the row keeps its width when a turn starts, so the controls never wrap or jump.
								SNew(SBox).WidthOverride(34.f).HeightOverride(34.f)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SButton)
										.ButtonStyle(&AssistantButtonStyle())
										.ContentPadding(6)
										.ToolTipText(FText::FromString(TEXT("While Yarin works, sending interrupts the current step and redirects it with your message.")))
										.Visibility_Lambda([this] { return ShowsStop() ? EVisibility::Collapsed : EVisibility::Visible; })
										.IsEnabled_Lambda([this] { return Client->IsReady(); })
										.OnClicked_Lambda([this] { SendCurrent(); return FReply::Handled(); })
										[
											SNew(SImage).Image(ComposerIcon(SendBrush, TEXT("Resources/Send.svg")))
											.DesiredSizeOverride(FVector2D(22, 22)).ColorAndOpacity(Accent())
										]
									]
									+ SOverlay::Slot()
									[
										SNew(SButton)
										.ButtonStyle(&AssistantButtonStyle())
										.ContentPadding(6)
										.ToolTipText(FText::FromString(TEXT("Stop current work. Type a message to steer instead.")))
										.Visibility_Lambda([this] { return ShowsStop() ? EVisibility::Visible : EVisibility::Collapsed; })
										.OnClicked_Lambda([this] { Client->StopTurn(); return FReply::Handled(); })
										[
											SNew(SImage).Image(ComposerIcon(StopBrush, TEXT("Resources/Stop.svg")))
											.DesiredSizeOverride(FVector2D(22, 22)).ColorAndOpacity(Muted())
										]
									]
								]
							]
							]
						]
					]
				]
					]
				]
			]
		];

		RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SACFAssistantPanel::OnAutoConnect));
		Refresh();
	}

private:
	bool IsNewConversation() const
	{
		const auto Active = Client->GetActiveSession();
		return !Active.IsValid() || (Active->ThreadId.IsEmpty() && Active->Messages.IsEmpty());
	}

	// Stop takes Send's place while a turn runs and the box is empty; typing a message turns it back into Send, which steers the turn.
	bool ShowsStop() const
	{
		return Client->IsBusy() && (!InputBox.IsValid() || InputBox->GetText().IsEmpty());
	}

	bool bFollowLatest = true;
	TSharedPtr<FAscentACFAssistant> Client;
	TSharedPtr<SScrollBox> ChatScroll;
	TSharedPtr<SVerticalBox> ChatRows;
	TSharedPtr<SMultiLineEditableTextBox> InputBox;
	TSharedPtr<SComboBox<TSharedPtr<FACFAssistantModel>>> ModelCombo;
	TSharedPtr<SBox> ModelComboContent;
	TSharedPtr<SACFContextMeter> ContextMeter;
	float BuiltContextFraction = -1.f;
	TSharedPtr<SComboBox<TSharedPtr<FACFAssistantWorkModeOption>>> WorkModeCombo;
	// "" (the model's default) followed by the levels the conversation's model supports.
	TSharedPtr<SComboBox<TSharedPtr<FString>>> EffortCombo;
	TArray<TSharedPtr<FString>> EffortOptions;
	TArray<FString> BuiltEffortLevels;
	TSharedPtr<SWrapBox> AttachmentsRow;
	TSharedPtr<SVerticalBox> SuggestedRow;
	// Session id plus the still-unanswered questions. Refresh runs on every token, so the buttons rebuild only when this changes.
	FString BuiltSuggestions;
	TSharedPtr<SVerticalBox> ConversationRows;
	bool bSidebarOpen = true;
	TArray<int32> BuiltConversationIds;
	int32 BuiltSessionId = 0;
	// Incremental chat rendering state; see Refresh for why full rebuilds are avoided during streaming. Bubbles use selectable text (LastRowEditable), small work rows use plain text blocks.
	int32 BuiltMessageCount = -1;
	bool bBuiltShowWork = true;
	// Rows an appended message can still extend: the collapsible work group being filled, its step counter, and the last message bubble.
	TSharedPtr<SVerticalBox> OpenWorkLines;
	TSharedPtr<int32> OpenWorkSteps;
	TSharedPtr<SVerticalBox> OpenBubble;
	// Last values pushed into the combos and the attachment strip, so they are only rebuilt when they really change.
	int32 BuiltModelsRevision = -1;
	FString BuiltComboModelId;
	TArray<FString> BuiltAttachments;
	FString LastRowRole;
	TSharedPtr<STextBlock> LastRowText;
	TSharedPtr<ACFAssistantMarkdown::SSelectableBody> LastRowEditable;
	// One brush per previewed image file and display size, kept alive as long as the panel shows the conversation.
	TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> ChatImageBrushes;
	TMap<FString, FVector2D> ChatImageSizes;
	TSet<int32> ExpandedWorkGroups;

	FString ChatImageKey(const FString& FilePath, const float MaxEdge) const
	{
		return FilePath + TEXT("@") + LexToString(FMath::RoundToInt(MaxEdge));
	}

	FVector2D GetChatImageDisplaySize(const FString& FilePath, const float MaxEdge)
	{
		const FString Key = ChatImageKey(FilePath, MaxEdge);
		if (const FVector2D* Existing = ChatImageSizes.Find(Key)) { return *Existing; }
		const FVector2D Size = FitImageDisplaySize(FilePath, MaxEdge);
		ChatImageSizes.Add(Key, Size);
		return Size;
	}

	const FSlateBrush* GetChatImageBrush(const FString& FilePath, const float MaxEdge)
	{
		const FString Key = ChatImageKey(FilePath, MaxEdge);
		if (const TSharedPtr<FSlateDynamicImageBrush>* Existing = ChatImageBrushes.Find(Key)) { return Existing->Get(); }
		const FVector2D Size = GetChatImageDisplaySize(FilePath, MaxEdge);
		TSharedPtr<FSlateDynamicImageBrush> Brush = MakeShareable(new FSlateDynamicImageBrush(FName(*FilePath), Size));
		ChatImageBrushes.Add(Key, Brush);
		return Brush.Get();
	}

	TSharedRef<SWidget> MakeImagePreview(const FString& FilePath, const float MaxEdge)
	{
		if (!FPaths::FileExists(FilePath))
		{
			return SNew(STextBlock)
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
				.ColorAndOpacity(Muted())
				.Text(FText::FromString(TEXT("Image not found: ") + FPaths::GetCleanFilename(FilePath)));
		}
		const FVector2D Size = GetChatImageDisplaySize(FilePath, MaxEdge);
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y).HAlign(HAlign_Left)
				[
					SNew(SScaleBox).Stretch(EStretch::ScaleToFit).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SImage).Image(GetChatImageBrush(FilePath, MaxEdge))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
				.ColorAndOpacity(Muted())
				.Text(FText::FromString(FPaths::GetCleanFilename(FilePath)))
			];
	}
	TArray<TSharedPtr<FACFAssistantHistory>> ConversationOptions;
	TSet<FString> HistoryIds;
	TArray<TSharedPtr<FACFAssistantWorkModeOption>> WorkModeOptions;

	EActiveTimerReturnType OnAutoConnect(double, float)
	{
		Client->AutoConnect();
		return EActiveTimerReturnType::Stop;
	}

	bool RebuildConversationOptions()
	{
		if (Client->History == ConversationOptions) { return false; }
		ConversationOptions = Client->History;
		HistoryIds.Reset();
		for (const auto& Entry : ConversationOptions) { HistoryIds.Add(Entry->Id); }
		return true;
	}

	TSharedRef<SWidget> MakeConversationRow(TAttribute<FText> Title, TAttribute<bool> Selected, FOnClicked OnClicked)
	{
		return SNew(SBorder)
			.Padding(2)
			.BorderImage(CardBackground(false))
			.BorderBackgroundColor_Lambda([Selected]
			{
				return Selected.Get() ? ConversationIdle().GetSpecifiedColor() : FLinearColor::Transparent;
			})
			[
				SNew(SButton)
				.ButtonStyle(&AssistantButtonStyle())
				.ContentPadding(FMargin(8))
				.HAlign(HAlign_Left)
				.ToolTipText(Title)
				.OnClicked(OnClicked)
				[
					SNew(STextBlock)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text(Title)
					.ColorAndOpacity_Lambda([Selected] { return Selected.Get() ? FSlateColor(FLinearColor::White) : Muted(); })
				]
			];
	}

	void RebuildSidebar(bool bHistoryChanged)
	{
		if (!ConversationRows.IsValid()) { return; }
		TArray<int32> DraftIds;
		for (const auto& Session : Client->GetSessions())
		{
			if (Session->ThreadId.IsEmpty() || !HistoryIds.Contains(Session->ThreadId))
			{
				DraftIds.Add(Session->LocalId);
			}
		}
		if (DraftIds == BuiltConversationIds && !bHistoryChanged) { return; }
		BuiltConversationIds = MoveTemp(DraftIds);
		ConversationRows->ClearChildren();
		for (const auto& Session : Client->GetSessions())
		{
			if (!BuiltConversationIds.Contains(Session->LocalId)) { continue; }
			const int32 LocalId = Session->LocalId;
			const TWeakPtr<FACFAssistantSession> WeakSession = Session;
			ConversationRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
			[
				MakeConversationRow(
					TAttribute<FText>::CreateLambda([WeakSession]
					{
						const auto Conversation = WeakSession.Pin();
						return FText::FromString(Conversation ? Conversation->GetTitle() : FString());
					}),
					TAttribute<bool>::CreateLambda([this, LocalId] { return Client->GetActiveSessionId() == LocalId; }),
					FOnClicked::CreateLambda([this, LocalId] { Client->ActivateSession(LocalId); return FReply::Handled(); }))
			];
		}
		for (const auto& Entry : ConversationOptions)
		{
			ConversationRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
			[
				MakeConversationRow(
					TAttribute<FText>::CreateLambda([Entry] { return FText::FromString(Entry->Title); }),
					TAttribute<bool>::CreateLambda([this, Entry]
					{
						const auto Active = Client->GetActiveSession();
						return Active.IsValid() && Active->ThreadId == Entry->Id;
					}),
					FOnClicked::CreateLambda([this, Entry] { Client->Resume(Entry->Id); return FReply::Handled(); }))
			];
		}
	}

	void RebuildWorkModeOptions()
	{
		WorkModeOptions = {
			MakeShared<FACFAssistantWorkModeOption>(FACFAssistantWorkModeOption{EACFAssistantWorkMode::Ask, TEXT("Ask")}),
			MakeShared<FACFAssistantWorkModeOption>(FACFAssistantWorkModeOption{EACFAssistantWorkMode::Plan, TEXT("Plan")}),
			MakeShared<FACFAssistantWorkModeOption>(FACFAssistantWorkModeOption{EACFAssistantWorkMode::AskForApproval, TEXT("Ask for approval")}),
			MakeShared<FACFAssistantWorkModeOption>(FACFAssistantWorkModeOption{EACFAssistantWorkMode::ApproveForMe, TEXT("Approve for me")})
		};
	}

	void OpenConnectionWindow()
	{
		if (SettingsWindow.IsValid())
		{
			SettingsWindow->BringToFront();
			return;
		}
		SettingsWindow = SNew(SWindow)
			.Title(FText::FromString(TEXT("Yarin Settings")))
			.ClientSize(FVector2D(580.0f, 820.0f))
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			[
				SNew(SACFAssistantConnectionWindow).Client(Client)
			];
		SettingsWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([](const TSharedRef<SWindow>&)
		{
			SettingsWindow.Reset();
		}));
		FSlateApplication::Get().AddWindow(SettingsWindow.ToSharedRef());
	}

	void SendCurrent()
	{
		if (!InputBox.IsValid()) { return; }
		const FString Text = InputBox->GetText().ToString();
		bFollowLatest = true;
		InputBox->SetText(FText::GetEmpty());
		Client->Send(Text);
	}

	void RebuildSuggestedReplies()
	{
		if (!SuggestedRow.IsValid()) { return; }
		FString Signature = LexToString(Client->GetActiveSessionId()) + TEXT("|");
		for (const FACFAssistantQuestion& Question : Client->GetSuggestedQuestions())
		{
			if (Client->IsSuggestedAnswered(Question.Id)) { continue; }
			Signature += Question.Id + TEXT(":") + Question.Question + TEXT(";");
			for (const FACFAssistantChoice& Choice : Question.Options) { Signature += Choice.Label + TEXT(","); }
		}
		if (Signature == BuiltSuggestions) { return; }
		BuiltSuggestions = Signature;
		SuggestedRow->ClearChildren();
		for (const FACFAssistantQuestion& Question : Client->GetSuggestedQuestions())
		{
			if (Client->IsSuggestedAnswered(Question.Id)) { continue; }
			SuggestedRow->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10))
				.ColorAndOpacity(Accent())
				.Text(FText::FromString(Question.Question))
			];
			TSharedRef<SWrapBox> Choices = SNew(SWrapBox).UseAllottedSize(true);
			for (const FACFAssistantChoice& Choice : Question.Options)
			{
				const FString QuestionId = Question.Id;
				const FString Label = Choice.Label;
				Choices->AddSlot().Padding(0, 0, 6, 6)
				[
					SNew(SButton)
					.Text(FText::FromString(Label))
					.ToolTipText(FText::FromString(Choice.Description.IsEmpty() ? Label : Choice.Description))
					.OnClicked_Lambda([this, QuestionId, Label]
					{
						Client->AnswerSuggestedReply(QuestionId, Label);
						return FReply::Handled();
					})
				];
			}
			if (Question.Options.Num() > 0)
			{
				SuggestedRow->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[Choices];
			}
		}
		if (ChatScroll.IsValid() && Client->HasSuggestedReplies()) { ChatScroll->ScrollToEnd(); }
	}

	FString EffortLabel(const FString& Level, const FString& InModelId) const
	{
		if (Level.IsEmpty())
		{
			const TSharedPtr<FACFAssistantModel> Model = Client->FindModel(InModelId);
			return Model.IsValid() && !Model->Effort.IsEmpty() ? FString::Printf(TEXT("Default (%s)"), *EffortLabel(Model->Effort, InModelId)) : TEXT("Default");
		}
		if (Level == TEXT("xhigh")) { return TEXT("Extra high"); }
		return Level.Left(1).ToUpper() + Level.Mid(1);
	}

	void RebuildForAppearance()
	{
		BuiltMessageCount = -1;
		Refresh();
	}

	// Model and work mode belong to the conversation on screen. SComboBox only reports a pick that differs from its own selection,
	// so without this a tab switch left the old tab's choice selected and picking it again in the new tab did nothing.
	void SyncConversationCombos()
	{
		if (ModelCombo.IsValid())
		{
			const FString Active = Client->GetModelId();
			const TSharedPtr<FACFAssistantModel> Selected = ModelCombo->GetSelectedItem();
			if (!Selected.IsValid() || Selected->Id != Active)
			{
				const TSharedPtr<FACFAssistantModel>* Match = Client->Models.FindByPredicate([&Active](const TSharedPtr<FACFAssistantModel>& Model) { return Model->Id == Active; });
				if (Match) { ModelCombo->SetSelectedItem(*Match); }
				else if (Selected.IsValid()) { ModelCombo->ClearSelection(); }
			}
		}
		if (EffortCombo.IsValid())
		{
			const TSharedPtr<FACFAssistantModel> Model = Client->FindModel(Client->GetModelId());
			const TArray<FString> Levels = Model.IsValid() ? Model->Efforts : TArray<FString>();
			if (Levels != BuiltEffortLevels || EffortOptions.IsEmpty())
			{
				BuiltEffortLevels = Levels;
				EffortOptions.Reset();
				EffortOptions.Add(MakeShared<FString>());
				for (const FString& Level : Levels) { EffortOptions.Add(MakeShared<FString>(Level)); }
				EffortCombo->RefreshOptions();
			}
			// A level the model does not support runs at the model's default, so the combo shows Default for it.
			const FString Wanted = Levels.Contains(Client->GetReasoningEffort()) ? Client->GetReasoningEffort() : FString();
			const TSharedPtr<FString> Selected = EffortCombo->GetSelectedItem();
			if (!Selected.IsValid() || *Selected != Wanted)
			{
				const TSharedPtr<FString>* Match = EffortOptions.FindByPredicate([&Wanted](const TSharedPtr<FString>& Option) { return *Option == Wanted; });
				if (Match) { EffortCombo->SetSelectedItem(*Match); }
			}
		}
		if (WorkModeCombo.IsValid())
		{
			const EACFAssistantWorkMode Active = Client->GetWorkMode();
			const TSharedPtr<FACFAssistantWorkModeOption> Selected = WorkModeCombo->GetSelectedItem();
			if (!Selected.IsValid() || Selected->Value != Active)
			{
				const TSharedPtr<FACFAssistantWorkModeOption>* Match = WorkModeOptions.FindByPredicate([Active](const TSharedPtr<FACFAssistantWorkModeOption>& Option) { return Option->Value == Active; });
				if (Match) { WorkModeCombo->SetSelectedItem(*Match); }
			}
		}
	}

	void Refresh()
	{
		// Everything in this method is gated on something actually changing: Refresh runs on every streaming delta, and unconditional
		// widget churn here made the whole panel relayout dozens of times per second.
		const bool bHistoryChanged = RebuildConversationOptions();
		RebuildSidebar(bHistoryChanged);
		const float ContextFraction = static_cast<float>(Client->GetContextUsageFraction());
		if (ContextMeter.IsValid() && ContextFraction != BuiltContextFraction)
		{
			BuiltContextFraction = ContextFraction;
			ContextMeter->Invalidate(EInvalidateWidgetReason::Paint);
		}
		if (ModelCombo.IsValid() && BuiltModelsRevision != Client->ModelsRevision)
		{
			ModelCombo->RefreshOptions();
		}
		// The closed combo shows logo + name of the conversation's model; rebuilt only when that model or any label changed.
		const FString ComboModelId = Client->GetModelId();
		if (ModelComboContent.IsValid() && (BuiltModelsRevision != Client->ModelsRevision || BuiltComboModelId != ComboModelId || BuiltSessionId != Client->GetActiveSessionId()))
		{
			BuiltComboModelId = ComboModelId;
			const TSharedPtr<FACFAssistantSession> Active = Client->GetActiveSession();
			const FString MissingLabel = !ComboModelId.IsEmpty() ? ComboModelId
				: Active.IsValid() ? FString(FAscentACFAssistant::ProviderName(Active->Provider)) : FString();
			ModelComboContent->SetContent(MakeModelLabel(Client->FindModel(ComboModelId), MissingLabel));
		}
		BuiltModelsRevision = Client->ModelsRevision;
		SyncConversationCombos();
		if (BuiltSessionId != Client->GetActiveSessionId())
		{
			BuiltSessionId = Client->GetActiveSessionId();
			BuiltMessageCount = -1;
			bFollowLatest = true;
			ExpandedWorkGroups.Reset();
		}
		RebuildSuggestedReplies();
		const TArray<FACFAssistantMessage>& Messages = Client->GetMessages();
		if (AttachmentsRow.IsValid() && BuiltAttachments != Client->GetPendingImages())
		{
			BuiltAttachments = Client->GetPendingImages();
			AttachmentsRow->ClearChildren();
			const TArray<FString>& Attachments = BuiltAttachments;
			for (int32 Index = 0; Index < Attachments.Num(); ++Index)
			{
				const FString& Path = Attachments[Index];
				const FVector2D Size = GetChatImageDisplaySize(Path, 32.0f);
				AttachmentsRow->AddSlot().Padding(0, 0, 6, 6)
				[
					SNew(SBorder)
					.Padding(FMargin(3))
					.BorderImage(CardBackground())
					.BorderBackgroundColor(Surface())
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)
							[
								SNew(SScaleBox).Stretch(EStretch::ScaleToFit).HAlign(HAlign_Center).VAlign(VAlign_Center)
								[
									SNew(SImage).Image(GetChatImageBrush(Path, 32.0f))
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
							.ColorAndOpacity(Muted())
							.Text(FText::FromString(FPaths::GetCleanFilename(Path)))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(&AssistantButtonStyle())
							.ToolTipText(FText::FromString(TEXT("Remove this image from the next message")))
							.OnClicked_Lambda([this, Index] { Client->RemoveImage(Index); return FReply::Handled(); })
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("X"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
							]
						]
					]
				];
			}
		}
		if (!ChatRows.IsValid()) { return; }
		const bool bShowWork = UACFAssistantSettings::Get().bShowWorkInChat;
		// Streaming touches the chat dozens of times per second and a new row arrives every few messages. Tearing every row down and
		// building it again each time made the whole panel flicker, so an unchanged transcript only retouches the last row's text, a
		// grown one only appends the rows that are new, and a full rebuild is kept for what genuinely reshapes the list: a different
		// conversation, the work filter, or a transcript that was cleared or replaced.
		if (bShowWork == bBuiltShowWork && BuiltMessageCount >= 0 && Messages.Num() == BuiltMessageCount)
		{
			if (!Messages.IsEmpty() && Messages.Last().Role == LastRowRole)
			{
				const FString& Text = Messages.Last().Text;
				if (LastRowEditable.IsValid()) { ACFAssistantMarkdown::SetBodyText(LastRowEditable.ToSharedRef(), Text); }
				else if (LastRowText.IsValid()) { LastRowText->SetText(FText::FromString(LastRowRole == TEXT("Tool") ? TEXT("> ") + Text : Text)); }
			}
			if (ChatScroll.IsValid() && bFollowLatest) { ChatScroll->ScrollToEnd(); }
			return;
		}
		if (bShowWork == bBuiltShowWork && BuiltMessageCount > 0 && Messages.Num() > BuiltMessageCount)
		{
			for (int32 Index = BuiltMessageCount; Index < Messages.Num(); ++Index) { AppendMessageRow(Messages[Index], Index, bShowWork); }
			BuiltMessageCount = Messages.Num();
			if (ChatScroll.IsValid() && bFollowLatest) { ChatScroll->ScrollToEnd(); }
			return;
		}
		BuiltMessageCount = Messages.Num();
		bBuiltShowWork = bShowWork;
		LastRowText.Reset();
		LastRowEditable.Reset();
		LastRowRole.Reset();
		OpenWorkLines.Reset();
		OpenWorkSteps.Reset();
		OpenBubble.Reset();
		ChatRows->ClearChildren();
		if (Messages.IsEmpty()) { ExpandedWorkGroups.Reset(); }
		for (int32 Index = 0; Index < Messages.Num(); ++Index) { AppendMessageRow(Messages[Index], Index, bShowWork); }
		if (ChatScroll.IsValid() && bFollowLatest) { ChatScroll->ScrollToEnd(); }
	}

	// Adds one message at the end of the transcript, extending the row it continues rather than starting a new one: consecutive work
	// steps share a single collapsible group, and an image that follows a message is shown inside that message's bubble.
	void AppendMessageRow(const FACFAssistantMessage& Message, int32 MessageIndex, bool bShowWork)
	{
		if (IsWorkRole(Message.Role))
		{
			if (!bShowWork) { return; }
			if (!OpenWorkLines.IsValid())
			{
				const int32 GroupStart = MessageIndex;
				OpenWorkSteps = MakeShared<int32>(0);
				OpenWorkLines = SNew(SVerticalBox);
				TSharedPtr<int32> Steps = OpenWorkSteps;
				ChatRows->AddSlot().AutoHeight().Padding(4, 0, 0, 4)
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(!ExpandedWorkGroups.Contains(GroupStart))
					.AllowAnimatedTransition(false)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.BorderBackgroundColor(FLinearColor(0, 0, 0, 0))
					.HeaderPadding(FMargin(0, 1))
					.Padding(FMargin(12, 2, 4, 2))
					.OnAreaExpansionChanged_Lambda([this, GroupStart](bool bExpanded)
					{
						if (bExpanded) { ExpandedWorkGroups.Add(GroupStart); }
						else { ExpandedWorkGroups.Remove(GroupStart); }
					})
					.HeaderContent()
					[
						SNew(STextBlock)
						.Font(UACFAssistantSettings::Get().ResolvedWorkFont())
						.ColorAndOpacity(WorkDim())
						// Bound to the counter because later steps join this same group instead of rebuilding it.
						.Text_Lambda([Steps]
						{
							const int32 Count = Steps.IsValid() ? *Steps : 0;
							return FText::FromString(FString::Printf(TEXT("Work - %d %s"), Count, Count == 1 ? TEXT("step") : TEXT("steps")));
						})
					]
					.BodyContent()
					[
						SNew(SBox).MaxDesiredHeight(220)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								OpenWorkLines.ToSharedRef()
							]
						]
					]
				];
			}
			FString Line = Message.Text;
			if (Message.Role == TEXT("Tool")) { Line = TEXT("> ") + Message.Text; }
			else if (Message.Role == TEXT("Reasoning") && Line.TrimStartAndEnd().IsEmpty()) { Line = TEXT("Thinking"); }
			TSharedPtr<STextBlock> WorkBody;
			OpenWorkLines->AddSlot().AutoHeight().Padding(0, 0, 0, 1)
			[
				SAssignNew(WorkBody, STextBlock)
				.AutoWrapText(true)
				.Font(UACFAssistantSettings::Get().ResolvedWorkFont())
				.ColorAndOpacity(WorkDim())
				.Text(FText::FromString(Line))
			];
			if (OpenWorkSteps.IsValid()) { ++(*OpenWorkSteps); }
			OpenBubble.Reset();
			LastRowText = WorkBody;
			LastRowEditable.Reset();
			LastRowRole = Message.Role;
			return;
		}
		// Anything that is not a work step closes the current group.
		OpenWorkLines.Reset();
		OpenWorkSteps.Reset();
		if (ACFAssistantMesh::IsMeshRole(Message.Role))
		{
			// A 3D generation: progress while it runs, then the mesh in a viewer. The card follows its own message, so the thread is never rebuilt for it.
			const TSharedRef<FACFMeshCardState> LastState = MakeShared<FACFMeshCardState>();
			ChatRows->AddSlot().AutoHeight().Padding(4, 0, 0, 8)
			[
				SNew(SACFAssistantMeshCard)
				.AccentColor(Accent())
				.MutedColor(Muted())
				.LabelFont(ChatLabelFont())
				.OnSizeChanged_Lambda([this]
				{
					// The card grows from a small progress line to the tall viewer after the thread last scrolled, so follow it once it has settled.
					if (bFollowLatest && ChatScroll.IsValid()) { ChatScroll->ScrollToEnd(); }
				})
				.OnGetState_Lambda([this, MessageIndex, LastState]() -> FACFMeshCardState
				{
					const TArray<FACFAssistantMessage>& Thread = Client->GetMessages();
					if (Thread.IsValidIndex(MessageIndex) && ACFAssistantMesh::IsMeshRole(Thread[MessageIndex].Role))
					{
						const FACFAssistantMessage& Card = Thread[MessageIndex];
						LastState->Kind = Card.Role == ACFAssistantMesh::RoleReady ? EACFMeshCardKind::Ready
							: Card.Role == ACFAssistantMesh::RoleFailed ? EACFMeshCardKind::Failed : EACFMeshCardKind::Pending;
						LastState->Text = Card.Text;
					}
					return *LastState;
				})
			];
			OpenBubble.Reset();
			LastRowText.Reset();
			LastRowEditable.Reset();
			LastRowRole = Message.Role;
			return;
		}
		if (Message.Role == TEXT("Image"))
		{
			if (OpenBubble.IsValid())
			{
				OpenBubble->AddSlot().AutoHeight().Padding(0, 8, 0, 0)[MakeImagePreview(Message.Text, 320.0f)];
			}
			else
			{
				ChatRows->AddSlot().AutoHeight().Padding(4, 0, 0, 8)[MakeImagePreview(Message.Text, 320.0f)];
			}
			LastRowText.Reset();
			LastRowEditable.Reset();
			LastRowRole = Message.Role;
			return;
		}
		OpenBubble.Reset();
		if (Message.Role == TEXT("System"))
		{
			TSharedPtr<STextBlock> Body;
			ChatRows->AddSlot().AutoHeight().Padding(4, 0, 0, 8)
			[
				SAssignNew(Body, STextBlock)
				.AutoWrapText(true)
				.Font(ChatLabelFont())
				.ColorAndOpacity(Accent())
				.Text(FText::FromString(Message.Text))
			];
			LastRowText = Body;
			LastRowEditable.Reset();
			LastRowRole = Message.Role;
			return;
		}
		const bool bUser = Message.Role == TEXT("You");
		TSharedPtr<ACFAssistantMarkdown::SSelectableBody> EditableBody;
		if (!Message.Text.IsEmpty()) { EditableBody = ACFAssistantMarkdown::CreateSelectableBody(Message.Text); }
		TSharedRef<SVerticalBox> Bubble = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(bUser ? TEXT("You") : TEXT("Yarin")))
				.Font(ChatLabelFont())
				.ColorAndOpacity(Accent())
			];
		if (EditableBody.IsValid())
		{
			Bubble->AddSlot().AutoHeight().Padding(0, 8, 0, 0)[EditableBody.ToSharedRef()];
		}
		ChatRows->AddSlot().AutoHeight().Padding(0, 0, 0, 16)
		[
			SNew(SBorder)
			.Padding(16)
			.BorderImage(CardBackground())
			.BorderBackgroundColor(bUser ? UserBubble() : AssistantBubble())
			[
				Bubble
			]
		];
		// Images that arrive right after this message belong inside its bubble.
		OpenBubble = Bubble;
		LastRowText.Reset();
		LastRowEditable = EditableBody;
		LastRowRole = Message.Role;
	}
};

TSharedRef<SWidget> CreateACFAssistantPanel()
{
	return SNew(SACFAssistantPanel);
}
