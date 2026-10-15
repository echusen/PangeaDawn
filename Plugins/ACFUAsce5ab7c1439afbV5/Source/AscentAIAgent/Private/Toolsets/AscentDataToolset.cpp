// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentDataToolset.h"

#include "Toolsets/AscentToolsetUtils.h"

namespace
{
    bool SetJsonArrayField(
        const FString& Json,
        const TCHAR* FieldName,
        const TSharedPtr<FJsonObject>& Params,
        FString& OutError)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        if (!FAscentToolsetUtils::ParseArray(Json, Values))
        {
            OutError = FString::Printf(TEXT("[AscentData] %s is not a valid JSON array"), FieldName);
            return false;
        }

        Params->SetArrayField(FieldName, Values);
        return true;
    }

    bool SetOptionalJsonArrayField(
        const FString& Json,
        const TCHAR* FieldName,
        const TSharedPtr<FJsonObject>& Params,
        FString& OutError)
    {
        return Json.IsEmpty() || SetJsonArrayField(Json, FieldName, Params, OutError);
    }

    bool SetOptionalJsonObjectField(
        const FString& Json,
        const TCHAR* FieldName,
        const TSharedPtr<FJsonObject>& Params,
        FString& OutError)
    {
        if (Json.IsEmpty())
        {
            return true;
        }

        const TSharedPtr<FJsonObject> Value = FAscentToolsetUtils::ParseObject(Json);
        if (!Value.IsValid())
        {
            OutError = FString::Printf(TEXT("[AscentData] %s is not a valid JSON object"), FieldName);
            return false;
        }

        Params->SetObjectField(FieldName, Value);
        return true;
    }

    bool SetOptionalBoolField(
        const FString& Value,
        const TCHAR* FieldName,
        const TSharedPtr<FJsonObject>& Params,
        FString& OutError)
    {
        if (Value.IsEmpty())
        {
            return true;
        }

        if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1"))
        {
            Params->SetBoolField(FieldName, true);
            return true;
        }
        if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0"))
        {
            Params->SetBoolField(FieldName, false);
            return true;
        }

        OutError = FString::Printf(
            TEXT("[AscentData] %s must be \"true\", \"false\", \"1\", or \"0\""),
            FieldName);
        return false;
    }
}


FString UAscentDataToolset::StructInspect(const FString& AssetPath)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), AssetPath);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_uds_inspect"), Params);
}

FString UAscentDataToolset::DataAssetInspect(const FString& AssetPath)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("asset_path"), AssetPath);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_da_inspect"), Params);
}

FString UAscentDataToolset::ListAssets(const FString& AssetClass, const FString& Path, int32 Limit)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("asset_class"), AssetClass);
    Params->SetStringField(TEXT("path"), Path);
    Params->SetNumberField(TEXT("limit"), Limit);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_asset_list"), Params);
}


FString UAscentDataToolset::DataAssetSetArrayProperty(
    const FString& AssetPath,
    const FString& ArrayProperty,
    const FString& FieldName,
    const FString& NewValue,
    const FString& Indices)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("asset_path"), AssetPath);
    Params->SetStringField(TEXT("array_property"), ArrayProperty);
    Params->SetStringField(TEXT("field_name"), FieldName);
    Params->SetStringField(TEXT("new_value"), NewValue);
    if (!Indices.IsEmpty())
    {
        Params->SetStringField(TEXT("indices"), Indices);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_da_set_array_property"), Params);
}


FString UAscentDataToolset::DataTableAddTag(
    const FString& DataTablePath,
    const FString& Tag,
    const FString& DevComment)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("datatable_path"), DataTablePath);
    Params->SetStringField(TEXT("tag"), Tag);
    if (!DevComment.IsEmpty())
    {
        Params->SetStringField(TEXT("dev_comment"), DevComment);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_dt_add_tag"), Params);
}

FString UAscentDataToolset::TagList(
    const FString& Query,
    const FString& Root,
    const FString& PropertyHint,
    int32 Limit)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    if (!Query.IsEmpty())
    {
        Params->SetStringField(TEXT("query"), Query);
    }
    if (!Root.IsEmpty())
    {
        Params->SetStringField(TEXT("root"), Root);
    }
    if (!PropertyHint.IsEmpty())
    {
        Params->SetStringField(TEXT("property_hint"), PropertyHint);
    }
    if (Limit >= 1)
    {
        Params->SetNumberField(TEXT("limit"), Limit);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_gt_list"), Params);
}

