// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistWidgets/BAWelcomeScreen.h"

#include "BASettings_Meta.h"
#include "BlueprintAssistCommands.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistTypes.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "ISettingsEditorModule.h"
#include "ISinglePropertyView.h"
#include "BlueprintAssistMisc/BAMiscUtils.h"
#include "BlueprintAssistMisc/FBAScopedPropertySetter.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

void SBAWelcomeScreen::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

#if BA_UE_VERSION_OR_LATER(5, 0)
	FName ButtonStyle("FVerticalToolBar.ToggleButton");
#else
	FName ButtonStyle("Menu.ToggleButton");
#endif

	// Use the tool bar style for this check box
	auto IntroMenuEntry = SNew(SCheckBox)
		.Style(BA_STYLE_CLASS::Get(), ButtonStyle)
		.Padding(8.0f)
		.IsChecked_Lambda([&]()
		{
			return WidgetSwitcher->GetActiveWidgetIndex() == 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([&](ECheckBoxState State)
		{
			WidgetSwitcher->SetActiveWidgetIndex(0);
		})
		[
			SNew(STextBlock).Text(INVTEXT("Introduction"))
		];

	auto CustomizeMenuEntry = SNew(SCheckBox)
		.Style(BA_STYLE_CLASS::Get(), ButtonStyle)
		.Padding(8.0f)
		.IsChecked_Lambda([&]()
		{
			return WidgetSwitcher->GetActiveWidgetIndex() == 1 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([&](ECheckBoxState State)
		{
			WidgetSwitcher->SetActiveWidgetIndex(1);
		})
		[
			SNew(STextBlock).Text(INVTEXT("Customize"))
		];

	WidgetSwitcher = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot().Padding(24.0f)
		[
			MakeIntroPage()
		]
		+ SWidgetSwitcher::Slot().Padding(24.0f)
		[
			MakeCustomizePage()
		];

	FSinglePropertyParams ShowWelcomeScreenParams;
	ShowWelcomeScreenParams.NotifyHook = &SettingsPropertyHook;
	ShowWelcomeScreenParams.NamePlacement = EPropertyNamePlacement::Type::Inside;
	ShowWelcomeScreenParams.Font = BA_STYLE_CLASS::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont"));

	auto SideButtons =
		SNew(SBorder).BorderImage(FBAStyle::GetBrush("BlueprintAssist.PanelBorder")).Padding(24.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				IntroMenuEntry
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				CustomizeMenuEntry
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().HAlign(HAlign_Right)
				[
					EditModule.CreateSingleProperty(GetMutableDefault<UBASettings_EditorFeatures>(), GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bShowWelcomeScreenOnLaunch), ShowWelcomeScreenParams).ToSharedRef()
				]
			]
		];

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SideButtons
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			WidgetSwitcher.ToSharedRef()
		]
	];
}

TSharedRef<SDockTab> SBAWelcomeScreen::CreateWelcomeScreenTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	MajorTab->SetContent(SNew(SBAWelcomeScreen));
	return MajorTab;
}

TSharedRef<SWidget> SBAWelcomeScreen::MakeCommandWidget(TSharedPtr<FUICommandInfo> Command)
{
	FString Text = Command->GetLabel().ToString() + " " + FBAMiscUtils::GetInputChordName(Command->GetFirstValidChord().Get());
	return SNew(STextBlock).Text(FText::FromString(Text));
}

FText SBAWelcomeScreen::GetCommandText(TSharedPtr<FUICommandInfo> Command)
{
	return FText::FromString(FString::Printf(TEXT("<NormalText.Important>%s (%s)</>"),
		*Command->GetLabel().ToString(),
		*FBAMiscUtils::GetInputChordName(Command->GetFirstValidChord().Get())));
}

