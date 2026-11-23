// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentMotionToolset.h"

#include "Toolsets/AscentToolsetUtils.h"

#include "ACFMotionImportLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"

/** The error envelope of this toolset: the standard one plus the "reason" callers branch on. */
static FString MotionError(const FString& Reason, const FString& Message)
{
    TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
    Error->SetStringField(TEXT("status"), TEXT("error"));
    Error->SetBoolField(TEXT("success"), false);
    Error->SetStringField(TEXT("error"), TEXT("[AscentMotion] ") + Message);
    Error->SetStringField(TEXT("reason"), Reason);
    return FAscentToolsetUtils::JsonToString(Error);
}

/** A skeletal mesh or a skeleton, by object path ("/Game/.../SKM_Manny") or by asset name ("SKM_Manny"). */
static UObject* MotionFindBoneAsset(const FString& NameOrPath)
{
    const FString Trimmed = NameOrPath.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return nullptr;
    }

    if (Trimmed.StartsWith(TEXT("/")))
    {
        FString PackagePath = Trimmed;
        PackagePath.Split(TEXT("."), &PackagePath, nullptr);
        if (FindPackage(nullptr, *PackagePath) == nullptr && !FPackageName::DoesPackageExist(PackagePath))
        {
            return nullptr;
        }
        const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
        return StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.bRecursiveClasses = true;
    Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(USkeleton::StaticClass()->GetClassPathName());
    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);

    TArray<FAssetData> Found = Assets.FilterByPredicate([&Trimmed](const FAssetData& Asset)
    {
        return Asset.AssetName.ToString().Equals(Trimmed, ESearchCase::IgnoreCase);
    });
    // A mesh carries its own reference skeleton, which is the one animations are built against, so it wins over a skeleton
    // of the same name.
    const FTopLevelAssetPath MeshClass = USkeletalMesh::StaticClass()->GetClassPathName();
    Found.Sort([&MeshClass](const FAssetData& A, const FAssetData& B)
    {
        const bool bMeshA = A.AssetClassPath == MeshClass;
        const bool bMeshB = B.AssetClassPath == MeshClass;
        return bMeshA != bMeshB ? bMeshA : A.PackageName.ToString() < B.PackageName.ToString();
    });
    return Found.Num() > 0 ? Found[0].GetAsset() : nullptr;
}

/** The reference skeleton of a skeletal mesh or of a skeleton, with the assets it came from. Null for anything else. */
static const FReferenceSkeleton* MotionRefSkeleton(UObject* Asset, USkeletalMesh*& OutMesh, USkeleton*& OutSkeleton)
{
    OutMesh = Cast<USkeletalMesh>(Asset);
    if (OutMesh)
    {
        OutSkeleton = OutMesh->GetSkeleton();
        return &OutMesh->GetRefSkeleton();
    }
    OutSkeleton = Cast<USkeleton>(Asset);
    if (OutSkeleton)
    {
        return &OutSkeleton->GetReferenceSkeleton();
    }
    return nullptr;
}

/** Which asset the bones were read from, the same fields in both bone tools. */
static void MotionAddAssetFields(const TSharedPtr<FJsonObject>& Data, const UObject* Asset, const USkeletalMesh* Mesh, const USkeleton* Skeleton)
{
    Data->SetStringField(TEXT("asset"), Asset->GetOutermost()->GetName());
    Data->SetStringField(TEXT("mesh"), Mesh ? Mesh->GetOutermost()->GetName() : FString());
    Data->SetStringField(TEXT("skeleton"), Skeleton ? Skeleton->GetOutermost()->GetName() : FString());
}

/** True when Bone Index is Root Index or sits below it. */
static bool MotionIsUnderBone(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex, int32 RootIndex)
{
    for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = RefSkeleton.GetParentIndex(Index))
    {
        if (Index == RootIndex)
        {
            return true;
        }
    }
    return false;
}

