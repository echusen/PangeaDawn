// Copyright Recursoft LLC. All Rights Reserved.

#include "SMUtilityLauncherModule.h"

#include "Blueprints/SMBlueprintFactory.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMEditorStyle.h"
#include "SMUnrealTypeDefs.h"
#include "Utilities/SMEditorAuthentication.h"

#include "Configuration/SMUtilityLauncherStyle.h"
#include "License/SSMLicenseDialog.h"
#include "SMUtilityLauncherCommands.h"
#include "Support/SMSupportUtils.h"
#include "Support/SSMSupportDialog.h"

#include "Blueprints/SMBlueprint.h"
#include "Misc/SMAuthenticator.h"
#include "SMNodeInstance.h"

#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IMainFrameModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelEditor.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "SMUtilityLauncherModule"

void FSMUtilityLauncherModule::StartupModule()
{
	MenuExtensibilityManager = MakeShared<FExtensibilityManager>();
	ToolBarExtensibilityManager = MakeShared<FExtensibilityManager>();

	FSMUtilityLauncherCommands::Register();
	FSMUtilityLauncherStyle::Initialize();
	
	PluginCommands = MakeShared<FUICommandList>();
	BindCommands();
	
	const USMEditorSettings* EditorSettings = GetDefault<USMEditorSettings>();
	// Always show the launcher toolbar if authentication failed.
	if (EditorSettings->bEnableUtilityLauncherToolbar
		|| EditorSettings->UserMarketplaceAuthenticationState == LD::EditorAuth::Failed)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		ToolbarExtender = MakeShared<FExtender>();
		ToolbarExtender->AddToolBarExtension("Content",
			EExtensionHook::After,
			PluginCommands,
			FToolBarExtensionDelegate::CreateRaw(this, &FSMUtilityLauncherModule::ExtendLevelEditorToolbar));

		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
}

void FSMUtilityLauncherModule::ShutdownModule()
{
	if (ToolbarExtender.IsValid())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}
	
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FSMUtilityLauncherCommands::Unregister();
	FSMUtilityLauncherStyle::Shutdown();
}

void FSMUtilityLauncherModule::BindCommands()
{
	const FSMUtilityLauncherCommands& Commands = FSMUtilityLauncherCommands::Get();
	PluginCommands->MapAction(Commands.OpenDocs, FExecuteAction::CreateStatic(&FSMUtilityLauncherModule::OpenDocs));
	PluginCommands->MapAction(Commands.OpenDiscord, FExecuteAction::CreateStatic(&FSMUtilityLauncherModule::OpenDiscord));
	PluginCommands->MapAction(Commands.ViewSystemInfo, FExecuteAction::CreateStatic(&FSMUtilityLauncherModule::ViewSystemInfo));
	PluginCommands->MapAction(Commands.CreateStateMachineClass, FExecuteAction::CreateStatic(&FSMUtilityLauncherModule::CreateNewStateMachineClass));
	PluginCommands->MapAction(Commands.CreateNodeClass, FExecuteAction::CreateStatic(&FSMUtilityLauncherModule::CreateNewNodeClass));
}

void FSMUtilityLauncherModule::ExtendLevelEditorToolbar(FToolBarBuilder& ToolbarBuilder)
{
	TAttribute<FSlateIcon> IconGetter =
		TAttribute<FSlateIcon>::Create(TAttribute<FSlateIcon>::FGetter::CreateRaw(this,
			&FSMUtilityLauncherModule::GetLevelToolbarIcon));
	
	ToolbarBuilder.AddComboButton(FUIAction(),
						FOnGetContent::CreateRaw(this, &FSMUtilityLauncherModule::GenerateMenuContent),
						LOCTEXT("LogicDriverToolbarMenu_Label", "Logic Driver"),
						LOCTEXT("LogicDriverToolbarMenu_Tooltip", "Logic Driver utilities."),
						IconGetter);
}

