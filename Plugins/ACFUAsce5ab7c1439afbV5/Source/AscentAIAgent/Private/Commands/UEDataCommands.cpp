// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/UEDataCommands.h"

#include "Commands/AgenticMCPDataCommands.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"

namespace
{
    bool IsGenericDataCommand(const FString& CommandType)
    {
        return CommandType.StartsWith(TEXT("ue_da_"))
            || CommandType.StartsWith(TEXT("ue_gt_"));
    }

    void NormalizeDataParams(const TSharedPtr<FJsonObject>& Params)
    {
        FString AssetPath;
        if (!Params->HasField(TEXT("path")) && Params->TryGetStringField(TEXT("asset_path"), AssetPath))
        {
            Params->SetStringField(TEXT("path"), AssetPath);
        }
    }

    constexpr int32 DefaultAssetListLimit = 200;

    /** Native class by name (with or without its U/A prefix) or by /Script path. */
    UClass* FindNativeClass(const FString& NameOrPath)
    {
        const FString Wanted = NameOrPath.TrimStartAndEnd();
        if (Wanted.StartsWith(TEXT("/")))
        {
            return FindObject<UClass>(nullptr, *Wanted);
        }
        const bool bHasPrefix = Wanted.Len() > 1 && (Wanted[0] == TCHAR('U') || Wanted[0] == TCHAR('A'));
        for (TObjectIterator<UClass> It; It; ++It)
        {
            UClass* Class = *It;
            if (Class->ClassGeneratedBy)
            {
                continue;
            }
            const FString Name = Class->GetName();
            if (Name.Equals(Wanted, ESearchCase::IgnoreCase) || (bHasPrefix && Name.Equals(Wanted.RightChop(1), ESearchCase::IgnoreCase)))
            {
                return Class;
            }
        }
        return nullptr;
    }

    TSharedPtr<FJsonObject> MakeAssetEntry(const FAssetData& Asset, const FString& ClassName, const TCHAR* Kind)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Asset.AssetName.ToString());
        Entry->SetStringField(TEXT("path"), Asset.GetObjectPathString());
        Entry->SetStringField(TEXT("class"), ClassName);
        Entry->SetStringField(TEXT("kind"), Kind);
        return Entry;
    }
}

FUEDataCommands::FUEDataCommands(UAscentAIAgentSubsystem* InOwner)
    : FUECommandBase(InOwner)
    , BridgeCommands(MakeUnique<FAgenticMCPDataCommands>())
{
}

FUEDataCommands::~FUEDataCommands() = default;

bool FUEDataCommands::IsCommand(const FString& CommandType) const
{
    return IsGenericDataCommand(CommandType) || CommandType == TEXT("ue_asset_list");
}

FString FUEDataCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (!BridgeCommands.IsValid())
    {
        return MakeErrorResponse(TEXT("[ue_data] bridge commands not initialised"));
    }

    if (CommandType == TEXT("ue_asset_list"))
    {
        return HandleListAssets(Params.IsValid() ? Params : MakeShared<FJsonObject>());
    }

    const FString NativeCommand = StripUEPrefix(CommandType);
    const TSharedPtr<FJsonObject> SafeParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();
    NormalizeDataParams(SafeParams);

    const TSharedPtr<FJsonObject> Result = BridgeCommands->HandleCommand(NativeCommand, SafeParams);
    return ForwardBridgeResult(Result, TEXT("ue_data"), CommandType);
}

FString FUEDataCommands::HandleListAssets(const TSharedPtr<FJsonObject>& Params) const
{
    const FString ClassName = Params->GetStringField(TEXT("asset_class"));
    if (ClassName.IsEmpty())
    {
        return MakeErrorResponse(TEXT("[ue_asset_list] AssetClass is required"));
    }
    UClass* BaseClass = FindNativeClass(ClassName);
    if (!BaseClass)
    {
        return MakeErrorResponse(FString::Printf(TEXT("[ue_asset_list] unknown native class '%s'"), *ClassName));
    }

    FString RootPath = Params->GetStringField(TEXT("path")).TrimStartAndEnd();
    RootPath.RemoveFromEnd(TEXT("/"));
    int32 Limit = DefaultAssetListLimit;
    Params->TryGetNumberField(TEXT("limit"), Limit);
    if (Limit <= 0)
    {
        Limit = DefaultAssetListLimit;
    }

    const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.bRecursiveClasses = true;
    if (!RootPath.IsEmpty())
    {
        Filter.PackagePaths.Add(FName(*RootPath));
        Filter.bRecursivePaths = true;
    }

    TArray<TSharedPtr<FJsonValue>> Entries;
    int32 Total = 0;
    auto AddEntry = [&Entries, &Total, Limit](const TSharedPtr<FJsonObject>& Entry)
    {
        ++Total;
        if (Entries.Num() < Limit)
        {
            Entries.Add(MakeShared<FJsonValueObject>(Entry));
        }
    };

    FARFilter InstanceFilter = Filter;
    InstanceFilter.ClassPaths.Add(BaseClass->GetClassPathName());
    TArray<FAssetData> Instances;
    Registry.GetAssets(InstanceFilter, Instances);
    for (const FAssetData& Asset : Instances)
    {
        AddEntry(MakeAssetEntry(Asset, Asset.AssetClassPath.GetAssetName().ToString(), TEXT("asset")));
    }

    // Blueprint subclasses are matched through their native-parent tag so nothing has to be loaded.
    FARFilter BlueprintFilter = Filter;
    BlueprintFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    TArray<FAssetData> Blueprints;
    Registry.GetAssets(BlueprintFilter, Blueprints);
    for (const FAssetData& Asset : Blueprints)
    {
        const FString NativeParent = FPackageName::ExportTextPathToObjectPath(Asset.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath));
        const UClass* NativeClass = NativeParent.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *NativeParent);
        if (NativeClass && NativeClass->IsChildOf(BaseClass))
        {
            AddEntry(MakeAssetEntry(Asset, NativeClass->GetName(), TEXT("blueprint")));
        }
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset_class"), BaseClass->GetPathName());
    Data->SetNumberField(TEXT("count"), Total);
    Data->SetBoolField(TEXT("truncated"), Total > Entries.Num());
    Data->SetArrayField(TEXT("assets"), Entries);
    return MakeSuccessResponse(Data);
}
