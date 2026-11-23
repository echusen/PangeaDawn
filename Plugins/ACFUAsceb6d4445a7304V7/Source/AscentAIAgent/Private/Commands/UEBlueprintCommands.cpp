// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEBlueprintCommands.h"

#include "Commands/AgenticMCPBlueprintCommands.h"
#include "Commands/AgenticMCPBpAuthorCommands.h"
#include "Commands/AgenticMCPStructEnumCommands.h"
#include "Commands/UEBatchToolHelper.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
    /** Field names this domain treats as bulk arrays of objects. */
    const TArray<FString>& GetBulkArrayFieldNames()
    {
        static const TArray<FString> Fields = {
            TEXT("nodes"),
            TEXT("links"),
            TEXT("pins"),
            TEXT("variables"),
            TEXT("components"),
            TEXT("params"),
            TEXT("names"),
            TEXT("node_names"),
            TEXT("variable_names"),
            TEXT("members"),
            TEXT("entries"),
            TEXT("queries"),
            TEXT("properties"),
        };
        return Fields;
    }

    /**
     * Normalize JSON-stringified bulk arrays into native arrays so AUB
     * handlers see uniform shape regardless of how the LLM serialized them.
     */
    void NormalizeBulkArrayFields(const TSharedPtr<FJsonObject>& Params)
    {
        if (!Params.IsValid())
        {
            return;
        }

        for (const FString& FieldName : GetBulkArrayFieldNames())
        {
            const TSharedPtr<FJsonValue> Existing = Params->TryGetField(FieldName);
            if (!Existing.IsValid() || Existing->IsNull())
            {
                continue;
            }

            // Native array already: nothing to rewrite. Most common LLM shape.
            if (Existing->Type == EJson::Array)
            {
                continue;
            }

            TArray<TSharedPtr<FJsonObject>> Items;
            FString IgnoredError;
            if (!FUEBatchToolHelper::ExtractArray(Params, FieldName, Items, IgnoredError))
            {
                continue;
            }

            TArray<TSharedPtr<FJsonValue>> AsValues;
            AsValues.Reserve(Items.Num());
            for (const TSharedPtr<FJsonObject>& Item : Items)
            {
                AsValues.Add(MakeShared<FJsonValueObject>(Item));
            }
            Params->SetArrayField(FieldName, AsValues);
        }
    }

    void CopyStringAliasIfMissing(const TSharedPtr<FJsonObject>& Params, const TCHAR* FromField, const TCHAR* ToField)
    {
        if (!Params.IsValid() || Params->HasField(ToField))
        {
            return;
        }

        FString Value;
        if (Params->TryGetStringField(FromField, Value) && !Value.IsEmpty())
        {
            Params->SetStringField(ToField, Value);
        }
    }

    /**
     * The public MCP catalog uses friendly names (`blueprint_path`, `graph_name`,
     * `node_name`) while several mirrored AgenticUEBridge handlers still expect
     * the converged wire names (`path`, `graph`, `node`). Add aliases at the
     * adapter boundary so the catalog and backend stay compatible.
     */
    void NormalizeSchemaAliases(const TSharedPtr<FJsonObject>& Params)
    {
        CopyStringAliasIfMissing(Params, TEXT("blueprint_path"), TEXT("path"));
        CopyStringAliasIfMissing(Params, TEXT("graph_name"), TEXT("graph"));
        CopyStringAliasIfMissing(Params, TEXT("node_name"), TEXT("node"));
    }

    /**
     * Public MCP names are now canonicalized as ue_bp_* for every generic
     * Blueprint/K2 tool. The mirrored AgenticUEBridge handlers still use a few
     * older top-level verbs, so translate only at this adapter boundary.
     */
    FString NormalizeBlueprintNativeCommand(const FString& NativeCommand)
    {
        static const TMap<FString, FString> LegacyHandlerNames = {
            { TEXT("bp_set_physics_properties"), TEXT("set_physics_properties") },
            { TEXT("bp_set_static_mesh_properties"), TEXT("set_static_mesh_properties") },
            { TEXT("bp_set_mesh_material_color"), TEXT("set_mesh_material_color") }
        };

        if (const FString* Mapped = LegacyHandlerNames.Find(NativeCommand))
        {
            return *Mapped;
        }
        return NativeCommand;
    }
}