TSharedRef<SWidget> FSMUtilityLauncherModule::GenerateMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, PluginCommands, GetMenuExtensibilityManager()->GetAllExtenders());

	MenuBuilder.BeginSection(TEXT("LogicDriver"), LOCTEXT("LogicDriverHeading", "Logic Driver"));
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("License_Name", "License"),
			LOCTEXT("License_Tooltip", "View Logic Driver license information."), 
			GetLicenseIcon(),
			FExecuteAction::CreateStatic(&FSMUtilityLauncherModule::OpenLicenseInformation));
		
		const FSlateIcon SupportIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.VisitSupportWebSite");
		MenuBuilder.AddSubMenu(
			LOCTEXT("OpenLogicDriverSupportSubMenu", "Support"),
			LOCTEXT("OpenLogicDriverSupportSubMenu_ToolTip", "View support options for Logic Driver."),
			FNewMenuDelegate::CreateStatic(&FSMUtilityLauncherModule::MakeSupportMenu),
			false,
			SupportIcon);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("BlueprintClass", LOCTEXT("BlueprintClassHeading", "Blueprint Class"));
	{
		MenuBuilder.AddMenuEntry(FSMUtilityLauncherCommands::Get().CreateStateMachineClass, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FSMEditorStyle::Get()->GetStyleSetName(), TEXT("ClassIcon.SMInstance")));

		MenuBuilder.AddMenuEntry(FSMUtilityLauncherCommands::Get().CreateNodeClass, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FSMEditorStyle::Get()->GetStyleSetName(), TEXT("ClassIcon.SMNodeInstance")));

		// Open an existing Blueprint Class...
		const FSlateIcon OpenBPIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.OpenClassBlueprint");
		MenuBuilder.AddSubMenu(
			LOCTEXT("OpenLogicDriverBlueprintClassSubMenu", "Open Logic Driver Class"),
			LOCTEXT("OpenLogicDriverBlueprintClassSubMenu_ToolTip", "Open an existing LogicDriver Blueprint Class in this project."),
			FNewMenuDelegate::CreateStatic(&FSMUtilityLauncherModule::MakeOpenBlueprintClassMenu),
			false,
			OpenBPIcon);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection(TEXT("Tools"), LOCTEXT("LogicDriverToolsHeading", "Tools"));
	{
		// Add a null widget so the section is created and can be extended from other modules.
		MenuBuilder.AddWidget(SNullWidget::NullWidget, FText::GetEmpty());
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FSlateIcon FSMUtilityLauncherModule::GetLevelToolbarIcon() const
{
	LD::EditorAuth::LDAuthStatus AuthenticationStatus = LD::EditorAuth::GetUserMarketplaceAuthenticationStatus();
	if (AuthenticationStatus == LD::EditorAuth::Succeeded || FSMAuthenticator::Get().IsAuthenticated())
	{
		// Standard launcher icon.
		return FSlateIcon(FSMUtilityLauncherStyle::GetStyleSetName(), "SMUtilityLauncherIcon");
	}
	
	if (AuthenticationStatus == LD::EditorAuth::Never)
	{
		return FSlateIcon(FSMUtilityLauncherStyle::GetStyleSetName(), "SMUtilityLauncherWarningIcon");
	}
	
	return FSlateIcon(FSMUtilityLauncherStyle::GetStyleSetName(), "SMUtilityLauncherErrorIcon");
}

FSlateIcon FSMUtilityLauncherModule::GetLicenseIcon() const
{
	LD::EditorAuth::LDAuthStatus AuthenticationStatus = LD::EditorAuth::GetUserMarketplaceAuthenticationStatus();
	if (AuthenticationStatus == LD::EditorAuth::Succeeded || FSMAuthenticator::Get().IsAuthenticated())
	{
		// Standard license icon.
		return FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(),
					TEXT("MainFrame.VisitCommunitySnippets"));
	}
	
	if (AuthenticationStatus == LD::EditorAuth::Never)
	{
		return FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(),
						TEXT("Icons.WarningWithColor"));
	}
	
	return FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(),
						TEXT("Icons.ErrorWithColor"));
}

void FSMUtilityLauncherModule::MakeSupportMenu(FMenuBuilder& InMenu)
{
	InMenu.BeginSection(TEXT("Links"), LOCTEXT("LinksHeader", "Links"));
	{
		InMenu.AddMenuEntry(FSMUtilityLauncherCommands::Get().OpenDocs, NAME_None,
			TAttribute<FText>(),TAttribute<FText>(),
			FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), TEXT("MainFrame.DocumentationHome")));

		InMenu.AddMenuEntry(FSMUtilityLauncherCommands::Get().OpenDiscord, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FSMUtilityLauncherStyle::Get()->GetStyleSetName(), TEXT("DiscordIcon")));
	}
	InMenu.EndSection();

	InMenu.BeginSection(TEXT("Report"), LOCTEXT("ReportHeader", "Report"));
	{
		InMenu.AddMenuEntry(FSMUtilityLauncherCommands::Get().ViewSystemInfo, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FSMUnrealAppStyle::Get().GetStyleSetName(), TEXT("MainFrame.VisitCommunitySnippets")));
	}
	InMenu.EndSection();
}