TSharedRef<SWidget> SBAWelcomeScreen::MakeIntroPage()
{
	auto OnLinkClicked = [](const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		if (const FString* Url = Metadata.Find(TEXT("href")))
		{
			FPlatformProcess::LaunchURL(**Url, nullptr, nullptr);
		}
	};

	const FText IntroText = INVTEXT(
		"<LargeText>Welcome to the Blueprint Assist plugin!</>"
		"\nTo get an overview of the plugin features, please start by looking at <a id=\"browser\" href=\"https://blueprintassist.github.io/features/command-list\" style=\"Hyperlink\">the examples in the wiki</> and "
		"<a id=\"browser\" href=\"https://blueprintassist.github.io/features/editor-features/#auto-enable-instance-editable\" style=\"Hyperlink\">an overview of new editor features</>"
		"\nYou can find a new toolbar icon when opening Blueprints or supported graphs, this will allow quick access to some useful settings and menus."
	);

	const FText FeaturesText = FText::FormatOrdered(INVTEXT(
			"<LargeText>Key features of the plugin</>"
			"\n\t- Navigate pins on a node with the <NormalText.Important>Arrow Keys</>"
			"\n\t- With nodes selected, press {0} to layout nodes"
			"\n\t- Bring up the node creation menu with {1}"
			"\n\t- Menu to open all tabs and settings in the editor with {2}"
			"\n\t- Menu to display all the hotkeys in the plugin and editor with {3}"
			"\n\t- Attempt to connect any unlinked pins on the selected node by proximity with {4}")
		, GetCommandText(FBACommands::Get().FormatNodes)
		, GetCommandText(FBACommands::Get().OpenContextMenu)
		, GetCommandText(FBACommands::Get().OpenWindow)
		, GetCommandText(FBACommands::Get().OpenBlueprintAssistHotkeySheet)
		, GetCommandText(FBACommands::Get().ConnectUnlinkedPins)
	);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			.Text(IntroText)
			.DecoratorStyleSet(&BA_STYLE_CLASS::Get())
			+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateLambda(OnLinkClicked))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSpacer).Size(FVector2D(0.0f, 24.0f))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			.Text(FeaturesText)
			.DecoratorStyleSet(&BA_STYLE_CLASS::Get())
			+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateLambda(OnLinkClicked))
		];
}

TSharedRef<SWidget> SBAWelcomeScreen::MakeCustomizePage()
{
	TMap<UObject*, TArray<FName>> FormattingProps;
	FormattingProps.Add(GetMutableDefault<UBASettings>(), {
			GET_MEMBER_NAME_CHECKED(UBASettings, bGloballyDisableAutoFormatting),
			GET_MEMBER_NAME_CHECKED(UBASettings, ParameterStyle)
	});

	TMap<UObject*, TArray<FName>> AppearanceProps;
	AppearanceProps.Add(GetMutableDefault<UBASettings_EditorFeatures>(), {
			GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bEnableInvisibleKnotNodes),
	});

	TMap<UObject*, TArray<FName>> MiscProps;
	MiscProps.Add(GetMutableDefault<UBASettings_EditorFeatures>(), {
			GET_MEMBER_NAME_CHECKED(UBASettings_EditorFeatures, bPlayLiveCompileSound),
	});

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
			.Text(INVTEXT("<LargeText>Formatting</>"))
			.DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			MakePropertiesList(FormattingProps)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Text(INVTEXT("<LargeText>Appearance</>")).DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			MakePropertiesList(AppearanceProps)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Text(INVTEXT("<LargeText>Misc</>")).DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			MakePropertiesList(MiscProps)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SRichTextBlock).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Text(INVTEXT("<LargeText>Settings</>")).DecoratorStyleSet(&BA_STYLE_CLASS::Get())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 0.0f, 0.0f)
			[
				MakeProperty(GetMutableDefault<UBASettings_Meta>(), GET_MEMBER_NAME_CHECKED(UBASettings_Meta, CustomSettingsIniPath))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f).HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("Create New"))
					.ToolTipText(INVTEXT("Create a new ini file with your current plugin settings"))
					.OnClicked_Static(&SBAWelcomeScreen::OnCreateNewCustomSettingsClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("Save Settings"))
					.ToolTipText(INVTEXT("Override the ini file with your current settings"))
					.IsEnabled_Static(&SBAWelcomeScreen::IsCustomSettingsPathValid)
					.OnClicked_Static(&SBAWelcomeScreen::OnSaveCustomSettingsClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(INVTEXT("Reload"))
					.ToolTipText(INVTEXT("Apply the ini file settings to the current project"))
					.IsEnabled_Static(&SBAWelcomeScreen::IsCustomSettingsPathValid)
					.OnClicked_Static(&SBAWelcomeScreen::OnReloadCustomSettingsClicked)
				]
			]
		];
}

