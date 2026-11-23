// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/AgenticMCPStructEnumCommands.h"
#include "Commands/AgenticMCPCommonUtils.h"
#include "AgenticMCPModule.h"

#include "StructUtils/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Kismet2/EnumEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/Paths.h"

namespace AgenticUdsEditor
{
    // ─────────────────────────────────────────────────────────────────────
    // Asset loaders
    // ─────────────────────────────────────────────────────────────────────

    /** Load a UUserDefinedStruct from a content path, with error envelope. */
    static UUserDefinedStruct* LoadUdsOrError(const FString& Path, TSharedPtr<FJsonObject>& OutError)
    {
        UObject* Asset = LoadObject<UObject>(nullptr, *Path);
        if (!Asset)
        {
            const FString LeafName = FPaths::GetBaseFilename(Path);
            Asset = LoadObject<UObject>(nullptr, *(Path + TEXT(".") + LeafName));
        }
        if (!Asset)
        {
            OutError = FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Asset not found at %s"), *Path));
            return nullptr;
        }
        UUserDefinedStruct* Uds = Cast<UUserDefinedStruct>(Asset);
        if (!Uds)
        {
            OutError = FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Expected UUserDefinedStruct, got %s at %s"),
                    *Asset->GetClass()->GetName(), *Path));
            return nullptr;
        }
        return Uds;
    }

    /** Load a UUserDefinedEnum from a content path, with error envelope. */
    static UUserDefinedEnum* LoadUdeOrError(const FString& Path, TSharedPtr<FJsonObject>& OutError)
    {
        UObject* Asset = LoadObject<UObject>(nullptr, *Path);
        if (!Asset)
        {
            const FString LeafName = FPaths::GetBaseFilename(Path);
            Asset = LoadObject<UObject>(nullptr, *(Path + TEXT(".") + LeafName));
        }
        if (!Asset)
        {
            OutError = FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Asset not found at %s"), *Path));
            return nullptr;
        }
        UUserDefinedEnum* Ude = Cast<UUserDefinedEnum>(Asset);
        if (!Ude)
        {
            OutError = FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Expected UUserDefinedEnum, got %s at %s"),
                    *Asset->GetClass()->GetName(), *Path));
            return nullptr;
        }
        return Ude;
    }

    /** Split a full content path into (package_dir, asset_name).
     *  "/Game/Data/S_DamageInfo" → ("/Game/Data", "S_DamageInfo"). */
    static bool SplitAssetPath(const FString& InPath, FString& OutPkgDir, FString& OutAssetName, FString& OutErr)
    {
        int32 LastSlash;
        if (!InPath.FindLastChar(TEXT('/'), LastSlash) || LastSlash == 0)
        {
            OutErr = FString::Printf(TEXT("Invalid content path '%s' (expected /Game/.../Name)"), *InPath);
            return false;
        }
        OutPkgDir   = InPath.Left(LastSlash);
        OutAssetName = InPath.Mid(LastSlash + 1);
        if (OutAssetName.IsEmpty())
        {
            OutErr = FString::Printf(TEXT("Invalid content path '%s' (asset name missing)"), *InPath);
            return false;
        }
        return true;
    }

    /** FStructVariableDescription → JSON. */
    static TSharedPtr<FJsonObject> StructMemberToJson(const FStructVariableDescription& Var)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        // FriendlyName is what the user sees; VarName is the internal serialized name (with GUID suffix).
        const FString FriendlyName = Var.FriendlyName.IsEmpty() ? Var.VarName.ToString() : Var.FriendlyName;
        O->SetStringField(TEXT("name"), FriendlyName);
        O->SetStringField(TEXT("internal_name"), Var.VarName.ToString());
        O->SetStringField(TEXT("guid"), Var.VarGuid.ToString());

        // Pin type round-trip.
        TSharedPtr<FJsonObject> Type = MakeShared<FJsonObject>();
        const FEdGraphPinType& P = Var.ToPinType();
        Type->SetStringField(TEXT("category"), P.PinCategory.ToString());
        if (!P.PinSubCategory.IsNone()) Type->SetStringField(TEXT("subcategory"), P.PinSubCategory.ToString());
        if (UObject* So = P.PinSubCategoryObject.Get())
        {
            Type->SetStringField(TEXT("subcategory_object"), So->GetPathName());
        }
        switch (P.ContainerType)
        {
            case EPinContainerType::Array: Type->SetStringField(TEXT("container"), TEXT("Array")); break;
            case EPinContainerType::Set:   Type->SetStringField(TEXT("container"), TEXT("Set"));   break;
            case EPinContainerType::Map:   Type->SetStringField(TEXT("container"), TEXT("Map"));   break;
            default:                       Type->SetStringField(TEXT("container"), TEXT("None"));  break;
        }
        O->SetObjectField(TEXT("type"), Type);

        O->SetStringField(TEXT("default_value"), Var.DefaultValue);
        O->SetStringField(TEXT("tooltip"), Var.ToolTip);
        O->SetBoolField  (TEXT("editable_on_instance"), Var.bDontEditOnInstance == false);
        O->SetBoolField  (TEXT("save_game"),            Var.bEnableSaveGame);
        O->SetBoolField  (TEXT("multi_line"),           Var.bEnableMultiLineText);
        O->SetBoolField  (TEXT("show_3d_widget"),       Var.bEnable3dWidget);

        // Metadata bag — TMap<FName, FString> per the engine struct definition.
        if (Var.MetaData.Num() > 0)
        {
            TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
            for (const TPair<FName, FString>& Pair : Var.MetaData)
            {
                Meta->SetStringField(Pair.Key.ToString(), Pair.Value);
            }
            O->SetObjectField(TEXT("metadata"), Meta);
        }
        return O;
    }

    /** Resolve a member identifier (name OR guid string) to its FGuid. */
    static bool ResolveMemberGuid(UUserDefinedStruct* Uds, const FString& Identifier, FGuid& OutGuid)
    {
        // 1) Try guid parse directly.
        if (FGuid::Parse(Identifier, OutGuid))
        {
            for (const FStructVariableDescription& V : FStructureEditorUtils::GetVarDesc(Uds))
            {
                if (V.VarGuid == OutGuid) return true;
            }
            return false;
        }
        // 2) Fall back to friendly-name match.
        for (const FStructVariableDescription& V : FStructureEditorUtils::GetVarDesc(Uds))
        {
            const FString Friendly = V.FriendlyName.IsEmpty() ? V.VarName.ToString() : V.FriendlyName;
            if (Friendly.Equals(Identifier, ESearchCase::IgnoreCase))
            {
                OutGuid = V.VarGuid;
                return true;
            }
        }
        return false;
    }

    /** Apply the editable_on_instance / save_game / multi_line / 3d_widget / tooltip / metadata
     *  flags to a member (post-add or post-rename). Per-flag isolation — bad metadata key
     *  doesn't kill the rest. */
    static void ApplyMemberFlags(UUserDefinedStruct* Uds, const FGuid& Guid,
                                 const TSharedPtr<FJsonObject>& Spec,
                                 TArray<TSharedPtr<FJsonValue>>& OutApplied)
    {
        FString Tooltip;
        if (Spec->TryGetStringField(TEXT("tooltip"), Tooltip))
        {
            if (FStructureEditorUtils::ChangeVariableTooltip(Uds, Guid, Tooltip))
            {
                OutApplied.Add(MakeShared<FJsonValueString>(TEXT("tooltip")));
            }
        }

        bool bEditable = true;
        if (Spec->TryGetBoolField(TEXT("editable_on_instance"), bEditable))
        {
            if (FStructureEditorUtils::ChangeEditableOnBPInstance(Uds, Guid, bEditable))
            {
                OutApplied.Add(MakeShared<FJsonValueString>(TEXT("editable_on_instance")));
            }
        }

        bool bSave = false;
        if (Spec->TryGetBoolField(TEXT("save_game"), bSave))
        {
            if (FStructureEditorUtils::ChangeSaveGameEnabled(Uds, Guid, bSave))
            {
                OutApplied.Add(MakeShared<FJsonValueString>(TEXT("save_game")));
            }
        }

        bool bMulti = false;
        if (Spec->TryGetBoolField(TEXT("multi_line"), bMulti))
        {
            if (FStructureEditorUtils::CanEnableMultiLineText(Uds, Guid)
                && FStructureEditorUtils::ChangeMultiLineTextEnabled(Uds, Guid, bMulti))
            {
                OutApplied.Add(MakeShared<FJsonValueString>(TEXT("multi_line")));
            }
        }

        bool b3d = false;
        if (Spec->TryGetBoolField(TEXT("show_3d_widget"), b3d))
        {
            if (FStructureEditorUtils::CanEnable3dWidget(Uds, Guid)
                && FStructureEditorUtils::Change3dWidgetEnabled(Uds, Guid, b3d))
            {
                OutApplied.Add(MakeShared<FJsonValueString>(TEXT("show_3d_widget")));
            }
        }

        // Arbitrary metadata bag.
        const TSharedPtr<FJsonObject>* MetaSpec = nullptr;
        if (Spec->TryGetObjectField(TEXT("metadata"), MetaSpec) && MetaSpec && MetaSpec->IsValid())
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*MetaSpec)->Values)
            {
                FString Val;
                if (Pair.Value.IsValid() && Pair.Value->TryGetString(Val))
                {
                    if (FStructureEditorUtils::SetMetaData(Uds, Guid, FName(*Pair.Key), Val))
                    {
                        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
                        Entry->SetStringField(Pair.Key, Val);
                        OutApplied.Add(MakeShared<FJsonValueObject>(Entry));
                    }
                }
            }
        }
    }

    /** Save an asset by content path (no-op on failure). */
    static bool SaveAssetIfRequested(const TSharedPtr<FJsonObject>& Params, const FString& Path)
    {
        bool bSave = false;
        if (Params->TryGetBoolField(TEXT("save"), bSave) && bSave)
        {
            return UEditorAssetLibrary::SaveAsset(Path, /*bOnlyIfDirty=*/false);
        }
        return false;
    }
}

