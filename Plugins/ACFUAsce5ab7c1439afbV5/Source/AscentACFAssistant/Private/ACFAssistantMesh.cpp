// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantMesh.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object);
		return Object;
	}

	// The response is either one JSON object, or an event stream: the last data line that holds a JSON-RPC object wins.
	TSharedPtr<FJsonObject> ParseRpcBody(const FString& Body)
	{
		const FString Trimmed = Body.TrimStartAndEnd();
		if (Trimmed.StartsWith(TEXT("{")))
		{
			return ParseObject(Trimmed);
		}
		TArray<FString> Lines;
		Trimmed.ParseIntoArrayLines(Lines);
		for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
		{
			FString Line = Lines[Index].TrimStartAndEnd();
			if (!Line.RemoveFromStart(TEXT("data:"))) { continue; }
			if (const TSharedPtr<FJsonObject> Object = ParseObject(Line.TrimStartAndEnd()))
			{
				return Object;
			}
		}
		return nullptr;
	}

	// The text blocks of an MCP result, joined.
	FString ResultText(const TSharedPtr<FJsonObject>& Result)
	{
		FString Text;
		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (!Result->TryGetArrayField(TEXT("content"), Content) || !Content) { return Text; }
		for (const TSharedPtr<FJsonValue>& Block : *Content)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			FString Piece;
			if (Block.IsValid() && Block->TryGetObject(Object) && Object && (*Object)->TryGetStringField(TEXT("text"), Piece))
			{
				Text += Piece;
			}
		}
		return Text.TrimStartAndEnd();
	}
}

namespace ACFAssistantMesh
{
	bool IsMeshGenerationTool(const FString& ToolName)
	{
		return ToolName.Equals(TEXT("GenerateMeshFromText"), ESearchCase::IgnoreCase)
			|| ToolName.Equals(TEXT("GenerateMeshFromImage"), ESearchCase::IgnoreCase);
	}

	FToolResult ParseToolResponse(const FString& Body)
	{
		FToolResult Out;
		const TSharedPtr<FJsonObject> Rpc = ParseRpcBody(Body);
		if (!Rpc.IsValid())
		{
			Out.Error = TEXT("The Editor sent no answer for the 3D generation.");
			return Out;
		}
		const TSharedPtr<FJsonObject>* RpcError = nullptr;
		if (Rpc->TryGetObjectField(TEXT("error"), RpcError) && RpcError)
		{
			(*RpcError)->TryGetStringField(TEXT("message"), Out.Error);
			if (Out.Error.IsEmpty()) { Out.Error = TEXT("The Editor refused the 3D generation."); }
			return Out;
		}
		const TSharedPtr<FJsonObject>* Result = nullptr;
		if (!Rpc->TryGetObjectField(TEXT("result"), Result) || !Result)
		{
			Out.Error = TEXT("The Editor sent no result for the 3D generation.");
			return Out;
		}
		const FString Text = ResultText(*Result);
		bool bIsError = false;
		(*Result)->TryGetBoolField(TEXT("isError"), bIsError);
		if (bIsError)
		{
			Out.Error = Text.IsEmpty() ? TEXT("The 3D generation failed.") : Text;
			return Out;
		}
		// The tool answers with its own JSON: { "success": true, "asset_path": "..." } or { "success": false, "error": "..." }.
		// Epic's tool host hands that over wrapped as { "returnValue": "<that JSON as a string>" }, so look through the wrapper.
		FString Plain = Text;
		TSharedPtr<FJsonObject> Tool = Text.StartsWith(TEXT("{")) ? ParseObject(Text) : nullptr;
		if (Tool.IsValid() && Tool->HasField(TEXT("returnValue")))
		{
			const TSharedPtr<FJsonObject>* InnerObject = nullptr;
			FString Inner;
			if (Tool->TryGetObjectField(TEXT("returnValue"), InnerObject) && InnerObject)
			{
				Tool = *InnerObject;
			}
			else if (Tool->TryGetStringField(TEXT("returnValue"), Inner))
			{
				Plain = Inner.TrimStartAndEnd();
				Tool = Plain.StartsWith(TEXT("{")) ? ParseObject(Plain) : nullptr;
			}
		}
		if (!Tool.IsValid())
		{
			Out.Error = Plain.IsEmpty() ? TEXT("The 3D generation returned nothing.") : Plain;
			return Out;
		}
		bool bSuccess = true;
		Tool->TryGetBoolField(TEXT("success"), bSuccess);
		FString Path;
		Tool->TryGetStringField(TEXT("asset_path"), Path);
		if (!bSuccess || Path.IsEmpty())
		{
			Tool->TryGetStringField(TEXT("error"), Out.Error);
			if (Out.Error.IsEmpty()) { Out.Error = TEXT("The 3D generation returned no mesh."); }
			return Out;
		}
		Out.bOk = true;
		Out.AssetPath = Path;
		return Out;
	}

	FString DescribeForPhone(const FString& Role, const FString& Text)
	{
		if (Role == RolePending) { return TEXT("Generating a 3D mesh..."); }
		if (Role == RoleReady) { return FString::Printf(TEXT("3D mesh ready: %s. Open the Editor to view it in 3D."), *FPaths::GetBaseFilename(Text)); }
		return FString::Printf(TEXT("3D generation failed: %s"), *Text);
	}
}
