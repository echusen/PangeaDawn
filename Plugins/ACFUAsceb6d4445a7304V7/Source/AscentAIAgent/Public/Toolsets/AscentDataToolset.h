// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AscentDataToolset.generated.h"

/// Data authoring tools for the project: DataAssets,
/// DataTables, UserDefinedStructs/Enums, StringTables, and GameplayTags.
/// All tools return a JSON string: {"status":"success","success":true,...}
/// on success or {"status":"error","success":false,"error":...} on failure.
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentDataToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:

    /**
     * Inspects a UserDefinedStruct asset: lists every member with its GUID, type,
     * default value, and metadata, plus the struct-level tooltip.
     * @param AssetPath Full content path of the struct, e.g. "/Game/Data/S_ItemInfo".
     * @return JSON: {"success":true,"members":[{"name","guid","type","default_value",...}]}.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString StructInspect(const FString& AssetPath);

    /** Inspects a DataAsset and its reflected property values (was ue_da_inspect). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString DataAssetInspect(const FString& AssetPath);

    /**
     * Lists existing assets of a native class: saved instances (DataAssets, graphs) and Blueprint
     * subclasses (for example gameplay abilities or widgets). Use it to find assets to reference instead of creating new ones.
     * @param AssetClass Native class name or /Script path, e.g. "DataTable", "PrimaryDataAsset", "/Script/Engine.CurveFloat".
     * @param Path Content root to search recursively, e.g. "/Game/MyProject". Pass an empty string to search everything.
     * @param Limit Maximum entries returned. Pass -1 to use the default (200).
     * @return JSON {"success":true,"asset_class","count","truncated","assets":[{"name","path","class","kind":"asset"|"blueprint"}]}.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString ListAssets(const FString& AssetClass, const FString& Path, int32 Limit);


    /** Sets a field on existing struct-array elements in a DataAsset (was ue_da_set_array_property).
     * @param Indices Comma-separated indices or "all"; pass an empty string to edit every element.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString DataAssetSetArrayProperty(const FString& AssetPath, const FString& ArrayProperty, const FString& FieldName, const FString& NewValue, const FString& Indices);


    /** Adds a GameplayTag row to a GameplayTag DataTable (was ue_dt_add_tag).
     * @param DevComment Pass an empty string to use the default comment.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString DataTableAddTag(const FString& DataTablePath, const FString& Tag, const FString& DevComment);

    /** Lists registered GameplayTags with optional filters (was ue_gt_list).
     * @param Query,Root,PropertyHint Pass empty strings to skip each filter.
     * @param Limit Pass -1 to use the handler default.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString TagList(const FString& Query, const FString& Root, const FString& PropertyHint, int32 Limit);

    /** Searches registered GameplayTags with optional filters (was ue_gt_search).
     * @param Root,PropertyHint Pass empty strings to skip each filter.
     * @param Limit Pass -1 to use the handler default.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString TagSearch(const FString& Query, const FString& Root, const FString& PropertyHint, int32 Limit);

    /** Validates one GameplayTag and returns close matches (was ue_gt_validate).
     * @param Root,PropertyHint Pass empty strings to skip each filter.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString TagValidate(const FString& Tag, const FString& Root, const FString& PropertyHint);


    /** Creates a UserDefinedStruct with optional member specifications (was ue_uds_create).
     * @param Tooltip Pass an empty string to skip the struct tooltip.
     * @param MembersJson Optional JSON array of member specification objects; pass an empty string to skip.
     * @param Save Optional "true" or "false"; pass an empty string to use the handler default.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString StructCreate(const FString& Path, const FString& Tooltip, const FString& MembersJson, const FString& Save);

    /** Adds UserDefinedStruct members from JSON member specification objects (was ue_uds_add_members). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString StructAddMembers(const FString& Path, const FString& MembersJson);

    /** Removes UserDefinedStruct members by name or GUID (was ue_uds_remove_members). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString StructRemoveMembers(const FString& Path, const FString& MembersJson);

    /** Renames a UserDefinedStruct member selected by name or GUID (was ue_uds_rename_member). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString StructRenameMember(const FString& Path, const FString& OldName, const FString& NewName);

    /** Sets a UserDefinedStruct member's literal default value (was ue_uds_set_member_default). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString StructSetMemberDefault(const FString& Path, const FString& Name, const FString& Value);

    /** Reorders UserDefinedStruct members by names or GUIDs (was ue_uds_reorder_members). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString StructReorderMembers(const FString& Path, const FString& OrderJson);

    /** Creates a UserDefinedEnum with optional entries (was ue_ude_create).
     * @param EntriesJson Optional JSON array of display-name strings or entry objects; pass an empty string to skip.
     * @param Bitflags Optional "true" or "false"; pass an empty string to use the handler default.
     * @param Save Optional "true" or "false"; pass an empty string to use the handler default.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString EnumCreate(const FString& Path, const FString& Bitflags, const FString& EntriesJson, const FString& Save);

    /** Inspects a UserDefinedEnum and can include its auto-managed _MAX entry (was ue_ude_inspect).
     * @param IncludeMax Optional "true" or "false"; pass an empty string to use the handler default.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString EnumInspect(const FString& Path, const FString& IncludeMax);

    /** Adds UserDefinedEnum entries from display-name strings or objects (was ue_ude_add_entries). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString EnumAddEntries(const FString& Path, const FString& EntriesJson);

    /** Removes UserDefinedEnum entries by index (was ue_ude_remove_entries). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString EnumRemoveEntries(const FString& Path, const FString& IndicesJson);

    /** Renames a UserDefinedEnum entry at an index (was ue_ude_rename_entry). */
    UFUNCTION(meta = (AICallable), Category = "AscentData")
    static FString EnumRenameEntry(const FString& Path, int32 Index, const FString& NewName);
};
