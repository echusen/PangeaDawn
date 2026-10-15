// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentAnimGraphToolset.h"

#include "Toolsets/AscentToolsetUtils.h"

namespace
{
    TSharedPtr<FJsonObject> MakeGraphParams(const FString& Path, const FString& Graph)
    {
        const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        Params->SetStringField(TEXT("blueprint_path"), Path);
        if (!Graph.IsEmpty())
        {
            Params->SetStringField(TEXT("graph_name"), Graph);
        }
        return Params;
    }

    /** Copies every field of an optional JSON object string into Params. Returns false on invalid JSON. */
    bool MergeOptions(const FString& OptionsJson, const TSharedPtr<FJsonObject>& Params)
    {
        if (OptionsJson.TrimStartAndEnd().IsEmpty())
        {
            return true;
        }
        const TSharedPtr<FJsonObject> Options = FAscentToolsetUtils::ParseObject(OptionsJson);
        if (!Options.IsValid())
        {
            return false;
        }
        for (const auto& Pair : Options->Values)
        {
            Params->SetField(Pair.Key, Pair.Value);
        }
        return true;
    }
}

FString UAscentAnimGraphToolset::Analyze(const FString& Path)
{
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_analyze"), MakeGraphParams(Path, FString()));
}

FString UAscentAnimGraphToolset::AddNode(const FString& Path, const FString& Graph, const FString& NodeType,
    int32 X, int32 Y, const FString& OptionsJson)
{
    const TSharedPtr<FJsonObject> Params = MakeGraphParams(Path, Graph);
    if (!MergeOptions(OptionsJson, Params))
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentAnimGraph] OptionsJson is not a valid JSON object"));
    }
    Params->SetStringField(TEXT("node_type"), NodeType);
    Params->SetNumberField(TEXT("x"), X);
    Params->SetNumberField(TEXT("y"), Y);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_add_node"), Params);
}

FString UAscentAnimGraphToolset::ConnectNodes(const FString& Path, const FString& Graph, const FString& SourceNode,
    const FString& SourcePin, const FString& TargetNode, const FString& TargetPin)
{
    const TSharedPtr<FJsonObject> Params = MakeGraphParams(Path, Graph);
    Params->SetStringField(TEXT("source_node"), SourceNode);
    Params->SetStringField(TEXT("target_node"), TargetNode);
    if (!SourcePin.IsEmpty())
    {
        Params->SetStringField(TEXT("source_pin"), SourcePin);
    }
    if (!TargetPin.IsEmpty())
    {
        Params->SetStringField(TEXT("target_pin"), TargetPin);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_connect_nodes"), Params);
}

FString UAscentAnimGraphToolset::SetNodeProperties(const FString& Path, const FString& Graph, const FString& Node,
    const FString& PropertiesJson)
{
    const TSharedPtr<FJsonObject> Properties = FAscentToolsetUtils::ParseObject(PropertiesJson);
    if (!Properties.IsValid())
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentAnimGraph] PropertiesJson is not a valid JSON object"));
    }

    const TSharedPtr<FJsonObject> Params = MakeGraphParams(Path, Graph);
    Params->SetStringField(TEXT("node_name"), Node);
    Params->SetObjectField(TEXT("properties"), Properties);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_set_node_properties"), Params);
}

FString UAscentAnimGraphToolset::RemoveNode(const FString& Path, const FString& Graph, const FString& Node)
{
    const TSharedPtr<FJsonObject> Params = MakeGraphParams(Path, Graph);
    Params->SetStringField(TEXT("node_name"), Node);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_remove_node"), Params);
}

FString UAscentAnimGraphToolset::EditPins(const FString& Path, const FString& Graph, const FString& Node,
    const FString& Action, const FString& PinName)
{
    const TSharedPtr<FJsonObject> Params = MakeGraphParams(Path, Graph);
    Params->SetStringField(TEXT("node_name"), Node);
    Params->SetStringField(TEXT("action"), Action);
    if (!PinName.IsEmpty())
    {
        Params->SetStringField(TEXT("pin_name"), PinName);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_pin"), Params);
}

FString UAscentAnimGraphToolset::BlendProfile(const FString& SkeletonPath, const FString& Action,
    const FString& ProfileName, const FString& Mode, const FString& BonesJson, bool bRecursive)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("skeleton_path"), SkeletonPath);
    Params->SetStringField(TEXT("action"), Action);
    if (!ProfileName.IsEmpty())
    {
        Params->SetStringField(TEXT("profile_name"), ProfileName);
    }
    if (!Mode.IsEmpty())
    {
        Params->SetStringField(TEXT("mode"), Mode);
    }
    if (!BonesJson.IsEmpty())
    {
        const TSharedPtr<FJsonObject> Bones = FAscentToolsetUtils::ParseObject(BonesJson);
        if (!Bones.IsValid())
        {
            return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentAnimGraph] BonesJson is not a valid JSON object"));
        }
        Params->SetObjectField(TEXT("bones"), Bones);
    }
    Params->SetBoolField(TEXT("recursive"), bRecursive);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_blend_profile"), Params);
}

FString UAscentAnimGraphToolset::StateMachine(const FString& Path, const FString& Graph, const FString& Action,
    const FString& StateMachineNode, const FString& OptionsJson)
{
    const TSharedPtr<FJsonObject> Params = MakeGraphParams(Path, Graph);
    if (!MergeOptions(OptionsJson, Params))
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentAnimGraph] OptionsJson is not a valid JSON object"));
    }
    Params->SetStringField(TEXT("action"), Action);
    if (!StateMachineNode.IsEmpty())
    {
        Params->SetStringField(TEXT("state_machine"), StateMachineNode);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_anim_state_machine"), Params);
}
