// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AscentMotionToolset.generated.h"

/// Motion import tools: bring a body animation generated outside Unreal (a BVH file, for example from Kimodo) into the
/// project and retarget it onto a character, with the editor open.
/// All tools return a JSON string: {"status":"success","success":true,...}
/// on success or {"status":"error","success":false,"error":...,"reason":...} on failure.
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentMotionToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Imports a BVH motion and retargets it onto a character's skeletal mesh, then saves every asset.
     * The BVH skeleton (Kimodo SOMA by default) gets its own skeleton, preview mesh and IK Rig under <Folder>/Rigs,
     * reused on later imports; the raw motion is kept under <Folder>/Source; the target gets an IK Rig and an IK
     * Retargeter per skeleton under <Folder>/Rigs. An existing animation with the same name is replaced.
     * @param MotionFile Absolute path of the .bvh file on disk.
     * @param Folder Content folder for the result, e.g. "/Game/Yarin/Animations". Empty uses the Yarin content folder + "/Animations".
     * @param Name Name of the retargeted Anim Sequence, e.g. "Wave".
     * @param TargetMesh Skeletal mesh asset name (e.g. "SKM_Manny") or object path. Empty uses SKM_Manny. When the
     *   named mesh does not exist, SKM_Manny is used and "fallback" is true.
     * @return JSON with "asset" (package path of the new Anim Sequence), "mesh" (the mesh it was made for),
     *   "fallback", "source" (raw motion asset) and "frames". Errors carry a "reason": "motion_unreadable",
     *   "mannequin_missing", "skeleton_unsupported", "create_failed" or "retarget_failed".
     */
    UFUNCTION(meta = (AICallable), Category = "AscentMotion")
    static FString ImportMotion(const FString& MotionFile, const FString& Folder, const FString& Name, const FString& TargetMesh);

    /**
     * Reads a whole bone hierarchy in one call, so an import or a retarget does not need a call per bone.
     * Bones come in skeleton order, every parent before its children.
     * @param Asset Skeletal mesh or skeleton, by asset name ("SKM_Manny", "SK_Mannequin") or by object path.
     * @param Root Bone to start from: only its subtree is listed, depths counted from it. Pass an empty string for the
     *   whole skeleton.
     * @param bIncludeTransforms Adds each bone's reference pose relative to its parent ("location", "rotation", "scale").
     * @return JSON with "bones" (index, name, parent, parent_index, depth, children), "bone_count" (bones listed),
     *   "skeleton_bones" (bones on the skeleton), "asset", "mesh", "skeleton" and "root". Errors carry a "reason":
     *   "asset_missing", "skeleton_unsupported" or "bone_missing".
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "AscentMotion")
    static FString GetBoneHierarchy(const FString& Asset, const FString& Root, bool bIncludeTransforms);

    /**
     * Reads the parent chain of many bones in one call, instead of one call per bone.
     * @param Asset Skeletal mesh or skeleton, by asset name ("SKM_Manny", "SK_Mannequin") or by object path.
     * @param Bones Bone names, as a JSON array ["hand_l","hand_r"] or comma separated ("hand_l, hand_r").
     *   Pass an empty string for every bone of the skeleton.
     * @return JSON with "bones" (name as the skeleton spells it, index, parent, parent_index, depth, and "chain", the
     *   ancestors from the root down to the parent), "missing" (names not on the skeleton), "bone_count" and the same
     *   "asset", "mesh" and "skeleton" as Get Bone Hierarchy. Errors carry a "reason": "asset_missing" or
     *   "skeleton_unsupported".
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "AscentMotion")
    static FString GetBoneParents(const FString& Asset, const FString& Bones);
};