// =============================================================================
// Dispatch
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleCommand(
    const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("uds_create"))           return HandleUdsCreate(Params);
    if (CommandType == TEXT("uds_inspect"))          return HandleUdsInspect(Params);
    if (CommandType == TEXT("uds_add_members"))      return HandleUdsAddMembers(Params);
    if (CommandType == TEXT("uds_remove_members"))   return HandleUdsRemoveMembers(Params);
    if (CommandType == TEXT("uds_rename_member"))    return HandleUdsRenameMember(Params);
    if (CommandType == TEXT("uds_set_member_default")) return HandleUdsSetMemberDefault(Params);
    if (CommandType == TEXT("uds_reorder_members"))  return HandleUdsReorderMembers(Params);

    if (CommandType == TEXT("ude_create"))           return HandleUdeCreate(Params);
    if (CommandType == TEXT("ude_inspect"))          return HandleUdeInspect(Params);
    if (CommandType == TEXT("ude_add_entries"))      return HandleUdeAddEntries(Params);
    if (CommandType == TEXT("ude_remove_entries"))   return HandleUdeRemoveEntries(Params);
    if (CommandType == TEXT("ude_rename_entry"))     return HandleUdeRenameEntry(Params);

    return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown UDS/UDE command: %s"), *CommandType));
}

