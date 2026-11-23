// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantTools.h"
#include "ACFAssistantOwnPlugin.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

namespace ACFAssistantTools
{
	TSharedPtr<FJsonObject> Error(const FString& Message)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("error"), Message);
		Result->SetBoolField(TEXT("success"), false);
		return Result;
	}

	TSharedPtr<FJsonObject> Success(const FString& Message)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("message"), Message);
		Result->SetBoolField(TEXT("success"), true);
		return Result;
	}

	FString Encode(const TSharedPtr<FJsonObject>& Object)
	{
		FString Text;
		if (Object)
		{
			FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		}
		return Text;
	}

	FString Instructions()
	{
		FString Text;
		if (const TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner())
		{
			FFileHelper::LoadFileToString(Text, *(Plugin->GetBaseDir() / TEXT("Resources/ACFAssistantInstructions.txt")));
			// Only installs that ship the Ascent Combat Framework carry this file; the standalone does not, so it never claims ACF expertise.
			FString Expertise;
			if (FFileHelper::LoadFileToString(Expertise, *(Plugin->GetBaseDir() / TEXT("Resources/AscentExpertiseInstructions.txt"))) && !Expertise.TrimStartAndEnd().IsEmpty())
			{
				Text += TEXT("\n") + Expertise;
			}
		}
		if (Text.TrimStartAndEnd().IsEmpty())
		{
			Text = TEXT("You are Yarin, the in-Editor assistant for Unreal Engine. Use the tools supplied by the ue-editor MCP server.");
		}
		return Text.Left(30000);
	}
}
