// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Toolsets/AscentAnimAssetToolset.h"

#include "Toolsets/AscentToolsetUtils.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AnimationBlueprintLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Factories/AnimMontageFactory.h"
#include "IAssetTools.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "AscentAnimAssetToolset"

namespace
{
    // Deep enough for a notify state -> RootMotionModifier -> its own instanced settings.
    constexpr int32 ObjectDepth = 3;
    constexpr int32 MaxScannedAssets = 400;
    constexpr int32 MaxMatches = 300;

    FString Fail(const FString& Message)
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("[AscentAnimAsset] ") + Message);
    }

    FString EnumName(const UEnum* Enum, int64 Value)
    {
        return Enum ? Enum->GetNameStringByValue(Value) : FString::FromInt(static_cast<int32>(Value));
    }

    // FAnimNotifyEvent is not exported from the Engine module, so its reflection data is reached through the Notifies array.
    FProperty* NotifiesProperty()
    {
        return FindFProperty<FProperty>(UAnimSequenceBase::StaticClass(), GET_MEMBER_NAME_CHECKED(UAnimSequenceBase, Notifies));
    }

    const UScriptStruct* NotifyEventStruct()
    {
        const FArrayProperty* Array = CastField<FArrayProperty>(NotifiesProperty());
        const FStructProperty* Inner = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
        return Inner ? Inner->Struct.Get() : nullptr;
    }

    bool IsInstanced(const FProperty* Property, const UObject* Value, const UObject* Owner)
    {
        return Property->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance) || (Value && Value->GetOuter() == Owner);
    }

    TSharedRef<FJsonObject> ExportProperties(const UStruct* Struct, void* Container, UObject* Owner, int32 Depth);

    TSharedPtr<FJsonValue> ExportValue(const FProperty* Property, void* Value, UObject* Owner, int32 Depth)
    {
        if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
        {
            return MakeShared<FJsonValueBoolean>(Bool->GetPropertyValue(Value));
        }
        if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property); Numeric && !Numeric->IsEnum())
        {
            return MakeShared<FJsonValueNumber>(Numeric->IsFloatingPoint()
                ? Numeric->GetFloatingPointPropertyValue(Value)
                : static_cast<double>(Numeric->GetSignedIntPropertyValue(Value)));
        }
        if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
        {
            UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
            if (Object && Depth > 0 && IsInstanced(Property, Object, Owner))
            {
                const TSharedRef<FJsonObject> Nested = ExportProperties(Object->GetClass(), Object, Object, Depth - 1);
                Nested->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
                return MakeShared<FJsonValueObject>(Nested);
            }
            return MakeShared<FJsonValueString>(Object ? Object->GetPathName() : FString());
        }
        FString Text;
        // Delta == Data forces the export; a null Delta compares against zero and skips default-valued enums.
        Property->ExportText_Direct(Text, Value, Value, Owner, PPF_None);
        return MakeShared<FJsonValueString>(Text);
    }

    TSharedRef<FJsonObject> ExportProperties(const UStruct* Struct, void* Container, UObject* Owner, int32 Depth)
    {
        const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
        for (TFieldIterator<FProperty> It(Struct); It; ++It)
        {
            const FProperty* Property = *It;
            if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
            {
                continue;
            }
            Out->SetField(Property->GetName(), ExportValue(Property, Property->ContainerPtrToValuePtr<void>(Container), Owner, Depth));
        }
        return Out;
    }

    UClass* LoadClassQuietly(const FString& Path)
    {
        return LoadObject<UClass>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
    }

    /** A class by path ("/Script/Module.Class", a Blueprint asset path) or by name ("AnimNotifyState_MotionWarping"). */
    UClass* ResolveClass(const FString& Name, const UClass* Base)
    {
        const FString Trimmed = Name.TrimStartAndEnd();
        if (Trimmed.IsEmpty())
        {
            return nullptr;
        }
        UClass* Found = nullptr;
        if (Trimmed.StartsWith(TEXT("/")))
        {
            FString ObjectPath = Trimmed;
            if (!ObjectPath.Contains(TEXT(".")))
            {
                ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
            }
            Found = LoadClassQuietly(ObjectPath);
            if (!Found)
            {
                if (const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
                {
                    Found = Blueprint->GeneratedClass;
                }
            }
            if (!Found && !ObjectPath.EndsWith(TEXT("_C")))
            {
                Found = LoadClassQuietly(ObjectPath + TEXT("_C"));
            }
        }
        else
        {
            const FString Short = Trimmed.StartsWith(TEXT("U")) && Trimmed.Len() > 1 && FChar::IsUpper(Trimmed[1]) ? Trimmed.Mid(1) : Trimmed;
            for (TObjectIterator<UClass> It; It; ++It)
            {
                UClass* Class = *It;
                if (!Class->IsChildOf(Base) || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
                {
                    continue;
                }
                const FString ClassName = Class->GetName();
                if (ClassName.Equals(Trimmed, ESearchCase::IgnoreCase) || ClassName.Equals(Short, ESearchCase::IgnoreCase)
                    || ClassName.Equals(Trimmed + TEXT("_C"), ESearchCase::IgnoreCase))
                {
                    Found = Class;
                    break;
                }
            }
        }
        return Found && Found->IsChildOf(Base) ? Found : nullptr;
    }

    FString ValueText(const TSharedPtr<FJsonValue>& Value)
    {
        switch (Value->Type)
        {
        case EJson::Boolean:
            return Value->AsBool() ? TEXT("True") : TEXT("False");
        case EJson::Number:
        {
            const double Number = Value->AsNumber();
            const double Rounded = FMath::RoundToDouble(Number);
            return Number == Rounded && FMath::Abs(Number) < 1e15
                ? FString::Printf(TEXT("%lld"), static_cast<int64>(Rounded))
                : FString::SanitizeFloat(Number);
        }
        case EJson::String:
            return Value->AsString();
        default:
            return FString();
        }
    }

    /** Collects every object a property write touches, so each gets one Modify / PreEditChange and one PostEditChangeProperty. */
    struct FPropertyWrites
    {
        struct FChange
        {
            UObject* Owner = nullptr;
            FProperty* Member = nullptr;
            FProperty* Leaf = nullptr;
        };
        TArray<FChange> Changes;
        TArray<FString> Applied;

        void Touch(UObject* Owner, FProperty* Member, FProperty* Leaf)
        {
            if (Changes.ContainsByPredicate([&](const FChange& Change) { return Change.Owner == Owner && Change.Member == Member; }))
            {
                return;
            }
            Owner->Modify();
            Owner->PreEditChange(Member);
            Changes.Add({Owner, Member, Leaf});
        }

        void Finish()
        {
            for (const FChange& Change : Changes)
            {
                FPropertyChangedEvent Event(Change.Leaf, EPropertyChangeType::ValueSet);
                Event.SetActiveMemberProperty(Change.Member);
                Change.Owner->PostEditChangeProperty(Event);
            }
            Changes.Reset();
        }
    };

    /**
     * Writes Value at a dotted Path under Container (a Struct instance owned by Owner). Member is the property of Owner the
     * write sits under, for the edit notifications; nullptr starts from Owner's own properties.
     */
    bool WriteProperty(UObject* Owner, const UStruct* Struct, void* Container, const FString& Path, const TSharedPtr<FJsonValue>& Value,
        FProperty* Member, FPropertyWrites& Writes, FString& OutError)
    {
        FString Head;
        FString Rest;
        if (!Path.Split(TEXT("."), &Head, &Rest))
        {
            Head = Path;
            Rest.Reset();
        }
        FProperty* Property = FindFProperty<FProperty>(Struct, FName(*Head));
        if (!Property)
        {
            OutError = FString::Printf(TEXT("%s has no property '%s'"), *Struct->GetName(), *Head);
            return false;
        }
        FProperty* OwnerMember = Member ? Member : Property;
        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
        FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
        const FStructProperty* StructProperty = CastField<FStructProperty>(Property);

        if (!Rest.IsEmpty())
        {
            if (StructProperty)
            {
                return WriteProperty(Owner, StructProperty->Struct, ValuePtr, Rest, Value, OwnerMember, Writes, OutError);
            }
            if (ObjectProperty)
            {
                UObject* Object = ObjectProperty->GetObjectPropertyValue(ValuePtr);
                if (!Object)
                {
                    OutError = FString::Printf(TEXT("'%s' is empty; set it first with {\"class\": ...}"), *Head);
                    return false;
                }
                return WriteProperty(Object, Object->GetClass(), Object, Rest, Value, nullptr, Writes, OutError);
            }
            OutError = FString::Printf(TEXT("'%s' has no members"), *Head);
            return false;
        }

        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Fields = Value->AsObject();
            if (StructProperty)
            {
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Fields->Values)
                {
                    if (!WriteProperty(Owner, StructProperty->Struct, ValuePtr, Field.Key, Field.Value, OwnerMember, Writes, OutError))
                    {
                        return false;
                    }
                }
                return true;
            }
            if (ObjectProperty)
            {
                UObject* Object = ObjectProperty->GetObjectPropertyValue(ValuePtr);
                FString ClassName;
                if (Fields->TryGetStringField(TEXT("class"), ClassName))
                {
                    UClass* Class = ResolveClass(ClassName, ObjectProperty->PropertyClass);
                    if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
                    {
                        OutError = FString::Printf(TEXT("'%s' takes a non-abstract %s class; '%s' is not one"),
                            *Head, *ObjectProperty->PropertyClass->GetName(), *ClassName);
                        return false;
                    }
                    if (!Object || Object->GetClass() != Class)
                    {
                        Writes.Touch(Owner, OwnerMember, Property);
                        Object = NewObject<UObject>(Owner, Class, NAME_None, RF_Transactional);
                        ObjectProperty->SetObjectPropertyValue(ValuePtr, Object);
                        Writes.Applied.Add(FString::Printf(TEXT("%s = %s"), *Head, *Class->GetName()));
                    }
                }
                if (!Object)
                {
                    OutError = FString::Printf(TEXT("'%s' is empty; pass {\"class\": ...} to create it"), *Head);
                    return false;
                }
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Fields->Values)
                {
                    if (Field.Key == TEXT("class"))
                    {
                        continue;
                    }
                    if (!WriteProperty(Object, Object->GetClass(), Object, Field.Key, Field.Value, nullptr, Writes, OutError))
                    {
                        return false;
                    }
                }
                return true;
            }
            OutError = FString::Printf(TEXT("'%s' does not take a JSON object; pass its value in Unreal import-text form"), *Head);
            return false;
        }
        if (Value->Type == EJson::Array || Value->Type == EJson::Null)
        {
            OutError = FString::Printf(TEXT("'%s': pass arrays and empty values in Unreal import-text form, e.g. \"(A,B)\" or \"()\""), *Head);
            return false;
        }

        const FString Text = ValueText(Value);
        Writes.Touch(Owner, OwnerMember, Property);
        FOutputDeviceNull Ignored;
        if (!Property->ImportText_Direct(*Text, ValuePtr, Owner, PPF_None, &Ignored))
        {
            OutError = FString::Printf(TEXT("'%s' does not accept '%s'"), *Head, *Text);
            return false;
        }
        Writes.Applied.Add(FString::Printf(TEXT("%s = %s"), *Path, *Text));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FString& Value : Values)
        {
            Out.Add(MakeShared<FJsonValueString>(Value));
        }
        return Out;
    }

    FString AppliedSuffix(const FPropertyWrites& Writes)
    {
        return Writes.Applied.IsEmpty() ? FString() : TEXT(". Applied before the error: ") + FString::Join(Writes.Applied, TEXT(", "));
    }

    UObject* NotifyObject(const FAnimNotifyEvent& Event)
    {
        return Event.NotifyStateClass ? static_cast<UObject*>(Event.NotifyStateClass.Get()) : static_cast<UObject*>(Event.Notify.Get());
    }

    FString NotifyDisplayName(const FAnimNotifyEvent& Event)
    {
        if (!Event.NotifyName.IsNone())
        {
            return Event.NotifyName.ToString();
        }
        const UObject* Object = NotifyObject(Event);
        if (!Object)
        {
            return FString();
        }
        FString Name = Object->GetClass()->GetName();
        Name.RemoveFromEnd(TEXT("_C"));
        if (!Name.RemoveFromStart(TEXT("AnimNotifyState_")))
        {
            Name.RemoveFromStart(TEXT("AnimNotify_"));
        }
        return Name;
    }

    FString TrackNameAt(const UAnimSequenceBase* Asset, int32 TrackIndex)
    {
        return Asset->AnimNotifyTracks.IsValidIndex(TrackIndex) ? Asset->AnimNotifyTracks[TrackIndex].TrackName.ToString() : FString();
    }

    TSharedRef<FJsonObject> NotifySummary(const UAnimSequenceBase* Asset, int32 Index)
    {
        const FAnimNotifyEvent& Event = Asset->Notifies[Index];
        const UObject* Object = NotifyObject(Event);
        const TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetNumberField(TEXT("index"), Index);
        Out->SetStringField(TEXT("name"), NotifyDisplayName(Event));
        Out->SetStringField(TEXT("kind"), Event.NotifyStateClass ? TEXT("notify_state") : Event.Notify ? TEXT("notify") : TEXT("skeleton_notify"));
        Out->SetStringField(TEXT("class"), Object ? Object->GetClass()->GetPathName() : FString());
        Out->SetNumberField(TEXT("time"), Event.GetTime());
        if (Event.NotifyStateClass)
        {
            Out->SetNumberField(TEXT("duration"), Event.GetDuration());
            Out->SetNumberField(TEXT("end_time"), Event.GetTime() + Event.GetDuration());
        }
        Out->SetNumberField(TEXT("track_index"), Event.TrackIndex);
        Out->SetStringField(TEXT("track"), TrackNameAt(Asset, Event.TrackIndex));
        if (Asset->IsA<UAnimMontage>())
        {
            Out->SetNumberField(TEXT("slot_index"), Event.GetSlotIndex());
        }
        return Out;
    }

    int32 FindNotifyByGuid(const UAnimSequenceBase* Asset, const FGuid& Guid)
    {
        return Asset->Notifies.IndexOfByPredicate([&](const FAnimNotifyEvent& Event) { return Event.Guid == Guid; });
    }

    int32 FindTrack(const UAnimSequenceBase* Asset, FName Name)
    {
        return Asset->AnimNotifyTracks.IndexOfByPredicate([&](const FAnimNotifyTrack& Track) { return Track.TrackName == Name; });
    }

    /** The track named TrackName, created when missing; the first track (created as "1") for an empty name. Asset must be Modify()'d. */
    int32 EnsureTrack(UAnimSequenceBase* Asset, const FString& TrackName)
    {
        const FName Name = !TrackName.IsEmpty() ? FName(*TrackName)
            : Asset->AnimNotifyTracks.Num() ? Asset->AnimNotifyTracks[0].TrackName : FName(TEXT("1"));
        int32 Index = FindTrack(Asset, Name);
        if (Index == INDEX_NONE)
        {
            FAnimNotifyTrack Track;
            Track.TrackName = Name;
            Track.TrackColor = FLinearColor::White;
            Index = Asset->AnimNotifyTracks.Add(Track);
        }
        return Index;
    }

    TArray<TSharedPtr<FJsonValue>> TrackList(const UAnimSequenceBase* Asset)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (int32 Index = 0; Index < Asset->AnimNotifyTracks.Num(); ++Index)
        {
            const TSharedRef<FJsonObject> Track = MakeShared<FJsonObject>();
            Track->SetNumberField(TEXT("index"), Index);
            Track->SetStringField(TEXT("name"), Asset->AnimNotifyTracks[Index].TrackName.ToString());
            Out.Add(MakeShared<FJsonValueObject>(Track));
        }
        return Out;
    }

    TArray<TSharedPtr<FJsonValue>> CurveList(const UAnimSequenceBase* Asset)
    {
        TArray<FName> Names;
        UAnimationBlueprintLibrary::GetAnimationCurveNames(Asset, ERawCurveTrackTypes::RCT_Float, Names);
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FName& Name : Names)
        {
            Out.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        return Out;
    }

    TArray<TSharedPtr<FJsonValue>> SyncMarkerList(const UAnimSequence* Sequence)
    {
        TArray<FAnimSyncMarker> Markers;
        UAnimationBlueprintLibrary::GetAnimationSyncMarkers(Sequence, Markers);
        TArray<TSharedPtr<FJsonValue>> Out;
        for (const FAnimSyncMarker& Marker : Markers)
        {
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Marker.MarkerName.ToString());
            Entry->SetNumberField(TEXT("time"), Marker.Time);
            Entry->SetStringField(TEXT("track"), TrackNameAt(Sequence, Marker.TrackIndex));
            Out.Add(MakeShared<FJsonValueObject>(Entry));
        }
        return Out;
    }

    TArray<TSharedPtr<FJsonValue>> SectionList(const UAnimMontage* Montage)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (int32 Index = 0; Index < Montage->CompositeSections.Num(); ++Index)
        {
            const FCompositeSection& Section = Montage->CompositeSections[Index];
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("index"), Index);
            Entry->SetStringField(TEXT("name"), Section.SectionName.ToString());
            Entry->SetNumberField(TEXT("time"), Section.GetTime());
            Entry->SetStringField(TEXT("next"), Section.NextSectionName.IsNone() ? FString() : Section.NextSectionName.ToString());
            Out.Add(MakeShared<FJsonValueObject>(Entry));
        }
        return Out;
    }

    TArray<TSharedPtr<FJsonValue>> SlotList(const UAnimMontage* Montage)
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (int32 SlotIndex = 0; SlotIndex < Montage->SlotAnimTracks.Num(); ++SlotIndex)
        {
            const FSlotAnimationTrack& Slot = Montage->SlotAnimTracks[SlotIndex];
            TArray<TSharedPtr<FJsonValue>> Segments;
            for (int32 SegmentIndex = 0; SegmentIndex < Slot.AnimTrack.AnimSegments.Num(); ++SegmentIndex)
            {
                const FAnimSegment& Segment = Slot.AnimTrack.AnimSegments[SegmentIndex];
                const UAnimSequenceBase* Animation = Segment.GetAnimReference();
                const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetNumberField(TEXT("index"), SegmentIndex);
                Entry->SetStringField(TEXT("animation"), Animation ? Animation->GetPathName() : FString());
                Entry->SetNumberField(TEXT("start_pos"), Segment.StartPos);
                Entry->SetNumberField(TEXT("anim_start_time"), Segment.AnimStartTime);
                Entry->SetNumberField(TEXT("anim_end_time"), Segment.AnimEndTime);
                Entry->SetNumberField(TEXT("play_rate"), Segment.AnimPlayRate);
                Entry->SetNumberField(TEXT("looping_count"), Segment.LoopingCount);
                Entry->SetNumberField(TEXT("length"), Segment.GetLength());
                Segments.Add(MakeShared<FJsonValueObject>(Entry));
            }
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("index"), SlotIndex);
            Entry->SetStringField(TEXT("slot_name"), Slot.SlotName.ToString());
            Entry->SetArrayField(TEXT("segments"), Segments);
            Out.Add(MakeShared<FJsonValueObject>(Entry));
        }
        return Out;
    }

    void FinishEdit(UAnimSequenceBase* Asset)
    {
        Asset->SortNotifies();
        Asset->RefreshCacheData();
        Asset->PostEditChange();
        Asset->MarkPackageDirty();
    }

    void FinishMontageEdit(UAnimMontage* Montage)
    {
        Montage->SetCompositeLength(Montage->CalculateSequenceLength());
        Montage->UpdateLinkableElements();
        FinishEdit(Montage);
    }

    bool ValidTime(const UAnimSequenceBase* Asset, float Time)
    {
        return Time >= 0.f && Time <= Asset->GetPlayLength() + KINDA_SMALL_NUMBER;
    }

    /** Keys naming a trigger setting go to the notify event, the rest to its notify object. */
    bool WriteNotifyProperties(UAnimSequenceBase* Asset, int32 NotifyIndex, const TSharedPtr<FJsonObject>& Properties,
        FPropertyWrites& Writes, FString& OutError)
    {
        const UScriptStruct* EventStruct = NotifyEventStruct();
        if (!EventStruct)
        {
            OutError = TEXT("The notify layout could not be read");
            return false;
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Properties->Values)
        {
            FAnimNotifyEvent& Event = Asset->Notifies[NotifyIndex];
            FString Head = Field.Key;
            FString Rest;
            Field.Key.Split(TEXT("."), &Head, &Rest);
            if (FindFProperty<FProperty>(EventStruct, FName(*Head)))
            {
                if (!WriteProperty(Asset, EventStruct, &Event, Field.Key, Field.Value, NotifiesProperty(), Writes, OutError))
                {
                    return false;
                }
                continue;
            }
            UObject* Object = NotifyObject(Event);
            if (!Object)
            {
                OutError = FString::Printf(TEXT("'%s' is not a trigger setting, and a skeleton notify has no object properties"), *Field.Key);
                return false;
            }
            if (!WriteProperty(Object, Object->GetClass(), Object, Field.Key, Field.Value, nullptr, Writes, OutError))
            {
                return false;
            }
        }
        return true;
    }

    /** Sets the start (and for a notify state, the end) of a notify, keeping its montage slot. */
    void PlaceNotify(UAnimSequenceBase* Asset, FAnimNotifyEvent& Event, float Time, float Duration)
    {
        const int32 Slot = Event.GetSlotIndex();
        Event.Link(Asset, Time, Slot);
        Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Asset->CalculateOffsetForNotify(Time));
        if (Event.NotifyStateClass)
        {
            Event.SetDuration(Duration);
            Event.EndLink.Link(Asset, Time + Duration, Slot);
            Event.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Asset->CalculateOffsetForNotify(Time + Duration));
        }
    }

    TArray<FString> SettableProperties(const UObject* Asset)
    {
        TArray<FString> Names;
        for (TFieldIterator<FProperty> It(Asset->GetClass()); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_Edit) && !It->HasAnyPropertyFlags(CPF_Deprecated | CPF_EditConst))
            {
                Names.Add(It->GetName());
            }
        }
        return Names;
    }
}

