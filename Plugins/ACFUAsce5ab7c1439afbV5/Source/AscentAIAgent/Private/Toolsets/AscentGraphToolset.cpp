// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentGraphToolset.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_StructMemberGet.h"
#include "K2Node_StructMemberSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Toolsets/AscentToolsetUtils.h"
#include "UObject/UnrealType.h"

namespace
{
/** Resolves a node object path and returns the member pin list it owns, if it is a struct node. */
UEdGraphNode* ResolveStructNode(const FString& NodePath, TArray<FOptionalPinFromProperty>*& OutPins,
    UScriptStruct*& OutStruct, FString& OutError)
{
    if (NodePath.IsEmpty()) {
        OutError = TEXT("NodePath is empty");
        return nullptr;
    }

    UEdGraphNode* Node = FindObject<UEdGraphNode>(nullptr, *NodePath);
    if (!Node) {
        OutError = FString::Printf(TEXT("No graph node at '%s'"), *NodePath);
        return nullptr;
    }

    if (UK2Node_StructMemberSet* SetNode = Cast<UK2Node_StructMemberSet>(Node)) {
        OutPins = &SetNode->ShowPinForProperties;
        OutStruct = SetNode->StructType;
    } else if (UK2Node_StructMemberGet* GetNode = Cast<UK2Node_StructMemberGet>(Node)) {
        OutPins = &GetNode->ShowPinForProperties;
        OutStruct = GetNode->StructType;
    } else {
        OutError = FString::Printf(
            TEXT("Node '%s' is a %s, not a Make / Break / Set-members struct node"),
            *NodePath, *Node->GetClass()->GetName());
        return nullptr;
    }

    return Node;
}
} // namespace

FString UAscentGraphToolset::ListStructMemberPins(const FString& NodePath)
{
    TArray<FOptionalPinFromProperty>* Pins = nullptr;
    UScriptStruct* Struct = nullptr;
    FString Error;
    if (!ResolveStructNode(NodePath, Pins, Struct, Error)) {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    TArray<TSharedPtr<FJsonValue>> Members;
    for (const FOptionalPinFromProperty& Pin : *Pins) {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Pin.PropertyName.ToString());
        Entry->SetBoolField(TEXT("shown"), Pin.bShowPin != 0);
        Entry->SetBoolField(TEXT("can_toggle"), Pin.bCanToggleVisibility != 0);
        Members.Add(MakeShared<FJsonValueObject>(Entry));
    }

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("struct"), Struct ? Struct->GetName() : TEXT(""));
    Data->SetArrayField(TEXT("members"), Members);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentGraphToolset::SetStructMemberPins(const FString& NodePath, const FString& MembersJson)
{
    TArray<FOptionalPinFromProperty>* Pins = nullptr;
    UScriptStruct* Struct = nullptr;
    FString Error;
    UEdGraphNode* Node = ResolveStructNode(NodePath, Pins, Struct, Error);
    if (!Node) {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    const TSharedPtr<FJsonObject> Requested = FAscentToolsetUtils::ParseObject(MembersJson);
    if (!Requested.IsValid()) {
        return FAscentToolsetUtils::MakeErrorResponse(
            TEXT("MembersJson must be a JSON object of member name -> bool"));
    }

    TArray<TSharedPtr<FJsonValue>> Changed;
    TArray<TSharedPtr<FJsonValue>> Unknown;
    TArray<TSharedPtr<FJsonValue>> Locked;

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Requested->Values) {
        bool bWanted = false;
        if (!Pair.Value.IsValid() || !Pair.Value->TryGetBool(bWanted)) {
            return FAscentToolsetUtils::MakeErrorResponse(
                FString::Printf(TEXT("Member '%s' must map to a bool"), *Pair.Key));
        }

        FOptionalPinFromProperty* Found = Pins->FindByPredicate(
            [&Pair](const FOptionalPinFromProperty& Pin) {
                return Pin.PropertyName.ToString().Equals(Pair.Key, ESearchCase::IgnoreCase);
            });

        if (!Found) {
            Unknown.Add(MakeShared<FJsonValueString>(Pair.Key));
            continue;
        }
        if (!Found->bCanToggleVisibility) {
            Locked.Add(MakeShared<FJsonValueString>(Pair.Key));
            continue;
        }
        if ((Found->bShowPin != 0) == bWanted) {
            continue;
        }

        Found->bShowPin = bWanted;
        Changed.Add(MakeShared<FJsonValueString>(Pair.Key));
    }

    if (Changed.Num() > 0) {
        Node->Modify();
        // Rebuilds the pin set from ShowPinForProperties; existing links on kept pins survive.
        Node->ReconstructNode();
        if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node)) {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }
    }

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("struct"), Struct ? Struct->GetName() : TEXT(""));
    Data->SetArrayField(TEXT("changed"), Changed);
    Data->SetArrayField(TEXT("unknown"), Unknown);
    Data->SetArrayField(TEXT("locked"), Locked);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}
