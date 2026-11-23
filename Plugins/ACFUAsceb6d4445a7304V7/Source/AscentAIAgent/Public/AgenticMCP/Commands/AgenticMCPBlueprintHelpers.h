// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

// Shared helpers for Blueprint authoring/inspection MCP commands.
// Originally lived as anonymous-namespace functions in
// AgenticMCPBlueprintCommands.cpp; promoted here so future sibling command
// classes can reuse them. The existing AgenticBpInspect / AgenticBpEdit
// namespaces inside BlueprintCommands.cpp continue to expose these names
// via `using` declarations so the 30+ handlers in that file don't need
// to be edited.

#include "CoreMinimal.h"
#include "Json.h"
#include "EdGraph/EdGraphPin.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UClass;
class UFunction;
struct FEdGraphPinType;

namespace AgenticMCPBlueprintHelpers
{
    /** Load a UBlueprint from a content path; populate OutError with an error JSON response on failure and return nullptr. */
    UBlueprint* LoadBlueprintOrError(const FString& Path, TSharedPtr<FJsonObject>& OutError);

    /** Convert FEdGraphPinType to a structured JSON object (richer than the compact pin-type shorthand). */
    TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& InType);

    /** Resolve a class string to a UClass*. Accepts:
     *  - short type name ("K2Node_CallFunction", "Pawn") via TryFindTypeSlow
     *  - native /Script path ("/Script/Engine.Pawn") via LoadClass
     *  - BP asset path ("/Game/Foo/BPI_Damageable") — pulls the generated class
     *    off UBlueprint, since UE stores it as <assetpath>.<assetname>_C. */
    UClass* ResolveClassByName(const FString& Spec);

    /** Resolve "/Script/Engine.GameplayStatics:GetPlayerCharacter" to a UFunction*. */
    UFunction* ResolveFunctionByPath(const FString& Spec, FString& OutErr);

    /** Find a graph by name across every Blueprint graph collection (Ubergraph/Function/Macro/Construction/etc). */
    UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName);

    /** Find a node by exact UE-assigned object name. */
    UEdGraphNode* FindNodeByExactName(UEdGraph* Graph, const FString& NodeName);

    /** Find a pin on a node by name + optional direction. Case-insensitive fallback. */
    UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName,
                                EEdGraphPinDirection Direction = EGPD_MAX);

    /** Brief JSON descriptor for a node — used in bulk add/inspect responses. */
    TSharedPtr<FJsonObject> NodeToBriefJson(UEdGraphNode* Node);

    /** Brief JSON descriptor for a pin — used in bulk add/inspect responses. */
    TSharedPtr<FJsonObject> PinToBriefJson(UEdGraphPin* Pin);

    /** Apply per-class construction (called BEFORE AllocateDefaultPins so pin layout is right).
     *  Handles K2Node_CallFunction, _VariableGet/Set, _DynamicCast, _CustomEvent, _Event,
     *  _MacroInstance, _MakeStruct, _BreakStruct. Returns empty string on success, error
     *  message otherwise. */
    FString ApplyNodeConfig(UEdGraphNode* Node, UBlueprint* Blueprint,
                             const TSharedPtr<FJsonObject>& Config);
}