FString UAscentDataToolset::TagSearch(
    const FString& Query,
    const FString& Root,
    const FString& PropertyHint,
    int32 Limit)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("query"), Query);
    if (!Root.IsEmpty())
    {
        Params->SetStringField(TEXT("root"), Root);
    }
    if (!PropertyHint.IsEmpty())
    {
        Params->SetStringField(TEXT("property_hint"), PropertyHint);
    }
    if (Limit >= 1)
    {
        Params->SetNumberField(TEXT("limit"), Limit);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_gt_search"), Params);
}

FString UAscentDataToolset::TagValidate(
    const FString& Tag,
    const FString& Root,
    const FString& PropertyHint)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("tag"), Tag);
    if (!Root.IsEmpty())
    {
        Params->SetStringField(TEXT("root"), Root);
    }
    if (!PropertyHint.IsEmpty())
    {
        Params->SetStringField(TEXT("property_hint"), PropertyHint);
    }
    return FAscentToolsetUtils::Dispatch(TEXT("ue_gt_validate"), Params);
}


FString UAscentDataToolset::StructCreate(
    const FString& Path,
    const FString& Tooltip,
    const FString& MembersJson,
    const FString& Save)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);
    if (!Tooltip.IsEmpty())
    {
        Params->SetStringField(TEXT("tooltip"), Tooltip);
    }

    FString Error;
    if (!SetOptionalBoolField(Save, TEXT("save"), Params, Error)
        || !SetOptionalJsonArrayField(MembersJson, TEXT("members"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_uds_create"), Params);
}

FString UAscentDataToolset::StructAddMembers(const FString& Path, const FString& MembersJson)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);

    FString Error;
    if (!SetJsonArrayField(MembersJson, TEXT("members"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_uds_add_members"), Params);
}

FString UAscentDataToolset::StructRemoveMembers(const FString& Path, const FString& MembersJson)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);

    FString Error;
    if (!SetJsonArrayField(MembersJson, TEXT("members"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_uds_remove_members"), Params);
}

FString UAscentDataToolset::StructRenameMember(
    const FString& Path,
    const FString& OldName,
    const FString& NewName)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);
    Params->SetStringField(TEXT("old_name"), OldName);
    Params->SetStringField(TEXT("new_name"), NewName);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_uds_rename_member"), Params);
}

FString UAscentDataToolset::StructSetMemberDefault(
    const FString& Path,
    const FString& Name,
    const FString& Value)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);
    Params->SetStringField(TEXT("name"), Name);
    Params->SetStringField(TEXT("value"), Value);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_uds_set_member_default"), Params);
}

FString UAscentDataToolset::StructReorderMembers(const FString& Path, const FString& OrderJson)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);

    FString Error;
    if (!SetJsonArrayField(OrderJson, TEXT("order"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_uds_reorder_members"), Params);
}

FString UAscentDataToolset::EnumCreate(
    const FString& Path,
    const FString& Bitflags,
    const FString& EntriesJson,
    const FString& Save)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);

    FString Error;
    if (!SetOptionalBoolField(Bitflags, TEXT("bitflags"), Params, Error)
        || !SetOptionalBoolField(Save, TEXT("save"), Params, Error)
        || !SetOptionalJsonArrayField(EntriesJson, TEXT("entries"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_ude_create"), Params);
}

FString UAscentDataToolset::EnumInspect(const FString& Path, const FString& IncludeMax)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);

    FString Error;
    if (!SetOptionalBoolField(IncludeMax, TEXT("include_max"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_ude_inspect"), Params);
}

FString UAscentDataToolset::EnumAddEntries(const FString& Path, const FString& EntriesJson)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);

    FString Error;
    if (!SetJsonArrayField(EntriesJson, TEXT("entries"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_ude_add_entries"), Params);
}

FString UAscentDataToolset::EnumRemoveEntries(const FString& Path, const FString& IndicesJson)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);

    FString Error;
    if (!SetJsonArrayField(IndicesJson, TEXT("indices"), Params, Error))
    {
        return FAscentToolsetUtils::MakeErrorResponse(Error);
    }

    return FAscentToolsetUtils::Dispatch(TEXT("ue_ude_remove_entries"), Params);
}

FString UAscentDataToolset::EnumRenameEntry(const FString& Path, int32 Index, const FString& NewName)
{
    const TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("path"), Path);
    Params->SetNumberField(TEXT("index"), Index);
    Params->SetStringField(TEXT("new_name"), NewName);
    return FAscentToolsetUtils::Dispatch(TEXT("ue_ude_rename_entry"), Params);
}