FUEBlueprintCommands::FUEBlueprintCommands(UAscentAIAgentSubsystem* InOwner)
    : FUECommandBase(InOwner)
    , BridgeCommands(MakeUnique<FAgenticMCPBlueprintCommands>())
    , BpAuthorCommands(MakeUnique<FAgenticMCPBpAuthorCommands>())
    , StructEnumCommands(MakeUnique<FAgenticMCPStructEnumCommands>())
{
}

FUEBlueprintCommands::~FUEBlueprintCommands() = default;

bool FUEBlueprintCommands::IsBpAuthorCommand(const FString& NativeCommand)
{
    static const TSet<FString> Commands = {
        TEXT("bp_actions_search"),
        TEXT("bp_author_graph"),
    };
    return Commands.Contains(NativeCommand);
}

bool FUEBlueprintCommands::IsStructEnumCommand(const FString& NativeCommand)
{
    static const TSet<FString> Commands = {
        TEXT("uds_create"),
        TEXT("uds_inspect"),
        TEXT("uds_add_members"),
        TEXT("uds_remove_members"),
        TEXT("uds_rename_member"),
        TEXT("uds_set_member_default"),
        TEXT("uds_reorder_members"),
        TEXT("ude_create"),
        TEXT("ude_inspect"),
        TEXT("ude_add_entries"),
        TEXT("ude_remove_entries"),
        TEXT("ude_rename_entry"),
    };
    return Commands.Contains(NativeCommand);
}

bool FUEBlueprintCommands::IsCommand(const FString& CommandType) const
{
    return CommandType.StartsWith(TEXT("ue_bp_"))
        || CommandType.StartsWith(TEXT("ue_uds_"))
        || CommandType.StartsWith(TEXT("ue_ude_"));
}

FString FUEBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonObject> SafeParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();
    NormalizeBulkArrayFields(SafeParams);
    NormalizeSchemaAliases(SafeParams);
    const TSharedPtr<FJsonObject>* SpecPtr = nullptr;
    if (SafeParams->TryGetObjectField(TEXT("spec"), SpecPtr) && SpecPtr)
    {
        NormalizeBulkArrayFields(*SpecPtr);
    }

    FString NativeCommand = NormalizeBlueprintNativeCommand(StripUEPrefix(CommandType));
    if (IsBpAuthorCommand(NativeCommand) && !SafeParams->HasField(TEXT("graph")))
    {
        SafeParams->SetStringField(TEXT("graph"), TEXT("EventGraph"));
    }

    // Route to BpAuthorCommands (bp_actions_search, bp_author_graph)
    if (IsBpAuthorCommand(NativeCommand))
    {
        if (!BpAuthorCommands.IsValid())
        {
            return MakeErrorResponse(TEXT("[ue_bp] bp_author commands not initialised"));
        }
        const TSharedPtr<FJsonObject> Result = BpAuthorCommands->HandleCommand(NativeCommand, SafeParams);
        return ForwardBridgeResult(Result, TEXT("ue_bp"), CommandType);
    }

    // Route to StructEnumCommands (uds_*, ude_*)
    if (IsStructEnumCommand(NativeCommand))
    {
        if (!StructEnumCommands.IsValid())
        {
            return MakeErrorResponse(TEXT("[ue_bp] struct_enum commands not initialised"));
        }
        const TSharedPtr<FJsonObject> Result = StructEnumCommands->HandleCommand(NativeCommand, SafeParams);
        return ForwardBridgeResult(Result, TEXT("ue_bp"), CommandType);
    }

    // Route to main BlueprintCommands (everything else)
    if (!BridgeCommands.IsValid())
    {
        return MakeErrorResponse(TEXT("[ue_bp] bridge commands not initialised"));
    }
    const TSharedPtr<FJsonObject> Result = BridgeCommands->HandleCommand(NativeCommand, SafeParams);
    return ForwardBridgeResult(Result, TEXT("ue_bp"), CommandType);
}