FString UAscentAnimAssetToolset::Describe(UAnimSequenceBase* Asset)
{
    if (!Asset)
    {
        return Fail(TEXT("Asset is required: an Anim Montage or Anim Sequence"));
    }
    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("path"), Asset->GetPathName());
    Data->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
    Data->SetStringField(TEXT("skeleton"), Asset->GetSkeleton() ? Asset->GetSkeleton()->GetPathName() : FString());
    Data->SetNumberField(TEXT("length"), Asset->GetPlayLength());
    Data->SetNumberField(TEXT("rate_scale"), Asset->RateScale);
    Data->SetBoolField(TEXT("loop"), Asset->bLoop);
    Data->SetNumberField(TEXT("frame_rate"), Asset->GetSamplingFrameRate().AsDecimal());
    Data->SetNumberField(TEXT("frames"), Asset->GetNumberOfSampledKeys());

    if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
    {
        TEnumAsByte<ERootMotionRootLock::Type> Lock;
        UAnimationBlueprintLibrary::GetRootMotionLockType(Sequence, Lock);
        TEnumAsByte<EAdditiveAnimationType> Additive;
        UAnimationBlueprintLibrary::GetAdditiveAnimationType(Sequence, Additive);
        const TSharedRef<FJsonObject> RootMotion = MakeShared<FJsonObject>();
        RootMotion->SetBoolField(TEXT("enabled"), UAnimationBlueprintLibrary::IsRootMotionEnabled(Sequence));
        RootMotion->SetStringField(TEXT("root_lock"), EnumName(StaticEnum<ERootMotionRootLock::Type>(), Lock.GetValue()));
        RootMotion->SetBoolField(TEXT("force_root_lock"), UAnimationBlueprintLibrary::IsRootMotionLockForced(Sequence));
        Data->SetObjectField(TEXT("root_motion"), RootMotion);
        Data->SetStringField(TEXT("additive_type"), EnumName(StaticEnum<EAdditiveAnimationType>(), Additive.GetValue()));
        Data->SetArrayField(TEXT("sync_markers"), SyncMarkerList(Sequence));
    }

    if (const UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
    {
        const TSharedRef<FJsonObject> Blend = MakeShared<FJsonObject>();
        Blend->SetNumberField(TEXT("blend_in"), Montage->BlendIn.GetBlendTime());
        Blend->SetNumberField(TEXT("blend_out"), Montage->BlendOut.GetBlendTime());
        Blend->SetNumberField(TEXT("blend_out_trigger_time"), Montage->BlendOutTriggerTime);
        Blend->SetBoolField(TEXT("auto_blend_out"), Montage->bEnableAutoBlendOut);
        Data->SetObjectField(TEXT("blend"), Blend);
        Data->SetStringField(TEXT("sync_group"), Montage->SyncGroup.IsNone() ? FString() : Montage->SyncGroup.ToString());
        Data->SetArrayField(TEXT("sections"), SectionList(Montage));
        Data->SetArrayField(TEXT("slots"), SlotList(Montage));
    }

    TArray<TSharedPtr<FJsonValue>> Notifies;
    for (int32 Index = 0; Index < Asset->Notifies.Num(); ++Index)
    {
        Notifies.Add(MakeShared<FJsonValueObject>(NotifySummary(Asset, Index)));
    }
    Data->SetArrayField(TEXT("notifies"), Notifies);
    Data->SetArrayField(TEXT("notify_tracks"), TrackList(Asset));
    Data->SetArrayField(TEXT("curves"), CurveList(Asset));
    Data->SetArrayField(TEXT("settable_properties"), StringArray(SettableProperties(Asset)));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::GetNotify(UAnimSequenceBase* Asset, int32 NotifyIndex)
{
    if (!Asset)
    {
        return Fail(TEXT("Asset is required"));
    }
    if (!Asset->Notifies.IsValidIndex(NotifyIndex))
    {
        return Fail(FString::Printf(TEXT("No notify at index %d; %s has %d. Read the indices with Describe"),
            NotifyIndex, *Asset->GetName(), Asset->Notifies.Num()));
    }
    const UScriptStruct* EventStruct = NotifyEventStruct();
    FAnimNotifyEvent& Event = Asset->Notifies[NotifyIndex];
    const TSharedRef<FJsonObject> Data = NotifySummary(Asset, NotifyIndex);
    if (EventStruct)
    {
        const TSharedRef<FJsonObject> EventFields = ExportProperties(EventStruct, &Event, Asset, 0);
        EventFields->RemoveField(TEXT("Notify"));
        EventFields->RemoveField(TEXT("NotifyStateClass"));
        Data->SetObjectField(TEXT("event"), EventFields);
    }
    if (UObject* Object = NotifyObject(Event))
    {
        const TSharedRef<FJsonObject> ObjectFields = ExportProperties(Object->GetClass(), Object, Object, ObjectDepth);
        ObjectFields->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
        Data->SetObjectField(TEXT("object"), ObjectFields);
    }
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::FindNotifies(const FString& Folder, const FString& NotifyClass, const FString& NameContains)
{
    if (!Folder.StartsWith(TEXT("/")))
    {
        return Fail(TEXT("Folder must be a content folder such as \"/Game/Characters\""));
    }
    UClass* Class = nullptr;
    if (!NotifyClass.IsEmpty())
    {
        Class = ResolveClass(NotifyClass, UAnimNotify::StaticClass());
        if (!Class)
        {
            Class = ResolveClass(NotifyClass, UAnimNotifyState::StaticClass());
        }
        if (!Class)
        {
            return Fail(FString::Printf(TEXT("No notify or notify state class '%s'"), *NotifyClass));
        }
    }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.ClassPaths.Add(UAnimSequenceBase::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    FString PackagePath = Folder;
    PackagePath.RemoveFromEnd(TEXT("/"));
    Filter.PackagePaths.Add(FName(*PackagePath));
    Filter.bRecursivePaths = true;
    TArray<FAssetData> Assets;
    Registry.GetAssets(Filter, Assets);

    TArray<TSharedPtr<FJsonValue>> Matches;
    int32 Scanned = 0;
    bool bTruncated = false;
    for (const FAssetData& AssetData : Assets)
    {
        if (Scanned >= MaxScannedAssets || Matches.Num() >= MaxMatches)
        {
            bTruncated = true;
            break;
        }
        const UAnimSequenceBase* Asset = Cast<UAnimSequenceBase>(AssetData.GetAsset());
        ++Scanned;
        if (!Asset)
        {
            continue;
        }
        for (int32 Index = 0; Index < Asset->Notifies.Num(); ++Index)
        {
            const FAnimNotifyEvent& Event = Asset->Notifies[Index];
            const UObject* Object = NotifyObject(Event);
            if (Class && (!Object || !Object->IsA(Class)))
            {
                continue;
            }
            if (!NameContains.IsEmpty() && !NotifyDisplayName(Event).Contains(NameContains)
                && !(Object && Object->GetClass()->GetName().Contains(NameContains)))
            {
                continue;
            }
            const TSharedRef<FJsonObject> Match = NotifySummary(Asset, Index);
            Match->SetStringField(TEXT("asset"), Asset->GetPathName());
            Matches.Add(MakeShared<FJsonValueObject>(Match));
        }
    }

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("matches"), Matches);
    Data->SetNumberField(TEXT("assets_scanned"), Scanned);
    Data->SetNumberField(TEXT("assets_in_folder"), Assets.Num());
    Data->SetBoolField(TEXT("truncated"), bTruncated);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::GetCurveKeys(UAnimSequenceBase* Asset, const FString& CurveName)
{
    if (!Asset || CurveName.IsEmpty())
    {
        return Fail(TEXT("Asset and CurveName are required"));
    }
    if (!UAnimationBlueprintLibrary::DoesCurveExist(Asset, FName(*CurveName), ERawCurveTrackTypes::RCT_Float))
    {
        return Fail(FString::Printf(TEXT("%s has no float curve '%s'"), *Asset->GetName(), *CurveName));
    }
    TArray<float> Times;
    TArray<float> Values;
    UAnimationBlueprintLibrary::GetFloatKeys(Asset, FName(*CurveName), Times, Values);
    TArray<TSharedPtr<FJsonValue>> TimeValues;
    TArray<TSharedPtr<FJsonValue>> KeyValues;
    for (int32 Index = 0; Index < Times.Num(); ++Index)
    {
        TimeValues.Add(MakeShared<FJsonValueNumber>(Times[Index]));
        KeyValues.Add(MakeShared<FJsonValueNumber>(Values.IsValidIndex(Index) ? Values[Index] : 0.f));
    }
    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("curve"), CurveName);
    Data->SetArrayField(TEXT("times"), TimeValues);
    Data->SetArrayField(TEXT("values"), KeyValues);
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::AddNotify(UAnimSequenceBase* Asset, const FString& NotifyClass, const FString& NotifyName, float Time,
    float Duration, const FString& TrackName, const FString& PropertiesJson)
{
    if (!Asset)
    {
        return Fail(TEXT("Asset is required"));
    }
    if (!ValidTime(Asset, Time))
    {
        return Fail(FString::Printf(TEXT("Time %.3f is outside %s (0 to %.3f s)"), Time, *Asset->GetName(), Asset->GetPlayLength()));
    }
    UClass* Class = nullptr;
    bool bState = false;
    if (!NotifyClass.IsEmpty())
    {
        Class = ResolveClass(NotifyClass, UAnimNotify::StaticClass());
        if (!Class)
        {
            Class = ResolveClass(NotifyClass, UAnimNotifyState::StaticClass());
            bState = Class != nullptr;
        }
        if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
        {
            return Fail(FString::Printf(TEXT("No notify or notify state class '%s'. Pass a class name such as "
                "AnimNotifyState_MotionWarping, or a Blueprint notify asset path"), *NotifyClass));
        }
    }
    else if (NotifyName.IsEmpty())
    {
        return Fail(TEXT("Pass NotifyClass, or NotifyName for a skeleton notify"));
    }
    if (bState && (Duration <= 0.f || !ValidTime(Asset, Time + Duration)))
    {
        return Fail(FString::Printf(TEXT("A notify state needs a Duration above 0 that ends within the asset (%.3f s)"), Asset->GetPlayLength()));
    }
    TSharedPtr<FJsonObject> Properties;
    if (!PropertiesJson.TrimStartAndEnd().IsEmpty())
    {
        Properties = FAscentToolsetUtils::ParseObject(PropertiesJson);
        if (!Properties.IsValid())
        {
            return Fail(TEXT("PropertiesJson is not a valid JSON object"));
        }
    }

    const FScopedTransaction Transaction(LOCTEXT("AddNotify", "Add Anim Notify"));
    Asset->Modify();
    const int32 TrackIndex = EnsureTrack(Asset, TrackName);
    const FGuid Guid = FGuid::NewGuid();
    int32 Index = Asset->Notifies.AddDefaulted();
    {
        FAnimNotifyEvent& Event = Asset->Notifies[Index];
        Event.NotifyName = NotifyName.IsEmpty() ? NAME_None : FName(*NotifyName);
        Event.TrackIndex = TrackIndex;
        Event.Guid = Guid;
        if (Class)
        {
            UObject* Object = NewObject<UObject>(Asset, Class, NAME_None, RF_Transactional);
            if (bState)
            {
                Event.NotifyStateClass = Cast<UAnimNotifyState>(Object);
            }
            else
            {
                Event.Notify = Cast<UAnimNotify>(Object);
            }
        }
        PlaceNotify(Asset, Event, Time, Duration);
    }

    FPropertyWrites Writes;
    FString Error;
    if (Properties.IsValid() && !WriteNotifyProperties(Asset, Index, Properties, Writes, Error))
    {
        Writes.Finish();
        Asset->Notifies.RemoveAt(FindNotifyByGuid(Asset, Guid));
        FinishEdit(Asset);
        return Fail(Error + TEXT(". The notify was not added"));
    }
    Writes.Finish();
    FinishEdit(Asset);

    Index = FindNotifyByGuid(Asset, Guid);
    const TSharedRef<FJsonObject> Data = NotifySummary(Asset, Index);
    Data->SetArrayField(TEXT("applied"), StringArray(Writes.Applied));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::SetNotify(UAnimSequenceBase* Asset, int32 NotifyIndex, float Time, float Duration, const FString& TrackName,
    const FString& PropertiesJson)
{
    if (!Asset)
    {
        return Fail(TEXT("Asset is required"));
    }
    if (!Asset->Notifies.IsValidIndex(NotifyIndex))
    {
        return Fail(FString::Printf(TEXT("No notify at index %d; read the indices with Describe"), NotifyIndex));
    }
    TSharedPtr<FJsonObject> Properties;
    if (!PropertiesJson.TrimStartAndEnd().IsEmpty())
    {
        Properties = FAscentToolsetUtils::ParseObject(PropertiesJson);
        if (!Properties.IsValid())
        {
            return Fail(TEXT("PropertiesJson is not a valid JSON object"));
        }
    }
    const FAnimNotifyEvent& Current = Asset->Notifies[NotifyIndex];
    const bool bState = Current.NotifyStateClass != nullptr;
    if (Duration > 0.f && !bState)
    {
        return Fail(TEXT("Only a notify state has a duration"));
    }
    const float NewTime = Time >= 0.f ? Time : Current.GetTime();
    const float NewDuration = Duration > 0.f ? Duration : Current.GetDuration();
    if (!ValidTime(Asset, NewTime) || (bState && !ValidTime(Asset, NewTime + NewDuration)))
    {
        return Fail(FString::Printf(TEXT("The notify must stay within %s (0 to %.3f s)"), *Asset->GetName(), Asset->GetPlayLength()));
    }

    const FScopedTransaction Transaction(LOCTEXT("SetNotify", "Edit Anim Notify"));
    Asset->Modify();
    FGuid Guid = Asset->Notifies[NotifyIndex].Guid;
    if (!Guid.IsValid())
    {
        Guid = FGuid::NewGuid();
        Asset->Notifies[NotifyIndex].Guid = Guid;
    }
    if (!TrackName.IsEmpty())
    {
        const int32 TrackIndex = EnsureTrack(Asset, TrackName);
        Asset->Notifies[NotifyIndex].TrackIndex = TrackIndex;
    }
    if (Time >= 0.f || Duration > 0.f)
    {
        PlaceNotify(Asset, Asset->Notifies[NotifyIndex], NewTime, NewDuration);
    }

    FPropertyWrites Writes;
    FString Error;
    const bool bWritten = !Properties.IsValid() || WriteNotifyProperties(Asset, NotifyIndex, Properties, Writes, Error);
    Writes.Finish();
    FinishEdit(Asset);
    if (!bWritten)
    {
        return Fail(Error + AppliedSuffix(Writes));
    }

    const TSharedRef<FJsonObject> Data = NotifySummary(Asset, FindNotifyByGuid(Asset, Guid));
    Data->SetArrayField(TEXT("applied"), StringArray(Writes.Applied));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::RemoveNotifies(UAnimSequenceBase* Asset, const TArray<int32>& NotifyIndices)
{
    if (!Asset || NotifyIndices.IsEmpty())
    {
        return Fail(TEXT("Asset and NotifyIndices are required"));
    }
    TArray<int32> Sorted = NotifyIndices;
    Sorted.Sort(TGreater<int32>());
    for (const int32 Index : Sorted)
    {
        if (!Asset->Notifies.IsValidIndex(Index))
        {
            return Fail(FString::Printf(TEXT("No notify at index %d; nothing was removed"), Index));
        }
    }
    const FScopedTransaction Transaction(LOCTEXT("RemoveNotifies", "Remove Anim Notifies"));
    Asset->Modify();
    int32 Removed = 0;
    int32 Previous = INDEX_NONE;
    for (const int32 Index : Sorted)
    {
        if (Index == Previous)
        {
            continue;
        }
        Asset->Notifies.RemoveAt(Index);
        Previous = Index;
        ++Removed;
    }
    FinishEdit(Asset);

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("removed"), Removed);
    Data->SetNumberField(TEXT("notifies_left"), Asset->Notifies.Num());
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::EditNotifyTrack(UAnimSequenceBase* Asset, const FString& Action, const FString& TrackName, const FString& NewName)
{
    if (!Asset || TrackName.IsEmpty())
    {
        return Fail(TEXT("Asset and TrackName are required"));
    }
    const FName Name(*TrackName);
    const int32 Index = FindTrack(Asset, Name);
    const FString Kind = Action.ToLower();
    if (Kind == TEXT("add"))
    {
        if (Index != INDEX_NONE)
        {
            return Fail(FString::Printf(TEXT("Track '%s' already exists"), *TrackName));
        }
        const FScopedTransaction Transaction(LOCTEXT("AddTrack", "Add Notify Track"));
        Asset->Modify();
        EnsureTrack(Asset, TrackName);
    }
    else if (Kind == TEXT("remove") || Kind == TEXT("rename"))
    {
        if (Index == INDEX_NONE)
        {
            return Fail(FString::Printf(TEXT("No track '%s'"), *TrackName));
        }
        if (Kind == TEXT("rename"))
        {
            if (NewName.IsEmpty() || FindTrack(Asset, FName(*NewName)) != INDEX_NONE)
            {
                return Fail(TEXT("NewName must be a track name that is not taken"));
            }
            const FScopedTransaction Transaction(LOCTEXT("RenameTrack", "Rename Notify Track"));
            Asset->Modify();
            Asset->AnimNotifyTracks[Index].TrackName = FName(*NewName);
        }
        else
        {
            int32 Used = Asset->Notifies.FilterByPredicate([&](const FAnimNotifyEvent& Event) { return Event.TrackIndex == Index; }).Num();
            if (const UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
            {
                TArray<FAnimSyncMarker> Markers;
                UAnimationBlueprintLibrary::GetAnimationSyncMarkersForTrack(Sequence, Name, Markers);
                Used += Markers.Num();
            }
            if (Used > 0)
            {
                return Fail(FString::Printf(TEXT("Track '%s' holds %d notifies or sync markers. Remove or move them first"), *TrackName, Used));
            }
            const FScopedTransaction Transaction(LOCTEXT("RemoveTrack", "Remove Notify Track"));
            Asset->Modify();
            UAnimationBlueprintLibrary::RemoveAnimationNotifyTrack(Asset, Name);
        }
    }
    else
    {
        return Fail(FString::Printf(TEXT("Unknown Action '%s'. Use add, remove or rename"), *Action));
    }
    FinishEdit(Asset);

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("notify_tracks"), TrackList(Asset));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::EditSyncMarker(UAnimSequence* Sequence, const FString& Action, const FString& MarkerName, float Time, const FString& TrackName)
{
    if (!Sequence || MarkerName.IsEmpty())
    {
        return Fail(TEXT("Sequence and MarkerName are required"));
    }
    const FString Kind = Action.ToLower();
    if (Kind == TEXT("add"))
    {
        if (!ValidTime(Sequence, Time))
        {
            return Fail(FString::Printf(TEXT("Time %.3f is outside %s (0 to %.3f s)"), Time, *Sequence->GetName(), Sequence->GetPlayLength()));
        }
        const FScopedTransaction Transaction(LOCTEXT("AddSyncMarker", "Add Sync Marker"));
        Sequence->Modify();
        const int32 TrackIndex = EnsureTrack(Sequence, TrackName);
        UAnimationBlueprintLibrary::AddAnimationSyncMarker(Sequence, FName(*MarkerName), Time, Sequence->AnimNotifyTracks[TrackIndex].TrackName);
    }
    else if (Kind == TEXT("remove"))
    {
        const FScopedTransaction Transaction(LOCTEXT("RemoveSyncMarker", "Remove Sync Marker"));
        Sequence->Modify();
        if (UAnimationBlueprintLibrary::RemoveAnimationSyncMarkersByName(Sequence, FName(*MarkerName)) == 0)
        {
            return Fail(FString::Printf(TEXT("%s has no sync marker '%s'"), *Sequence->GetName(), *MarkerName));
        }
    }
    else
    {
        return Fail(FString::Printf(TEXT("Unknown Action '%s'. Use add or remove"), *Action));
    }
    FinishEdit(Sequence);

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("sync_markers"), SyncMarkerList(Sequence));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::EditCurve(UAnimSequenceBase* Asset, const FString& Action, const FString& CurveName, const TArray<float>& Times,
    const TArray<float>& Values)
{
    if (!Asset || CurveName.IsEmpty())
    {
        return Fail(TEXT("Asset and CurveName are required"));
    }
    const FName Name(*CurveName);
    const bool bExists = UAnimationBlueprintLibrary::DoesCurveExist(Asset, Name, ERawCurveTrackTypes::RCT_Float);
    const FString Kind = Action.ToLower();
    if (Kind == TEXT("add"))
    {
        if (bExists)
        {
            return Fail(FString::Printf(TEXT("Curve '%s' already exists"), *CurveName));
        }
        const FScopedTransaction Transaction(LOCTEXT("AddCurve", "Add Curve"));
        Asset->Modify();
        UAnimationBlueprintLibrary::AddCurve(Asset, Name, ERawCurveTrackTypes::RCT_Float);
    }
    else if (Kind == TEXT("remove"))
    {
        if (!bExists)
        {
            return Fail(FString::Printf(TEXT("No curve '%s'"), *CurveName));
        }
        const FScopedTransaction Transaction(LOCTEXT("RemoveCurve", "Remove Curve"));
        Asset->Modify();
        UAnimationBlueprintLibrary::RemoveCurve(Asset, Name);
    }
    else if (Kind == TEXT("set_keys"))
    {
        if (Times.IsEmpty() || Times.Num() != Values.Num())
        {
            return Fail(TEXT("set_keys needs Times and Values of the same, non-zero length"));
        }
        for (const float KeyTime : Times)
        {
            if (!ValidTime(Asset, KeyTime))
            {
                return Fail(FString::Printf(TEXT("Key time %.3f is outside the asset (0 to %.3f s)"), KeyTime, Asset->GetPlayLength()));
            }
        }
        const FScopedTransaction Transaction(LOCTEXT("SetCurveKeys", "Set Curve Keys"));
        Asset->Modify();
        if (!bExists)
        {
            UAnimationBlueprintLibrary::AddCurve(Asset, Name, ERawCurveTrackTypes::RCT_Float);
        }
        UAnimationBlueprintLibrary::AddFloatCurveKeys(Asset, Name, Times, Values);
    }
    else
    {
        return Fail(FString::Printf(TEXT("Unknown Action '%s'. Use add, remove or set_keys"), *Action));
    }
    Asset->PostEditChange();
    Asset->MarkPackageDirty();

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("curves"), CurveList(Asset));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::SetProperties(UAnimSequenceBase* Asset, const FString& PropertiesJson)
{
    if (!Asset)
    {
        return Fail(TEXT("Asset is required"));
    }
    const TSharedPtr<FJsonObject> Properties = FAscentToolsetUtils::ParseObject(PropertiesJson);
    if (!Properties.IsValid() || Properties->Values.IsEmpty())
    {
        return Fail(TEXT("PropertiesJson must be a JSON object with at least one property"));
    }
    const FScopedTransaction Transaction(LOCTEXT("SetProperties", "Edit Animation Properties"));
    FPropertyWrites Writes;
    FString Error;
    bool bWritten = true;
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Properties->Values)
    {
        if (!WriteProperty(Asset, Asset->GetClass(), Asset, Field.Key, Field.Value, nullptr, Writes, Error))
        {
            bWritten = false;
            break;
        }
    }
    Writes.Finish();
    if (UAnimMontage* Montage = Cast<UAnimMontage>(Asset))
    {
        FinishMontageEdit(Montage);
    }
    else
    {
        FinishEdit(Asset);
    }
    if (!bWritten)
    {
        return Fail(Error + AppliedSuffix(Writes));
    }

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("applied"), StringArray(Writes.Applied));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::EditMontageSection(UAnimMontage* Montage, const FString& Action, const FString& SectionName, float Time, const FString& Other)
{
    if (!Montage || SectionName.IsEmpty())
    {
        return Fail(TEXT("Montage and SectionName are required"));
    }
    const FName Name(*SectionName);
    const int32 Index = Montage->GetSectionIndex(Name);
    const FString Kind = Action.ToLower();
    if (Kind != TEXT("add") && Index == INDEX_NONE)
    {
        return Fail(FString::Printf(TEXT("%s has no section '%s'"), *Montage->GetName(), *SectionName));
    }
    if ((Kind == TEXT("add") || Kind == TEXT("move")) && !ValidTime(Montage, Time))
    {
        return Fail(FString::Printf(TEXT("Time %.3f is outside the montage (0 to %.3f s)"), Time, Montage->GetPlayLength()));
    }

    const FScopedTransaction Transaction(LOCTEXT("EditSection", "Edit Montage Section"));
    Montage->Modify();
    if (Kind == TEXT("add"))
    {
        if (Montage->AddAnimCompositeSection(Name, Time) == INDEX_NONE)
        {
            return Fail(FString::Printf(TEXT("Section '%s' already exists"), *SectionName));
        }
    }
    else if (Kind == TEXT("remove"))
    {
        if (Montage->CompositeSections.Num() <= 1)
        {
            return Fail(TEXT("A montage keeps at least one section"));
        }
        Montage->DeleteAnimCompositeSection(Index);
        for (FCompositeSection& Section : Montage->CompositeSections)
        {
            if (Section.NextSectionName == Name)
            {
                Section.NextSectionName = NAME_None;
            }
        }
    }
    else if (Kind == TEXT("rename"))
    {
        const FName NewName(*Other);
        if (Other.IsEmpty() || Montage->GetSectionIndex(NewName) != INDEX_NONE)
        {
            return Fail(TEXT("Other must be a section name that is not taken"));
        }
        Montage->CompositeSections[Index].SectionName = NewName;
        for (FCompositeSection& Section : Montage->CompositeSections)
        {
            if (Section.NextSectionName == Name)
            {
                Section.NextSectionName = NewName;
            }
        }
    }
    else if (Kind == TEXT("move"))
    {
        Montage->CompositeSections[Index].Link(Montage, Time, Montage->CompositeSections[Index].GetSlotIndex());
        Montage->CompositeSections.Sort([](const FCompositeSection& A, const FCompositeSection& B) { return A.GetTime() < B.GetTime(); });
    }
    else if (Kind == TEXT("link"))
    {
        const FName Next = Other.IsEmpty() ? NAME_None : FName(*Other);
        if (!Next.IsNone() && Montage->GetSectionIndex(Next) == INDEX_NONE)
        {
            return Fail(FString::Printf(TEXT("%s has no section '%s'"), *Montage->GetName(), *Other));
        }
        Montage->CompositeSections[Index].NextSectionName = Next;
    }
    else
    {
        return Fail(FString::Printf(TEXT("Unknown Action '%s'. Use add, remove, rename, move or link"), *Action));
    }
    FinishEdit(Montage);

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("sections"), SectionList(Montage));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::EditMontageSlot(UAnimMontage* Montage, const FString& Action, int32 SlotIndex, int32 SegmentIndex, const FString& OptionsJson)
{
    if (!Montage)
    {
        return Fail(TEXT("Montage is required"));
    }
    TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
    if (!OptionsJson.TrimStartAndEnd().IsEmpty())
    {
        Options = FAscentToolsetUtils::ParseObject(OptionsJson);
        if (!Options.IsValid())
        {
            return Fail(TEXT("OptionsJson is not a valid JSON object"));
        }
    }
    const FString Kind = Action.ToLower();
    FString SlotName;
    Options->TryGetStringField(TEXT("slot_name"), SlotName);
    if (Kind != TEXT("add_slot") && !Montage->SlotAnimTracks.IsValidIndex(SlotIndex))
    {
        return Fail(FString::Printf(TEXT("No slot at index %d; read the slots with Describe"), SlotIndex));
    }
    const bool bSegmentAction = Kind == TEXT("set_segment") || Kind == TEXT("remove_segment");
    if (bSegmentAction && !Montage->SlotAnimTracks[SlotIndex].AnimTrack.AnimSegments.IsValidIndex(SegmentIndex))
    {
        return Fail(FString::Printf(TEXT("Slot %d has no segment %d"), SlotIndex, SegmentIndex));
    }
    UAnimSequenceBase* Animation = nullptr;
    FString AnimationPath;
    if (Options->TryGetStringField(TEXT("animation"), AnimationPath) && !AnimationPath.IsEmpty())
    {
        Animation = LoadObject<UAnimSequenceBase>(nullptr, *AnimationPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (!Animation || !Animation->CanBeUsedInComposition())
        {
            return Fail(FString::Printf(TEXT("'%s' is not an Anim Sequence a montage can play"), *AnimationPath));
        }
        const USkeleton* Skeleton = Montage->GetSkeleton();
        if (Skeleton && !Skeleton->IsCompatibleForEditor(Animation->GetSkeleton()))
        {
            return Fail(TEXT("The animation uses a different skeleton from the montage"));
        }
    }

    const FScopedTransaction Transaction(LOCTEXT("EditSlot", "Edit Montage Slot"));
    Montage->Modify();
    if (Kind == TEXT("add_slot") || Kind == TEXT("rename_slot"))
    {
        if (SlotName.IsEmpty())
        {
            return Fail(TEXT("slot_name is required, e.g. \"DefaultGroup.UpperBody\""));
        }
        if (Montage->IsValidSlot(FName(*SlotName)))
        {
            return Fail(FString::Printf(TEXT("The montage already has slot '%s'"), *SlotName));
        }
        if (Kind == TEXT("add_slot"))
        {
            Montage->AddSlot(FName(*SlotName));
        }
        else
        {
            Montage->SlotAnimTracks[SlotIndex].SlotName = FName(*SlotName);
        }
    }
    else if (Kind == TEXT("remove_slot"))
    {
        if (Montage->SlotAnimTracks.Num() <= 1)
        {
            return Fail(TEXT("A montage keeps at least one slot"));
        }
        Montage->SlotAnimTracks.RemoveAt(SlotIndex);
    }
    else if (Kind == TEXT("add_segment") || Kind == TEXT("set_segment"))
    {
        FAnimTrack& Track = Montage->SlotAnimTracks[SlotIndex].AnimTrack;
        if (Kind == TEXT("add_segment") && !Animation)
        {
            return Fail(TEXT("add_segment needs \"animation\""));
        }
        FAnimSegment Segment = Kind == TEXT("add_segment") ? FAnimSegment() : Track.AnimSegments[SegmentIndex];
        if (Animation)
        {
            Segment.SetAnimReference(Animation, true);
        }
        if (Kind == TEXT("add_segment"))
        {
            Segment.StartPos = Track.GetLength();
        }
        double Number = 0.0;
        if (Options->TryGetNumberField(TEXT("start_pos"), Number)) { Segment.StartPos = static_cast<float>(Number); }
        if (Options->TryGetNumberField(TEXT("anim_start_time"), Number)) { Segment.AnimStartTime = static_cast<float>(Number); }
        if (Options->TryGetNumberField(TEXT("anim_end_time"), Number)) { Segment.AnimEndTime = static_cast<float>(Number); }
        if (Options->TryGetNumberField(TEXT("play_rate"), Number)) { Segment.AnimPlayRate = static_cast<float>(Number); }
        if (Options->TryGetNumberField(TEXT("looping_count"), Number)) { Segment.LoopingCount = FMath::Max(1, FMath::RoundToInt(Number)); }
        const UAnimSequenceBase* Played = Segment.GetAnimReference();
        if (Played && (Segment.AnimStartTime < 0.f || Segment.AnimEndTime > Played->GetPlayLength() + KINDA_SMALL_NUMBER
            || Segment.AnimEndTime <= Segment.AnimStartTime))
        {
            return Fail(FString::Printf(TEXT("anim_start_time and anim_end_time must lie within the animation (0 to %.3f s), start before end"),
                Played->GetPlayLength()));
        }
        if (Segment.StartPos < 0.f || FMath::IsNearlyZero(Segment.AnimPlayRate))
        {
            return Fail(TEXT("start_pos must be 0 or more and play_rate must not be 0"));
        }
        if (Kind == TEXT("add_segment"))
        {
            Track.AnimSegments.Add(Segment);
        }
        else
        {
            Track.AnimSegments[SegmentIndex] = Segment;
        }
        Track.AnimSegments.Sort([](const FAnimSegment& A, const FAnimSegment& B) { return A.StartPos < B.StartPos; });
    }
    else if (Kind == TEXT("remove_segment"))
    {
        Montage->SlotAnimTracks[SlotIndex].AnimTrack.AnimSegments.RemoveAt(SegmentIndex);
    }
    else
    {
        return Fail(FString::Printf(TEXT("Unknown Action '%s'. Use add_slot, rename_slot, remove_slot, add_segment, set_segment or remove_segment"), *Action));
    }
    FinishMontageEdit(Montage);

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("length"), Montage->GetPlayLength());
    Data->SetArrayField(TEXT("slots"), SlotList(Montage));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

FString UAscentAnimAssetToolset::CreateMontage(UAnimSequence* Source, const FString& AssetPath, const FString& SlotName)
{
    if (!Source || AssetPath.IsEmpty())
    {
        return Fail(TEXT("Source and AssetPath are required"));
    }
    FString PackageName = AssetPath;
    int32 Dot = INDEX_NONE;
    if (PackageName.FindChar(TEXT('.'), Dot))
    {
        PackageName.LeftInline(Dot);
    }
    if (!FPackageName::IsValidLongPackageName(PackageName))
    {
        return Fail(FString::Printf(TEXT("'%s' is not a valid asset path, e.g. \"/Game/Characters/AM_Vault\""), *AssetPath));
    }
    if (FPackageName::DoesPackageExist(PackageName))
    {
        return Fail(FString::Printf(TEXT("An asset already exists at '%s'"), *PackageName));
    }

    UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
    Factory->TargetSkeleton = Source->GetSkeleton();
    Factory->SourceAnimation = Source;
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    UAnimMontage* Montage = Cast<UAnimMontage>(AssetTools.CreateAsset(FPackageName::GetShortName(PackageName),
        FPackageName::GetLongPackagePath(PackageName), UAnimMontage::StaticClass(), Factory));
    if (!Montage)
    {
        return Fail(TEXT("The montage could not be created. Check the output log"));
    }
    if (!SlotName.IsEmpty() && Montage->SlotAnimTracks.Num() > 0)
    {
        Montage->Modify();
        Montage->SlotAnimTracks[0].SlotName = FName(*SlotName);
        FinishMontageEdit(Montage);
    }
    Montage->MarkPackageDirty();

    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("path"), Montage->GetPathName());
    Data->SetNumberField(TEXT("length"), Montage->GetPlayLength());
    Data->SetArrayField(TEXT("sections"), SectionList(Montage));
    Data->SetArrayField(TEXT("slots"), SlotList(Montage));
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

#undef LOCTEXT_NAMESPACE
