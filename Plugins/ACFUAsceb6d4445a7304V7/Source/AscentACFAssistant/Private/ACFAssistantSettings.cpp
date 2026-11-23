// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantSettings.h"
#include "AscentAIAgentSettings.h"
#include "Misc/MessageDialog.h"
#include "Styling/CoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ACFAssistantSettings)

FSimpleMulticastDelegate UACFAssistantSettings::ChatDisplayChanged;
FSimpleMulticastDelegate UACFAssistantSettings::PhoneAccessChanged;

UACFAssistantSettings& UACFAssistantSettings::Get()
{
	return *GetMutableDefault<UACFAssistantSettings>();
}

UACFAssistantSettings::UACFAssistantSettings()
{
	ChatFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
	WorkFont = FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 7);
}

FString UACFAssistantSettings::ResolvedContentFolder() const
{
	FString Folder = DefaultContentFolder.TrimStartAndEnd();
	while (Folder.EndsWith(TEXT("/")))
	{
		Folder.LeftChopInline(1);
	}
	return Folder;
}

FSlateFontInfo UACFAssistantSettings::ResolvedChatFont() const
{
	FSlateFontInfo Font = ChatFont.Size > 0 ? ChatFont : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
	const int32 Size = ChatFontSize > 0 ? ChatFontSize : 10;
	Font.Size = FMath::Clamp(Size, 6, 48);
	return Font;
}

FSlateFontInfo UACFAssistantSettings::ResolvedWorkFont() const
{
	FSlateFontInfo Font = WorkFont.Size > 0 ? WorkFont : FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 7);
	const int32 Size = WorkFontSize > 0 ? WorkFontSize : 7;
	Font.Size = FMath::Clamp(Size, 6, 48);
	return Font;
}

void UACFAssistantSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName Name = PropertyChangedEvent.GetMemberPropertyName();
	if (Name == GET_MEMBER_NAME_CHECKED(UACFAssistantSettings, McpServerPort))
	{
		if (UACFAIAgentDeveloperSettings* Agent = UACFAIAgentDeveloperSettings::GetMutable())
		{
			Agent->McpServerPort = FMath::Clamp(McpServerPort, 1024, 65535);
#if WITH_EDITOR
			Agent->TryUpdateDefaultConfigFile();
#endif
		}
	}
	if (Name == GET_MEMBER_NAME_CHECKED(UACFAssistantSettings, bAllowPhoneAccessFromTheInternet)
		|| Name == GET_MEMBER_NAME_CHECKED(UACFAssistantSettings, PhoneTransport)
		|| Name == GET_MEMBER_NAME_CHECKED(UACFAssistantSettings, PhoneAccessPort))
	{
		if (Name == GET_MEMBER_NAME_CHECKED(UACFAssistantSettings, bAllowPhoneAccessFromTheInternet) && bAllowPhoneAccessFromTheInternet)
		{
			const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT(
				"Phone access opens a connection from the internet to this Editor while the tunnel is open: an encrypted Iroh connection (the default) or a public Cloudflare HTTPS link, as chosen under Phone Transport.\n\n"
				"Anyone with that link can send messages to Yarin and approve Editor changes, including edits to this project.\n\n"
				"With Iroh and Open Phone Tunnel At Editor Startup on (both are the default), the link reopens by itself every time this Editor starts. Turn that option off to start it only when you press Open tunnel in the phone window. The link stops on Close tunnel or when you quit the Editor.\n\n"
				"Enable phone access?")));
			if (Answer != EAppReturnType::Yes)
			{
				bAllowPhoneAccessFromTheInternet = false;
			}
		}
		PhoneAccessChanged.Broadcast();
	}
	SaveConfig();
	ChatDisplayChanged.Broadcast();
}