TSharedRef<SWidget> SBAWelcomeScreen::MakePropertiesList(const TMap<UObject*, TArray<FName>>& Properties)
{
	TSharedRef<SVerticalBox> PropBox = SNew(SVerticalBox);
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (auto& Elem : Properties)
	{
		const auto& PropertyNames = Elem.Value;
		// create the widgets for setting properties we want to edit
		{
			FSinglePropertyParams Params;
			Params.NotifyHook = &SettingsPropertyHook;
			Params.NamePlacement = EPropertyNamePlacement::Type::Inside;

			for (auto& PropertyName : PropertyNames)
			{
				PropBox->AddSlot().Padding(8.0f, 4.0f).AttachWidget(EditModule.CreateSingleProperty(Elem.Key, PropertyName, Params).ToSharedRef());
			}
		}
	}

	return PropBox;
}

TSharedRef<SWidget> SBAWelcomeScreen::MakeProperty(UObject* Obj, FName PropName)
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FSinglePropertyParams Params;
	Params.NotifyHook = &SettingsPropertyHook;
	Params.NamePlacement = EPropertyNamePlacement::Type::Inside;

	return EditModule.CreateSingleProperty(Obj, PropName, Params).ToSharedRef();
}

FReply SBAWelcomeScreen::OnCreateNewCustomSettingsClicked()
{
	const FString Title = "Save Custom Settings INI File";
	const FString DefaultPath = FPaths::ProjectDir();
	const FString DefaultFile = "CustomBlueprintAssistSettings.ini";
	const FString FileTypes = "INI files (*.ini)|*.ini";

	TArray<FString> OutFileNames;
	const bool bSelectedPath = FDesktopPlatformModule::Get()->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		Title,
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_SAVE),
		DefaultFile,
		FileTypes,
		EFileDialogFlags::None,
		OutFileNames
	);

	if (bSelectedPath && OutFileNames.Num() > 0)
	{
		FString RelativePath = OutFileNames[0];
		const bool bCreatedFile = FFileHelper::SaveStringToFile(TEXT(""), *RelativePath);
		if (bCreatedFile)
		{
			FString FullPath = FPaths::ConvertRelativePathToFull(RelativePath);

			UE_LOG(LogBlueprintAssist, Log, TEXT("Made custom settings file at %s"), *FullPath);
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_SAVE, FPaths::GetPath(FullPath)); // Save path as default for next time.

			{
				FBAScopedPropertySetter(&UBASettings_Meta::GetMutable(), GET_MEMBER_NAME_CHECKED(UBASettings_Meta, CustomSettingsIniPath));
				UBASettings_Meta::GetMutable().CustomSettingsIniPath.FilePath = FullPath;
			}

			OnSaveCustomSettingsClicked();
		}
	}

	return FReply::Handled();
}

FReply SBAWelcomeScreen::OnSaveCustomSettingsClicked()
{
	const FString& Path = FConfigCacheIni::NormalizeConfigIniPath(UBASettings_Meta::Get().CustomSettingsIniPath.FilePath);
	if (!Path.IsEmpty())
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Saved settings to file: %s"), *Path);
		UBASettings::GetMutable().TryUpdateDefaultConfigFile(*Path);
		UBASettings_EditorFeatures::GetMutable().TryUpdateDefaultConfigFile(*Path);
		UBASettings_Advanced::GetMutable().TryUpdateDefaultConfigFile(*Path);
	}

	return FReply::Handled();
}

FReply SBAWelcomeScreen::OnReloadCustomSettingsClicked()
{
	const FString& Path = FConfigCacheIni::NormalizeConfigIniPath(UBASettings_Meta::Get().CustomSettingsIniPath.FilePath);
	if (FPaths::FileExists(Path))
	{
		UE_LOG(LogBlueprintAssist, Log, TEXT("Reloaded custom settings from file: %s"), *Path);
		UBASettingsBase::ReloadSettings(UBASettings::StaticClass());
		UBASettingsBase::ReloadSettings(UBASettings_EditorFeatures::StaticClass());
		UBASettingsBase::ReloadSettings(UBASettings_Advanced::StaticClass());

		UBASettings::GetMutable().ReloadConfig(nullptr, *Path);
		UBASettings_EditorFeatures::GetMutable().ReloadConfig(nullptr, *Path);
		UBASettings_Advanced::GetMutable().ReloadConfig(nullptr, *Path);
	}

	return FReply::Handled();
}

bool SBAWelcomeScreen::IsCustomSettingsPathValid()
{
	const FString& Path = UBASettings_Meta::Get().CustomSettingsIniPath.FilePath;
	return !Path.IsEmpty() && FPaths::FileExists(Path);
}
