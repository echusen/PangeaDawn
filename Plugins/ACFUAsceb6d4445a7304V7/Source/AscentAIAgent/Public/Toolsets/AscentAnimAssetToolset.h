// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AscentAnimAssetToolset.generated.h"

class UAnimMontage;
class UAnimSequence;
class UAnimSequenceBase;

/// Animation asset tools for Anim Montages and Anim Sequences, which Epic's toolsets do not cover: notifies and notify
/// states with their instanced objects (for example a Motion Warping notify's RootMotionModifier), notify tracks, sync
/// markers, curves, montage sections, slots and segments, root motion and blend settings, and new montages.
/// Times are in seconds from the start of the asset. Notify indices come from Describe and change when notifies are
/// added or removed, so read them again after a change. Changes are not saved: save with AssetTools.save_assets.
/// Property values use Unreal import-text form; a nested struct or instanced object takes a JSON object of its members,
/// and an instanced object is replaced by passing {"class": "<class name or path>", ...members}.
/// All tools return a JSON string: {"status":"success","success":true,...}
/// on success or {"status":"error","success":false,"error":...} on failure.
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentAnimAssetToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Describes an Anim Montage or Anim Sequence: length, frame rate, skeleton, rate scale, root motion, notify tracks,
     * every notify with its index, time, duration and class, sync markers, curves, and for montages the blend settings,
     * sections and slot segments. Also lists the names SetProperties accepts.
     * @param Asset Anim Montage or Anim Sequence, e.g. "/Game/Characters/Hero/AM_Vault.AM_Vault".
     * @return JSON with "notifies", "notify_tracks", "curves", "sync_markers", and "sections" / "slots" for montages.
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "AscentAnimAsset")
    static FString Describe(UAnimSequenceBase* Asset);

    /**
     * Reads one notify completely: its trigger settings and every property of its notify or notify state object,
     * including instanced sub-objects such as a Motion Warping RootMotionModifier and its warp settings.
     * @param Asset Anim Montage or Anim Sequence.
     * @param NotifyIndex Index from Describe.
     * @return JSON with the notify summary, "event" (trigger settings, the keys SetNotify accepts) and "object"
     *   (the notify object's properties, nested objects with their "class").
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "AscentAnimAsset")
    static FString GetNotify(UAnimSequenceBase* Asset, int32 NotifyIndex);

    /**
     * Finds notifies across the animation assets in a folder, by notify class and/or name.
     * @param Folder Content folder searched recursively, e.g. "/Game/Characters".
     * @param NotifyClass Notify or notify state class name or path, e.g. "AnimNotifyState_MotionWarping". Pass an empty string for any class.
     * @param NameContains Text the notify name or class name must contain. Pass an empty string to skip.
     * @return JSON with "matches" (asset, index, name, class, time, duration), "assets_scanned" and "truncated".
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "AscentAnimAsset")
    static FString FindNotifies(const FString& Folder, const FString& NotifyClass, const FString& NameContains);

    /**
     * Reads the keys of a float curve.
     * @param Asset Anim Montage or Anim Sequence.
     * @param CurveName Curve name from Describe.
     * @return JSON with "times" and "values".
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "AscentAnimAsset")
    static FString GetCurveKeys(UAnimSequenceBase* Asset, const FString& CurveName);

    /**
     * Adds a notify, a notify state, or a named skeleton notify.
     * @param Asset Anim Montage or Anim Sequence.
     * @param NotifyClass Notify or notify state class name, e.g. "AnimNotify_PlaySound" or "AnimNotifyState_MotionWarping",
     *   or a Blueprint notify asset path. Pass an empty string for a skeleton notify named NotifyName.
     * @param NotifyName Name of a skeleton notify, or an optional display name for a class notify. Pass an empty string to skip.
     * @param Time Start time in seconds.
     * @param Duration Length in seconds for a notify state; ignored for a notify.
     * @param TrackName Notify track; created when missing. Pass an empty string for the first track.
     * @param PropertiesJson Optional JSON object of properties to set, the same keys as SetNotify, e.g.
     *   {"RootMotionModifier": {"class": "RootMotionModifier_SkewWarp", "WarpTargetName": "ReachTarget"}}. Pass an empty string to skip.
     * @return JSON with the new notify's index and details.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString AddNotify(UAnimSequenceBase* Asset, const FString& NotifyClass, const FString& NotifyName, float Time,
        float Duration, const FString& TrackName, const FString& PropertiesJson);

    /**
     * Edits a notify: its time, duration, track, trigger settings, and the properties of its notify object.
     * @param Asset Anim Montage or Anim Sequence.
     * @param NotifyIndex Index from Describe.
     * @param Time New start time in seconds. Pass -1 to keep it.
     * @param Duration New notify state length in seconds. Pass -1 to keep it.
     * @param TrackName Notify track to move it to; created when missing. Pass an empty string to keep it.
     * @param PropertiesJson Optional JSON object. Keys are trigger settings from GetNotify "event" (e.g. "NotifyTriggerChance",
     *   "MontageTickType", "bTriggerOnDedicatedServer") or properties of the notify object from GetNotify "object"; dotted
     *   paths reach nested members, e.g. {"RootMotionModifier.bIgnoreZAxis": true}. Pass an empty string to skip.
     * @return JSON with the notify's new index and details, and the properties applied.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString SetNotify(UAnimSequenceBase* Asset, int32 NotifyIndex, float Time, float Duration, const FString& TrackName,
        const FString& PropertiesJson);

    /**
     * Removes notifies.
     * @param Asset Anim Montage or Anim Sequence.
     * @param NotifyIndices Indices from Describe, e.g. [0, 3].
     * @return JSON with the number removed and the notifies left.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString RemoveNotifies(UAnimSequenceBase* Asset, const TArray<int32>& NotifyIndices);

    /**
     * Adds, removes or renames a notify track.
     * @param Asset Anim Montage or Anim Sequence.
     * @param Action "add", "remove" (the track must hold no notifies or sync markers), or "rename".
     * @param TrackName Track to add, remove or rename.
     * @param NewName New name for "rename". Pass an empty string otherwise.
     * @return JSON with the notify tracks after the edit.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString EditNotifyTrack(UAnimSequenceBase* Asset, const FString& Action, const FString& TrackName, const FString& NewName);

    /**
     * Adds or removes sync markers on an Anim Sequence.
     * @param Sequence Anim Sequence.
     * @param Action "add" or "remove" (every marker with this name).
     * @param MarkerName Marker name, e.g. "LeftFoot".
     * @param Time Time in seconds for "add".
     * @param TrackName Notify track for "add"; created when missing. Pass an empty string for the first track.
     * @return JSON with the sync markers after the edit.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString EditSyncMarker(UAnimSequence* Sequence, const FString& Action, const FString& MarkerName, float Time, const FString& TrackName);

    /**
     * Adds, removes or keys a float curve.
     * @param Asset Anim Montage or Anim Sequence.
     * @param Action "add", "remove", or "set_keys" (adds the keys, creating the curve when missing).
     * @param CurveName Curve name.
     * @param Times Key times in seconds for "set_keys". Pass an empty array otherwise.
     * @param Values Key values for "set_keys", one per time. Pass an empty array otherwise.
     * @return JSON with the curves after the edit.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString EditCurve(UAnimSequenceBase* Asset, const FString& Action, const FString& CurveName, const TArray<float>& Times,
        const TArray<float>& Values);

    /**
     * Sets properties of the asset itself: rate scale, looping, root motion ("bEnableRootMotion", "RootMotionRootLock",
     * "bForceRootLock"), montage blending ("BlendIn", "BlendOut", "BlendOutTriggerTime", "bEnableAutoBlendOut"), sync group,
     * additive settings, and any other name Describe lists in "settable_properties".
     * @param Asset Anim Montage or Anim Sequence.
     * @param PropertiesJson JSON object, e.g. {"RateScale": 1.2, "BlendIn": {"BlendTime": 0.15}, "RootMotionRootLock": "RRL_Zero"}.
     * @return JSON with the properties applied.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString SetProperties(UAnimSequenceBase* Asset, const FString& PropertiesJson);

    /**
     * Edits montage sections.
     * @param Montage Anim Montage.
     * @param Action "add", "remove", "rename", "move" (new start time), or "link" (the section that plays next).
     * @param SectionName Section to add or edit.
     * @param Time Start time in seconds for "add" and "move".
     * @param Other New name for "rename", next section for "link" (an empty string clears it). Pass an empty string otherwise.
     * @return JSON with the sections after the edit.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString EditMontageSection(UAnimMontage* Montage, const FString& Action, const FString& SectionName, float Time, const FString& Other);

    /**
     * Edits montage slots and the animation segments they play.
     * @param Montage Anim Montage.
     * @param Action "add_slot", "rename_slot", "remove_slot", "add_segment", "set_segment", or "remove_segment".
     * @param SlotIndex Slot index from Describe. Ignored by "add_slot".
     * @param SegmentIndex Segment index for "set_segment" and "remove_segment". Pass -1 otherwise.
     * @param OptionsJson Optional JSON object. "slot_name" for "add_slot" and "rename_slot", e.g. "DefaultGroup.UpperBody".
     *   For segments: "animation" (Anim Sequence path, required by "add_segment"), "start_pos" (montage time, default: end of
     *   the slot), "anim_start_time", "anim_end_time", "play_rate", "looping_count". Pass an empty string to skip.
     * @return JSON with the slots and the montage length after the edit.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString EditMontageSlot(UAnimMontage* Montage, const FString& Action, int32 SlotIndex, int32 SegmentIndex, const FString& OptionsJson);

    /**
     * Creates an Anim Montage that plays an Anim Sequence.
     * @param Source Anim Sequence the montage plays.
     * @param AssetPath New montage path, e.g. "/Game/Characters/Hero/AM_Vault".
     * @param SlotName Slot of the montage track, e.g. "DefaultGroup.UpperBody". Pass an empty string for the default slot.
     * @return JSON with the new montage's path, length and slots.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentAnimAsset")
    static FString CreateMontage(UAnimSequence* Source, const FString& AssetPath, const FString& SlotName);
};
