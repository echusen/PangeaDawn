// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentACFAssistant.h"
#include "ACFAssistantPanelFactory.h"
#include "ACFAssistantClientInternal.h"
#include "ACFAssistantPhoneAccess.h"
#include "ACFAssistantSettings.h"
#include "ACFAssistantToolGate.h"
#include "AscentAIToolsSettings.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAscentACFAssistant);

TSharedRef<SWidget> CreateACFAssistantPanel();

static FDelegateHandle PhoneAccessChangedHandle;
static FDelegateHandle EnginePreExitHandle;
static FTSTicker::FDelegateHandle StartupTunnelHandle;

static void StopACFAssistantOnEngineExit()
{
	StopACFAssistantPhoneAccess();
	ShutdownSharedACFAssistant();
}

void FAscentACFAssistantModule::StartupModule()
{
	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("AscentAIAgent"));
	UACFAIToolsSettings::SetGeneratedContentFolderResolver([]()
	{
		return UACFAssistantSettings::Get().ResolvedContentFolder();
	});
	GetACFAssistantPanelFactory().BindStatic(&CreateACFAssistantPanel);
	PhoneAccessChangedHandle = UACFAssistantSettings::PhoneAccessChanged.AddStatic(&SyncACFAssistantPhoneAccess);
	RegisterACFAssistantPhoneCommands();
	// Tunnels are also closed on Editor exit, before module shutdown ordering comes into play. Any tunnel a previous Editor
	// left behind (crash, kill) is closed at startup, so no stale public hostname survives a restart.
	EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddStatic(&StopACFAssistantOnEngineExit);
	CleanupACFAssistantPhoneTunnels();
	// A paired phone should not need a click on the PC either. Opt-in and Iroh only; delayed so the Editor finishes loading first.
	StartupTunnelHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
	{
		OpenACFAssistantPhoneTunnelAtStartup();
		return false;
	}), 5.0f);
	UE_LOG(LogAscentACFAssistant, Log, TEXT("AscentACFAssistant started (native Slate panel registered)"));
}

void FAscentACFAssistantModule::ShutdownModule()
{
	FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
	EnginePreExitHandle.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(StartupTunnelHandle);
	StartupTunnelHandle.Reset();
	UACFAssistantSettings::PhoneAccessChanged.Remove(PhoneAccessChangedHandle);
	PhoneAccessChangedHandle.Reset();
	UnregisterACFAssistantPhoneCommands();
	StopACFAssistantPhoneAccess();
	ShutdownSharedACFAssistant();
	ACFAssistantToolGate::Shutdown();
	UACFAIToolsSettings::SetGeneratedContentFolderResolver(nullptr);
	GetACFAssistantPanelFactory().Unbind();
	UE_LOG(LogAscentACFAssistant, Log, TEXT("AscentACFAssistant shut down"));
}

IMPLEMENT_MODULE(FAscentACFAssistantModule, AscentACFAssistant)