void FSMUtilityLauncherModule::OpenDocs()
{
	const FString Url = TEXT("https://logicdriver.com/docs/");
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

void FSMUtilityLauncherModule::OpenDiscord()
{
	const FString Url = TEXT("https://logicdriver.com/discord/");
	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

void FSMUtilityLauncherModule::ViewSystemInfo()
{
	LD::EditorAuth::AuthenticateAndRecordResult(FSimpleDelegate::CreateLambda([&]()
	{
		if (GEditor && GEditor->IsTimerManagerValid())
		{
			// Add frame delay so the license validation window has a chance to close.
			GEditor->GetTimerManager()->SetTimerForNextTick([&]()
			{
				if (GEditor)
				{
					const IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
					const TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();
	
					const TSharedPtr<SSMSupportDialog> SupportDialog =
					SNew(SSMSupportDialog, LD::Support::GenerateSystemInfo());
					FSlateApplication::Get().AddModalWindow(SupportDialog.ToSharedRef(), ParentWindow);
				}
			});
		}
	}));
}

void FSMUtilityLauncherModule::CreateNewStateMachineClass()
{
	// Use the BlueprintFactory to allow the user to pick a parent class for the new Blueprint class
	USMBlueprintFactory* NewFactory = CastChecked<USMBlueprintFactory>(NewObject<UFactory>(GetTransientPackage(), USMBlueprintFactory::StaticClass()));
	NewFactory->AddToRoot();
	
	FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(NewFactory);
	if (NewFactory->ConfigureProperties())
	{
		// Now help the user pick a path and name for the new Blueprint
		if(USMBlueprint* Blueprint = NewFactory->CreateAssetWithSaveAsDialog())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(
				Blueprint,
				EToolkitMode::Standalone);
		}
	}

	NewFactory->RemoveFromRoot();
}

void FSMUtilityLauncherModule::CreateNewNodeClass()
{
	// Use the BlueprintFactory to allow the user to pick a parent class for the new Blueprint class
	USMNodeBlueprintFactory* NewFactory = Cast<USMNodeBlueprintFactory>(NewObject<UFactory>(GetTransientPackage(), USMNodeBlueprintFactory::StaticClass()));
	FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(NewFactory);
	if (NewFactory->ConfigureProperties())
	{
		UClass* SelectedClass = NewFactory->GetParentClass();
		
		// Now help the user pick a path and name for the new Blueprint
		if(UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(LOCTEXT("CreateNodeBlueprintClass_Title", "Create Node Blueprint Class"), SelectedClass, NewFactory->GetDefaultNewAssetName()))
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(
				Blueprint,
				EToolkitMode::Standalone);
		}
	}
}

void FSMUtilityLauncherModule::MakeOpenBlueprintClassMenu(FMenuBuilder& InMenu)
{
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	// Configure filter for asset picker
	FAssetPickerConfig Config;
	Config.Filter.ClassPaths.Append({
		USMBlueprint::StaticClass()->GetClassPathName(),
		USMNodeBlueprint::StaticClass()->GetClassPathName()
	});
	Config.InitialAssetViewType = EAssetViewType::List;
	Config.OnAssetSelected = FOnAssetSelected::CreateLambda([&](const FAssetData& AssetData)
		{
			if(UBlueprint* SelectedBP = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(SelectedBP);
			}
		});
	Config.bAllowDragging = false;
	// Allow saving user defined filters via View Options
	Config.SaveSettingsName = FString(TEXT("ToolbarOpenLogicDriverClass"));

	InMenu.BeginSection(TEXT("Browse"), LOCTEXT("BrowseHeader", "Browse"));
	
	const TSharedRef<SWidget> Widget = 
		SNew(SBox)
		.WidthOverride(300.f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(Config)
		];
	
	InMenu.AddWidget(Widget, FText::GetEmpty(), true);
	InMenu.EndSection();
}

void FSMUtilityLauncherModule::OpenLicenseInformation()
{
#if LOGICDRIVER_IS_MARKETPLACE_BUILD
	if (LD::EditorAuth::GetUserMarketplaceAuthenticationStatus() != LD::EditorAuth::Succeeded &&
		!LD::EditorAuth::AuthenticateAndRecordResultSilently())
	{
		return;
	}
#endif

	const IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	const TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();
	
	const TSharedPtr<SSMLicenseDialog> LicenseDialog =
	SNew(SSMLicenseDialog);
	FSlateApplication::Get().AddModalWindow(LicenseDialog.ToSharedRef(), ParentWindow);
}

IMPLEMENT_MODULE(FSMUtilityLauncherModule, SMUtilityLauncher)

#undef LOCTEXT_NAMESPACE