// =============================================================================
// UDS — UserDefinedStruct
// =============================================================================

// ─── HandleUdsCreate ────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdsCreate(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' (full content path, e.g. /Game/Data/S_Foo)"));

    if (UEditorAssetLibrary::DoesAssetExist(Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset already exists at %s — use uds_add_members to extend it"), *Path));

    FString PkgDir, AssetName, Err;
    if (!SplitAssetPath(Path, PkgDir, AssetName, Err))
        return FAgenticMCPCommonUtils::CreateErrorResponse(Err);

    UPackage* Package = CreatePackage(*Path);
    if (!Package)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("CreatePackage failed for %s"), *Path));

    UUserDefinedStruct* NewStruct = FStructureEditorUtils::CreateUserDefinedStruct(
        Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!NewStruct)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("FStructureEditorUtils::CreateUserDefinedStruct returned null for %s"), *AssetName));

    FAssetRegistryModule::AssetCreated(NewStruct);
    Package->MarkPackageDirty();

    // CreateUserDefinedStruct seeds one default boolean member ("MemberVar"). Strip it so callers
    // can start fresh — the round-trip story with C++ headers is cleaner without the placeholder.
    {
        const TArray<FStructVariableDescription>& Initial = FStructureEditorUtils::GetVarDesc(NewStruct);
        TArray<FGuid> InitialGuids;
        InitialGuids.Reserve(Initial.Num());
        for (const FStructVariableDescription& V : Initial) InitialGuids.Add(V.VarGuid);
        for (const FGuid& G : InitialGuids)
        {
            FStructureEditorUtils::RemoveVariable(NewStruct, G);
        }
    }

    // Optional struct-level tooltip.
    FString Tooltip;
    if (Params->TryGetStringField(TEXT("tooltip"), Tooltip))
    {
        FStructureEditorUtils::ChangeTooltip(NewStruct, Tooltip);
    }

    // Optional initial members.
    TArray<TSharedPtr<FJsonValue>> MemberResults;
    int32 AddedCount = 0;
    int32 ErrorCount = 0;
    const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
    if (Params->TryGetArrayField(TEXT("members"), Members) && Members)
    {
        for (const TSharedPtr<FJsonValue>& Entry : *Members)
        {
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            if (!Entry.IsValid() || Entry->Type != EJson::Object)
            {
                R->SetStringField(TEXT("error"), TEXT("member entry must be a JSON object"));
                ++ErrorCount;
                MemberResults.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }
            const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();
            FString MemberName;
            if (!Spec->TryGetStringField(TEXT("name"), MemberName))
            {
                R->SetStringField(TEXT("error"), TEXT("member.name missing"));
                ++ErrorCount;
                MemberResults.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }
            R->SetStringField(TEXT("name"), MemberName);

            const TSharedPtr<FJsonObject>* TypeSpec = nullptr;
            FEdGraphPinType PinType;
            FString TypeErr;
            if (!Spec->TryGetObjectField(TEXT("type"), TypeSpec) || !TypeSpec || !TypeSpec->IsValid()
                || !FAgenticMCPCommonUtils::BuildPinTypeFromJson(*TypeSpec, PinType, TypeErr))
            {
                R->SetStringField(TEXT("error"), FString::Printf(TEXT("type build failed: %s"),
                    *(TypeErr.IsEmpty() ? FString(TEXT("missing type")) : TypeErr)));
                ++ErrorCount;
                MemberResults.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }

            const int32 OldCount = FStructureEditorUtils::GetVarDesc(NewStruct).Num();
            if (!FStructureEditorUtils::AddVariable(NewStruct, PinType))
            {
                R->SetStringField(TEXT("error"), TEXT("FStructureEditorUtils::AddVariable failed"));
                ++ErrorCount;
                MemberResults.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }

            const TArray<FStructVariableDescription>& Desc = FStructureEditorUtils::GetVarDesc(NewStruct);
            if (Desc.Num() <= OldCount)
            {
                R->SetStringField(TEXT("error"), TEXT("AddVariable returned true but VarDesc count did not grow"));
                ++ErrorCount;
                MemberResults.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }

            const FGuid NewGuid = Desc.Last().VarGuid;
            FStructureEditorUtils::RenameVariable(NewStruct, NewGuid, MemberName);

            FString Default;
            if (Spec->TryGetStringField(TEXT("default_value"), Default))
            {
                FStructureEditorUtils::ChangeVariableDefaultValue(NewStruct, NewGuid, Default);
            }

            TArray<TSharedPtr<FJsonValue>> Applied;
            ApplyMemberFlags(NewStruct, NewGuid, Spec, Applied);

            R->SetStringField(TEXT("guid"), NewGuid.ToString());
            R->SetArrayField (TEXT("applied"), Applied);
            R->SetBoolField  (TEXT("added"), true);
            MemberResults.Add(MakeShared<FJsonValueObject>(R));
            ++AddedCount;
        }
    }

    FStructureEditorUtils::CompileStructure(NewStruct);
    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),         Path);
    Result->SetStringField(TEXT("name"),         AssetName);
    Result->SetStringField(TEXT("type"),         TEXT("UserDefinedStruct"));
    Result->SetArrayField (TEXT("members"),      MemberResults);
    Result->SetNumberField(TEXT("added_count"),  AddedCount);
    Result->SetNumberField(TEXT("error_count"),  ErrorCount);
    Result->SetBoolField  (TEXT("created"),      true);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdsInspect ───────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdsInspect(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedStruct* Uds = LoadUdsOrError(Path, Err);
    if (!Uds) return Err;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"), Path);
    Result->SetStringField(TEXT("name"), Uds->GetName());
    Result->SetStringField(TEXT("type"), TEXT("UserDefinedStruct"));
    Result->SetStringField(TEXT("guid"), Uds->Guid.ToString());

    // Struct-level tooltip.
    if (UUserDefinedStructEditorData* EditorData = Cast<UUserDefinedStructEditorData>(Uds->EditorData))
    {
        Result->SetStringField(TEXT("tooltip"), EditorData->ToolTip);
    }

    TArray<TSharedPtr<FJsonValue>> Members;
    for (const FStructVariableDescription& V : FStructureEditorUtils::GetVarDesc(Uds))
    {
        Members.Add(MakeShared<FJsonValueObject>(StructMemberToJson(V)));
    }
    Result->SetArrayField (TEXT("members"),      Members);
    Result->SetNumberField(TEXT("member_count"), Members.Num());

    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdsAddMembers ────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdsAddMembers(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
    if (!Params->TryGetArrayField(TEXT("members"), Members) || !Members)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'members' array"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedStruct* Uds = LoadUdsOrError(Path, Err);
    if (!Uds) return Err;

    Uds->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 AddedCount = 0;
    int32 ErrorCount = 0;
    Results.Reserve(Members->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *Members)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        if (!Entry.IsValid() || Entry->Type != EJson::Object)
        {
            R->SetStringField(TEXT("error"), TEXT("member entry must be a JSON object"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();

        FString MemberName;
        if (!Spec->TryGetStringField(TEXT("name"), MemberName))
        {
            R->SetStringField(TEXT("error"), TEXT("member.name missing"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("name"), MemberName);

        const TSharedPtr<FJsonObject>* TypeSpec = nullptr;
        FEdGraphPinType PinType;
        FString TypeErr;
        if (!Spec->TryGetObjectField(TEXT("type"), TypeSpec) || !TypeSpec || !TypeSpec->IsValid()
            || !FAgenticMCPCommonUtils::BuildPinTypeFromJson(*TypeSpec, PinType, TypeErr))
        {
            R->SetStringField(TEXT("error"), FString::Printf(TEXT("type build failed: %s"),
                *(TypeErr.IsEmpty() ? FString(TEXT("missing type")) : TypeErr)));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        const int32 OldCount = FStructureEditorUtils::GetVarDesc(Uds).Num();
        if (!FStructureEditorUtils::AddVariable(Uds, PinType))
        {
            R->SetStringField(TEXT("error"), TEXT("FStructureEditorUtils::AddVariable failed (invalid type?)"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        const TArray<FStructVariableDescription>& Desc = FStructureEditorUtils::GetVarDesc(Uds);
        if (Desc.Num() <= OldCount)
        {
            R->SetStringField(TEXT("error"), TEXT("VarDesc count did not grow after AddVariable"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        const FGuid NewGuid = Desc.Last().VarGuid;
        FStructureEditorUtils::RenameVariable(Uds, NewGuid, MemberName);

        FString Default;
        if (Spec->TryGetStringField(TEXT("default_value"), Default))
        {
            FStructureEditorUtils::ChangeVariableDefaultValue(Uds, NewGuid, Default);
        }

        TArray<TSharedPtr<FJsonValue>> Applied;
        ApplyMemberFlags(Uds, NewGuid, Spec, Applied);

        R->SetStringField(TEXT("guid"),    NewGuid.ToString());
        R->SetArrayField (TEXT("applied"), Applied);
        R->SetBoolField  (TEXT("added"),   true);
        Results.Add(MakeShared<FJsonValueObject>(R));
        ++AddedCount;
    }

    FStructureEditorUtils::CompileStructure(Uds);
    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),         Path);
    Result->SetArrayField (TEXT("members"),      Results);
    Result->SetNumberField(TEXT("added_count"),  AddedCount);
    Result->SetNumberField(TEXT("error_count"),  ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdsRemoveMembers ─────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdsRemoveMembers(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Params->TryGetArrayField(TEXT("members"), Items) || !Items)
    {
        if (!Params->TryGetArrayField(TEXT("names"), Items) || !Items)
            return FAgenticMCPCommonUtils::CreateErrorResponse(
                TEXT("Missing 'members' (array of names or guids) or legacy 'names'"));
    }

    TSharedPtr<FJsonObject> Err;
    UUserDefinedStruct* Uds = LoadUdsOrError(Path, Err);
    if (!Uds) return Err;

    Uds->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 RemovedCount = 0;
    int32 ErrorCount = 0;

    for (const TSharedPtr<FJsonValue>& Entry : *Items)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        FString Identifier;
        if (!Entry.IsValid() || !Entry->TryGetString(Identifier))
        {
            R->SetStringField(TEXT("error"), TEXT("entry must be a string (name or guid)"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("identifier"), Identifier);

        FGuid Guid;
        if (!ResolveMemberGuid(Uds, Identifier, Guid))
        {
            R->SetStringField(TEXT("error"), TEXT("member not found"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        if (!FStructureEditorUtils::RemoveVariable(Uds, Guid))
        {
            R->SetStringField(TEXT("error"), TEXT("FStructureEditorUtils::RemoveVariable returned false"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("guid"),    Guid.ToString());
        R->SetBoolField  (TEXT("removed"), true);
        Results.Add(MakeShared<FJsonValueObject>(R));
        ++RemovedCount;
    }

    FStructureEditorUtils::CompileStructure(Uds);
    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),          Path);
    Result->SetArrayField (TEXT("members"),       Results);
    Result->SetNumberField(TEXT("removed_count"), RemovedCount);
    Result->SetNumberField(TEXT("error_count"),   ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdsRenameMember ──────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdsRenameMember(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path, OldName, NewName;
    if (!Params->TryGetStringField(TEXT("path"),     Path))    return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("old_name"), OldName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_name' (or guid)"));
    if (!Params->TryGetStringField(TEXT("new_name"), NewName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name'"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedStruct* Uds = LoadUdsOrError(Path, Err);
    if (!Uds) return Err;

    FGuid Guid;
    if (!ResolveMemberGuid(Uds, OldName, Guid))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Member not found: %s"), *OldName));

    if (!FStructureEditorUtils::IsUniqueVariableFriendlyName(Uds, NewName))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("New name '%s' already in use"), *NewName));

    Uds->Modify();
    const bool bRenamed = FStructureEditorUtils::RenameVariable(Uds, Guid, NewName);
    if (!bRenamed)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("FStructureEditorUtils::RenameVariable returned false"));

    FStructureEditorUtils::CompileStructure(Uds);
    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),     Path);
    Result->SetStringField(TEXT("old_name"), OldName);
    Result->SetStringField(TEXT("new_name"), NewName);
    Result->SetStringField(TEXT("guid"),     Guid.ToString());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdsSetMemberDefault ──────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdsSetMemberDefault(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path, Identifier, Value;
    if (!Params->TryGetStringField(TEXT("path"),       Path))       return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("name"),       Identifier)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' (or guid)"));
    if (!Params->TryGetStringField(TEXT("value"),      Value))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' (literal string)"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedStruct* Uds = LoadUdsOrError(Path, Err);
    if (!Uds) return Err;

    FGuid Guid;
    if (!ResolveMemberGuid(Uds, Identifier, Guid))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Member not found: %s"), *Identifier));

    Uds->Modify();
    if (!FStructureEditorUtils::ChangeVariableDefaultValue(Uds, Guid, Value))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            TEXT("FStructureEditorUtils::ChangeVariableDefaultValue returned false (invalid literal for type?)"));

    FStructureEditorUtils::CompileStructure(Uds);
    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),  Path);
    Result->SetStringField(TEXT("name"),  Identifier);
    Result->SetStringField(TEXT("guid"),  Guid.ToString());
    Result->SetStringField(TEXT("value"), Value);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdsReorderMembers ─────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdsReorderMembers(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* OrderArr = nullptr;
    if (!Params->TryGetArrayField(TEXT("order"), OrderArr) || !OrderArr)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'order' (array of names or guids in the desired top-to-bottom sequence)"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedStruct* Uds = LoadUdsOrError(Path, Err);
    if (!Uds) return Err;

    // Resolve every order entry to a guid up front. Refuse if any is unknown.
    TArray<FGuid> Order;
    Order.Reserve(OrderArr->Num());
    for (int32 Idx = 0; Idx < OrderArr->Num(); ++Idx)
    {
        FString Ident;
        if (!(*OrderArr)[Idx].IsValid() || !(*OrderArr)[Idx]->TryGetString(Ident))
        {
            return FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("order[%d] must be a string"), Idx));
        }
        FGuid G;
        if (!ResolveMemberGuid(Uds, Ident, G))
        {
            return FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("order[%d] member not found: %s"), Idx, *Ident));
        }
        Order.Add(G);
    }

    Uds->Modify();

    // Walk the target sequence pairwise. For each i>0, ensure Order[i] sits directly
    // below Order[i-1]. MoveVariable handles the relative-position bookkeeping.
    int32 Moved = 0;
    for (int32 i = 1; i < Order.Num(); ++i)
    {
        if (FStructureEditorUtils::CanMoveVariable(Uds, Order[i], Order[i-1], FStructureEditorUtils::PositionBelow))
        {
            if (FStructureEditorUtils::MoveVariable(Uds, Order[i], Order[i-1], FStructureEditorUtils::PositionBelow))
            {
                ++Moved;
            }
        }
    }

    FStructureEditorUtils::CompileStructure(Uds);
    SaveAssetIfRequested(Params, Path);

    // Echo the post-move order so callers can verify.
    TArray<TSharedPtr<FJsonValue>> Final;
    for (const FStructVariableDescription& V : FStructureEditorUtils::GetVarDesc(Uds))
    {
        const FString Name = V.FriendlyName.IsEmpty() ? V.VarName.ToString() : V.FriendlyName;
        TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
        E->SetStringField(TEXT("name"), Name);
        E->SetStringField(TEXT("guid"), V.VarGuid.ToString());
        Final.Add(MakeShared<FJsonValueObject>(E));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),         Path);
    Result->SetNumberField(TEXT("moved"),        Moved);
    Result->SetArrayField (TEXT("final_order"),  Final);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// UDE — UserDefinedEnum
// =============================================================================

namespace AgenticUdeEditor
{
    /** Is this index the trailing `_MAX` sentinel? UE auto-manages it. */
    static bool IsMaxIndex(UUserDefinedEnum* Ude, int32 Index)
    {
        return Ude && Index == Ude->NumEnums() - 1;
    }
}

// ─── HandleUdeCreate ────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdeCreate(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    if (UEditorAssetLibrary::DoesAssetExist(Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset already exists at %s — use ude_add_entries to extend it"), *Path));

    FString PkgDir, AssetName, Err;
    if (!SplitAssetPath(Path, PkgDir, AssetName, Err))
        return FAgenticMCPCommonUtils::CreateErrorResponse(Err);

    UPackage* Package = CreatePackage(*Path);
    if (!Package)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("CreatePackage failed for %s"), *Path));

    UEnum* CreatedEnumBase = FEnumEditorUtils::CreateUserDefinedEnum(
        Package, FName(*AssetName), RF_Public | RF_Standalone);
    UUserDefinedEnum* NewEnum = Cast<UUserDefinedEnum>(CreatedEnumBase);
    if (!NewEnum)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("FEnumEditorUtils::CreateUserDefinedEnum returned non-UUserDefinedEnum for %s"), *AssetName));

    FAssetRegistryModule::AssetCreated(NewEnum);
    Package->MarkPackageDirty();

    // CreateUserDefinedEnum seeds one default entry. Replace strategy: strip it before adding seeds.
    while (NewEnum->NumEnums() > 1)  // > 1 keeps the trailing _MAX
    {
        FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(NewEnum, 0);
    }

    // Optional bitflags toggle.
    bool bBitflags = false;
    if (Params->TryGetBoolField(TEXT("bitflags"), bBitflags) && bBitflags)
    {
        FEnumEditorUtils::SetEnumeratorBitflagsTypeState(NewEnum, true);
    }

    TArray<TSharedPtr<FJsonValue>> EntryResults;
    int32 AddedCount = 0;
    int32 ErrorCount = 0;
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (Params->TryGetArrayField(TEXT("entries"), Entries) && Entries)
    {
        for (const TSharedPtr<FJsonValue>& Entry : *Entries)
        {
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();

            FString DisplayName, Tooltip;
            if (Entry.IsValid() && Entry->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();
                Spec->TryGetStringField(TEXT("display_name"), DisplayName);
                Spec->TryGetStringField(TEXT("tooltip"),      Tooltip);
            }
            else if (Entry.IsValid() && Entry->TryGetString(DisplayName))
            {
                // bare string accepted as display name
            }
            else
            {
                R->SetStringField(TEXT("error"), TEXT("entry must be a string or object with display_name"));
                ++ErrorCount;
                EntryResults.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }
            if (DisplayName.IsEmpty())
            {
                R->SetStringField(TEXT("error"), TEXT("display_name missing"));
                ++ErrorCount;
                EntryResults.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }

            FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(NewEnum);
            const int32 NewIdx = NewEnum->NumEnums() - 2;  // before _MAX
            FEnumEditorUtils::SetEnumeratorDisplayName(NewEnum, NewIdx, FText::FromString(DisplayName));
            if (!Tooltip.IsEmpty())
            {
                // Tooltip on enumerator goes through metadata.
                NewEnum->SetMetaData(TEXT("ToolTip"), *Tooltip, NewIdx);
            }

            R->SetNumberField(TEXT("index"),        NewIdx);
            R->SetStringField(TEXT("display_name"), DisplayName);
            if (!Tooltip.IsEmpty()) R->SetStringField(TEXT("tooltip"), Tooltip);
            R->SetBoolField  (TEXT("added"),        true);
            EntryResults.Add(MakeShared<FJsonValueObject>(R));
            ++AddedCount;
        }
    }

    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),         Path);
    Result->SetStringField(TEXT("name"),         AssetName);
    Result->SetStringField(TEXT("type"),         TEXT("UserDefinedEnum"));
    Result->SetBoolField  (TEXT("bitflags"),     FEnumEditorUtils::IsEnumeratorBitflagsType(NewEnum));
    Result->SetArrayField (TEXT("entries"),      EntryResults);
    Result->SetNumberField(TEXT("added_count"),  AddedCount);
    Result->SetNumberField(TEXT("error_count"),  ErrorCount);
    Result->SetBoolField  (TEXT("created"),      true);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdeInspect ───────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdeInspect(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;
    using namespace AgenticUdeEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedEnum* Ude = LoadUdeOrError(Path, Err);
    if (!Ude) return Err;

    bool bIncludeMax = false;
    Params->TryGetBoolField(TEXT("include_max"), bIncludeMax);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),     Path);
    Result->SetStringField(TEXT("name"),     Ude->GetName());
    Result->SetStringField(TEXT("type"),     TEXT("UserDefinedEnum"));
    Result->SetBoolField  (TEXT("bitflags"), FEnumEditorUtils::IsEnumeratorBitflagsType(Ude));

    TArray<TSharedPtr<FJsonValue>> Entries;
    const int32 Count = Ude->NumEnums();
    for (int32 i = 0; i < Count; ++i)
    {
        const bool bMax = IsMaxIndex(Ude, i);
        if (bMax && !bIncludeMax) continue;

        TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
        E->SetNumberField(TEXT("index"),        i);
        E->SetStringField(TEXT("raw_name"),     Ude->GetNameByIndex(i).ToString());
        E->SetStringField(TEXT("display_name"), Ude->GetDisplayNameTextByIndex(i).ToString());
        E->SetNumberField(TEXT("value"),        static_cast<double>(Ude->GetValueByIndex(i)));
        const FString Tooltip = Ude->GetMetaData(TEXT("ToolTip"), i);
        if (!Tooltip.IsEmpty()) E->SetStringField(TEXT("tooltip"), Tooltip);
        E->SetBoolField  (TEXT("is_max"),       bMax);
        Entries.Add(MakeShared<FJsonValueObject>(E));
    }
    Result->SetArrayField (TEXT("entries"),     Entries);
    Result->SetNumberField(TEXT("entry_count"), Entries.Num());

    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdeAddEntries ────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdeAddEntries(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Params->TryGetArrayField(TEXT("entries"), Entries) || !Entries)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'entries' array"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedEnum* Ude = LoadUdeOrError(Path, Err);
    if (!Ude) return Err;

    Ude->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 AddedCount = 0;
    int32 ErrorCount = 0;

    for (const TSharedPtr<FJsonValue>& Entry : *Entries)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();

        FString DisplayName, Tooltip;
        if (Entry.IsValid() && Entry->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();
            Spec->TryGetStringField(TEXT("display_name"), DisplayName);
            Spec->TryGetStringField(TEXT("tooltip"),      Tooltip);
        }
        else if (Entry.IsValid() && Entry->TryGetString(DisplayName))
        {
            // bare string accepted as display name
        }
        else
        {
            R->SetStringField(TEXT("error"), TEXT("entry must be a string or object with display_name"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        if (DisplayName.IsEmpty())
        {
            R->SetStringField(TEXT("error"), TEXT("display_name missing"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(Ude);
        const int32 NewIdx = Ude->NumEnums() - 2;  // before _MAX
        FEnumEditorUtils::SetEnumeratorDisplayName(Ude, NewIdx, FText::FromString(DisplayName));
        if (!Tooltip.IsEmpty())
        {
            Ude->SetMetaData(TEXT("ToolTip"), *Tooltip, NewIdx);
        }

        R->SetNumberField(TEXT("index"),        NewIdx);
        R->SetStringField(TEXT("display_name"), DisplayName);
        if (!Tooltip.IsEmpty()) R->SetStringField(TEXT("tooltip"), Tooltip);
        R->SetBoolField  (TEXT("added"),        true);
        Results.Add(MakeShared<FJsonValueObject>(R));
        ++AddedCount;
    }

    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),         Path);
    Result->SetArrayField (TEXT("entries"),      Results);
    Result->SetNumberField(TEXT("added_count"),  AddedCount);
    Result->SetNumberField(TEXT("error_count"),  ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdeRemoveEntries ─────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdeRemoveEntries(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;
    using namespace AgenticUdeEditor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* Indices = nullptr;
    if (!Params->TryGetArrayField(TEXT("indices"), Indices) || !Indices)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'indices' (array of int)"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedEnum* Ude = LoadUdeOrError(Path, Err);
    if (!Ude) return Err;

    // Sort indices descending so the per-removal shift doesn't invalidate later entries.
    // Validate up front; refuse `_MAX` removal.
    TArray<int32> Sorted;
    TArray<TSharedPtr<FJsonValue>> EarlyErrors;
    Sorted.Reserve(Indices->Num());
    for (int32 i = 0; i < Indices->Num(); ++i)
    {
        const TSharedPtr<FJsonValue>& V = (*Indices)[i];
        double Num = 0.0;
        if (!V.IsValid() || !V->TryGetNumber(Num))
        {
            TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
            E->SetNumberField(TEXT("position"), i);
            E->SetStringField(TEXT("error"),    TEXT("entry must be an integer"));
            EarlyErrors.Add(MakeShared<FJsonValueObject>(E));
            continue;
        }
        const int32 Idx = static_cast<int32>(Num);
        if (Idx < 0 || Idx >= Ude->NumEnums())
        {
            TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
            E->SetNumberField(TEXT("index"), Idx);
            E->SetStringField(TEXT("error"), TEXT("out of range"));
            EarlyErrors.Add(MakeShared<FJsonValueObject>(E));
            continue;
        }
        if (IsMaxIndex(Ude, Idx))
        {
            TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
            E->SetNumberField(TEXT("index"), Idx);
            E->SetStringField(TEXT("error"), TEXT("cannot remove auto-managed `_MAX` sentinel"));
            EarlyErrors.Add(MakeShared<FJsonValueObject>(E));
            continue;
        }
        Sorted.AddUnique(Idx);
    }
    Sorted.Sort([](int32 A, int32 B) { return A > B; });

    Ude->Modify();

    TArray<TSharedPtr<FJsonValue>> Removed;
    for (int32 Idx : Sorted)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        const FString Display = Ude->GetDisplayNameTextByIndex(Idx).ToString();
        R->SetNumberField(TEXT("index"),        Idx);
        R->SetStringField(TEXT("display_name"), Display);

        FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(Ude, Idx);

        R->SetBoolField(TEXT("removed"), true);
        Removed.Add(MakeShared<FJsonValueObject>(R));
    }

    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),          Path);
    Result->SetArrayField (TEXT("removed"),       Removed);
    Result->SetArrayField (TEXT("errors"),        EarlyErrors);
    Result->SetNumberField(TEXT("removed_count"), Removed.Num());
    Result->SetNumberField(TEXT("error_count"),   EarlyErrors.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleUdeRenameEntry ───────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPStructEnumCommands::HandleUdeRenameEntry(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticUdsEditor;
    using namespace AgenticUdeEditor;

    FString Path, NewName;
    int32 Index = INDEX_NONE;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    double IndexNum = -1.0;
    if (!Params->TryGetNumberField(TEXT("index"), IndexNum))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'index' (int)"));
    Index = static_cast<int32>(IndexNum);
    if (!Params->TryGetStringField(TEXT("new_name"), NewName))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name'"));

    TSharedPtr<FJsonObject> Err;
    UUserDefinedEnum* Ude = LoadUdeOrError(Path, Err);
    if (!Ude) return Err;

    if (Index < 0 || Index >= Ude->NumEnums())
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Index %d out of range [0, %d)"), Index, Ude->NumEnums()));
    if (IsMaxIndex(Ude, Index))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            TEXT("Cannot rename auto-managed `_MAX` sentinel"));

    const FText NewText = FText::FromString(NewName);
    if (!FEnumEditorUtils::IsEnumeratorDisplayNameValid(Ude, Index, NewText))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("New display name '%s' is not valid (duplicate or reserved)"), *NewName));

    Ude->Modify();
    const FString OldName = Ude->GetDisplayNameTextByIndex(Index).ToString();
    if (!FEnumEditorUtils::SetEnumeratorDisplayName(Ude, Index, NewText))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            TEXT("FEnumEditorUtils::SetEnumeratorDisplayName returned false"));

    SaveAssetIfRequested(Params, Path);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),     Path);
    Result->SetNumberField(TEXT("index"),    Index);
    Result->SetStringField(TEXT("old_name"), OldName);
    Result->SetStringField(TEXT("new_name"), NewName);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}