FString UAscentMotionToolset::ImportMotion(const FString& MotionFile, const FString& Folder, const FString& Name, const FString& TargetMesh)
{
    const FACFMotionImportResult Result = UACFMotionImportLibrary::ImportMotionFile(MotionFile, Folder, Name, TargetMesh);
    if (!Result.bSuccess)
    {
        return MotionError(Result.Reason, Result.Error);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("asset"), Result.Animation->GetOutermost()->GetName());
    Data->SetStringField(TEXT("mesh"), Result.TargetMesh->GetName());
    Data->SetStringField(TEXT("mesh_path"), Result.TargetMesh->GetOutermost()->GetName());
    Data->SetBoolField(TEXT("fallback"), Result.bUsedFallbackMesh);
    Data->SetStringField(TEXT("source"), Result.SourceAnimation->GetOutermost()->GetName());
    Data->SetNumberField(TEXT("frames"), Result.Frames);
    Data->SetNumberField(TEXT("frame_rate"), Result.FrameRate);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentMotionToolset::GetBoneHierarchy(const FString& Asset, const FString& Root, bool bIncludeTransforms)
{
    UObject* Resolved = MotionFindBoneAsset(Asset);
    if (!Resolved)
    {
        return MotionError(TEXT("asset_missing"), FString::Printf(TEXT("No skeletal mesh or skeleton named %s."), *Asset));
    }

    USkeletalMesh* Mesh = nullptr;
    USkeleton* Skeleton = nullptr;
    const FReferenceSkeleton* RefSkeleton = MotionRefSkeleton(Resolved, Mesh, Skeleton);
    if (!RefSkeleton)
    {
        return MotionError(TEXT("skeleton_unsupported"), FString::Printf(TEXT("%s is a %s, not a skeletal mesh or a skeleton."),
            *Resolved->GetName(), *Resolved->GetClass()->GetName()));
    }

    const int32 BoneCount = RefSkeleton->GetNum();
    int32 RootIndex = INDEX_NONE;
    const FString RootName = Root.TrimStartAndEnd();
    if (!RootName.IsEmpty())
    {
        RootIndex = RefSkeleton->FindBoneIndex(FName(*RootName));
        if (RootIndex == INDEX_NONE)
        {
            return MotionError(TEXT("bone_missing"), FString::Printf(TEXT("%s has no bone named %s."), *Resolved->GetName(), *RootName));
        }
    }

    // A reference skeleton always lists a parent before its children, so one pass is enough for depths and child counts.
    TArray<int32> Depths;
    Depths.SetNumZeroed(BoneCount);
    TArray<int32> ChildCounts;
    ChildCounts.SetNumZeroed(BoneCount);
    for (int32 Index = 0; Index < BoneCount; ++Index)
    {
        const int32 Parent = RefSkeleton->GetParentIndex(Index);
        if (Parent == INDEX_NONE)
        {
            continue;
        }
        Depths[Index] = Depths[Parent] + 1;
        ++ChildCounts[Parent];
    }

    const int32 RootDepth = RootIndex == INDEX_NONE ? 0 : Depths[RootIndex];
    const TArray<FTransform>& RefPose = RefSkeleton->GetRefBonePose();
    TArray<TSharedPtr<FJsonValue>> Bones;
    for (int32 Index = 0; Index < BoneCount; ++Index)
    {
        if (RootIndex != INDEX_NONE && !MotionIsUnderBone(*RefSkeleton, Index, RootIndex))
        {
            continue;
        }
        const int32 Parent = RefSkeleton->GetParentIndex(Index);
        TSharedPtr<FJsonObject> Bone = MakeShared<FJsonObject>();
        Bone->SetNumberField(TEXT("index"), Index);
        Bone->SetStringField(TEXT("name"), RefSkeleton->GetBoneName(Index).ToString());
        Bone->SetStringField(TEXT("parent"), Parent == INDEX_NONE ? FString() : RefSkeleton->GetBoneName(Parent).ToString());
        Bone->SetNumberField(TEXT("parent_index"), Parent);
        Bone->SetNumberField(TEXT("depth"), Depths[Index] - RootDepth);
        Bone->SetNumberField(TEXT("children"), ChildCounts[Index]);
        if (bIncludeTransforms && RefPose.IsValidIndex(Index))
        {
            const FTransform& Local = RefPose[Index];
            Bone->SetStringField(TEXT("location"), Local.GetLocation().ToString());
            Bone->SetStringField(TEXT("rotation"), Local.GetRotation().Rotator().ToString());
            Bone->SetStringField(TEXT("scale"), Local.GetScale3D().ToString());
        }
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    MotionAddAssetFields(Data, Resolved, Mesh, Skeleton);
    Data->SetStringField(TEXT("root"), RootName);
    Data->SetNumberField(TEXT("bone_count"), Bones.Num());
    Data->SetNumberField(TEXT("skeleton_bones"), BoneCount);
    Data->SetArrayField(TEXT("bones"), Bones);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentMotionToolset::GetBoneParents(const FString& Asset, const FString& Bones)
{
    UObject* Resolved = MotionFindBoneAsset(Asset);
    if (!Resolved)
    {
        return MotionError(TEXT("asset_missing"), FString::Printf(TEXT("No skeletal mesh or skeleton named %s."), *Asset));
    }

    USkeletalMesh* Mesh = nullptr;
    USkeleton* Skeleton = nullptr;
    const FReferenceSkeleton* RefSkeleton = MotionRefSkeleton(Resolved, Mesh, Skeleton);
    if (!RefSkeleton)
    {
        return MotionError(TEXT("skeleton_unsupported"), FString::Printf(TEXT("%s is a %s, not a skeletal mesh or a skeleton."),
            *Resolved->GetName(), *Resolved->GetClass()->GetName()));
    }

    TArray<FString> Requested;
    TArray<TSharedPtr<FJsonValue>> Parsed;
    if (FAscentToolsetUtils::ParseArray(Bones, Parsed))
    {
        for (const TSharedPtr<FJsonValue>& Value : Parsed)
        {
            Requested.Add(Value->AsString());
        }
    }
    else
    {
        Bones.ParseIntoArray(Requested, TEXT(","));
    }
    for (FString& Name : Requested)
    {
        Name.TrimStartAndEndInline();
    }
    Requested.RemoveAll([](const FString& Name)
    {
        return Name.IsEmpty();
    });

    const int32 BoneCount = RefSkeleton->GetNum();
    TArray<int32> Indices;
    TArray<TSharedPtr<FJsonValue>> Missing;
    if (Requested.Num() == 0)
    {
        for (int32 Index = 0; Index < BoneCount; ++Index)
        {
            Indices.Add(Index);
        }
    }
    else
    {
        for (const FString& Name : Requested)
        {
            const int32 Index = RefSkeleton->FindBoneIndex(FName(*Name));
            if (Index == INDEX_NONE)
            {
                Missing.Add(MakeShared<FJsonValueString>(Name));
                continue;
            }
            Indices.Add(Index);
        }
    }

    TArray<TSharedPtr<FJsonValue>> Entries;
    for (const int32 Index : Indices)
    {
        const int32 Parent = RefSkeleton->GetParentIndex(Index);
        TArray<TSharedPtr<FJsonValue>> Chain;
        for (int32 Ancestor = Parent; Ancestor != INDEX_NONE; Ancestor = RefSkeleton->GetParentIndex(Ancestor))
        {
            // Walking up yields the parent first; the root belongs first in the chain.
            Chain.Insert(MakeShared<FJsonValueString>(RefSkeleton->GetBoneName(Ancestor).ToString()), 0);
        }
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), RefSkeleton->GetBoneName(Index).ToString());
        Entry->SetNumberField(TEXT("index"), Index);
        Entry->SetStringField(TEXT("parent"), Parent == INDEX_NONE ? FString() : RefSkeleton->GetBoneName(Parent).ToString());
        Entry->SetNumberField(TEXT("parent_index"), Parent);
        Entry->SetNumberField(TEXT("depth"), Chain.Num());
        Entry->SetArrayField(TEXT("chain"), Chain);
        Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    MotionAddAssetFields(Data, Resolved, Mesh, Skeleton);
    Data->SetNumberField(TEXT("bone_count"), Entries.Num());
    Data->SetNumberField(TEXT("skeleton_bones"), BoneCount);
    Data->SetArrayField(TEXT("bones"), Entries);
    Data->SetArrayField(TEXT("missing"), Missing);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}
