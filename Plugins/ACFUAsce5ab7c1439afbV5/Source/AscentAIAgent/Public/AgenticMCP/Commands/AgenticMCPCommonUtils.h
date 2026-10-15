// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "EdGraph/EdGraphPin.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node_Event;

class AGENTICMCP_API FAgenticMCPCommonUtils
{
public:
	static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& InMessage);
	static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& InData = nullptr);

	static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& InJsonObject, const FString& InFieldName);
	static FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& InJsonObject, const FString& InFieldName);

	static UBlueprint* FindBlueprintByName(const FString& InBlueprintName);

	static UK2Node_Event* CreateEventNode(UEdGraph* InGraph, const FString& InEventName, const FVector2D& InPosition);
	static UEdGraphPin* FindPin(UEdGraphNode* InNode, const FString& InPinName, EEdGraphPinDirection InDirection = EGPD_MAX);

	static bool SetObjectProperty(UObject* InObject, const FString& InPropertyName,
		const TSharedPtr<FJsonValue>& InValue, FString& OutErrorMessage);

	static bool BuildPinTypeFromJson(const TSharedPtr<FJsonObject>& InSpec,
		FEdGraphPinType& OutPinType, FString& OutError);
};