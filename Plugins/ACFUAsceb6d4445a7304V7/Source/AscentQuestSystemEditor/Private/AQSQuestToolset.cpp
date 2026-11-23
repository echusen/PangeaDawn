// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AQSQuestToolset.h"

#include "AGSAction.h"
#include "AGSCondition.h"
#include "AGSGraphEdge.h"
#include "AQSQuestObjective.h"
#include "AQSTypes.h"
#include "AssetGraphSchema_AGSGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "EdGraph_AGSGraph.h"
#include "EdNode_AGSGraphNode.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Graph/AQSEdge.h"
#include "Graph/AQSFinishQuestNode.h"
#include "Graph/AQSObjectiveNode.h"
#include "Graph/AQSQuest.h"
#include "Graph/AQSQuestFailedNode.h"
#include "Graph/AQSQuestSuccededNode.h"
#include "Graph/AQSStartQuestNode.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/OutputDevice.h"
#include "Misc/PackageName.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"

namespace AQSQuestToolsetImpl
{
    /** Objective fields the spec carries explicitly; ObjectiveProperties covers everything else. */
    static const TCHAR* const BaseObjectiveFields[] = {
        TEXT("ObjectiveName"), TEXT("ObjectiveDescription"), TEXT("ObjectiveTag"),
        TEXT("TargetRefType"), TEXT("ReferencedTargets"), TEXT("ReferencedActors")
    };

    class FCapturedErrors : public FOutputDevice
    {
    public:
        FString Text;
        virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override
        {
            Text += Message;
        }
    };

    // ------------------------------------------------------------------
    // Asset paths
    // ------------------------------------------------------------------

    /** Accepts "/Game/Dir/Name" or "/Game/Dir/Name.Name"; returns the package name and the object path. */
    static bool SplitAssetPath(const FString& InPath, FString& OutPackage, FString& OutObjectPath, FString& OutError)
    {
        const FString Path = InPath.TrimStartAndEnd();
        OutPackage = Path;
        int32 Dot = INDEX_NONE;
        if (Path.FindChar(TEXT('.'), Dot))
        {
            OutPackage = Path.Left(Dot);
        }

        FText Reason;
        if (OutPackage.IsEmpty() || OutPackage.Contains(TEXT("//")) || OutPackage.Contains(TEXT(".."))
            || !FPackageName::IsValidLongPackageName(OutPackage, false, &Reason))
        {
            OutError = FString::Printf(TEXT("'%s' is not a valid asset path%s%s"), *InPath,
                Reason.IsEmpty() ? TEXT("") : TEXT(": "), *Reason.ToString());
            return false;
        }

        FString Filename;
        if (!FPackageName::TryConvertLongPackageNameToFilename(OutPackage, Filename, FPackageName::GetAssetPackageExtension()))
        {
            OutError = FString::Printf(TEXT("'%s' is not under a mounted content root"), *InPath);
            return false;
        }

        OutObjectPath = OutPackage + TEXT(".") + FPackageName::GetShortName(OutPackage);
        return true;
    }

    static bool SavePackageOf(UObject* Asset, FString& OutError)
    {
        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        FString Filename;
        if (!FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), Filename, FPackageName::GetAssetPackageExtension()))
        {
            OutError = FString::Printf(TEXT("'%s' does not map to any mounted content root"), *Package->GetName());
            return false;
        }
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
        {
            OutError = FString::Printf(TEXT("failed to save package %s"), *Package->GetName());
            return false;
        }
        return true;
    }

    // ------------------------------------------------------------------
    // Class discovery
    // ------------------------------------------------------------------

    static UClass* GetKindBaseClass(const FString& Kind)
    {
        if (Kind.Equals(TEXT("objective"), ESearchCase::IgnoreCase)) { return UAQSQuestObjective::StaticClass(); }
        if (Kind.Equals(TEXT("action"), ESearchCase::IgnoreCase)) { return UAGSAction::StaticClass(); }
        if (Kind.Equals(TEXT("condition"), ESearchCase::IgnoreCase)) { return UAGSCondition::StaticClass(); }
        return nullptr;
    }

    /** Native and Blueprint subclasses of Base, keyed by class path. Blueprints are found unloaded through the registry. */
    static TArray<FAQSQuestClassInfo> CollectClasses(UClass* Base)
    {
        TMap<FString, FAQSQuestClassInfo> ByPath;

        for (TObjectIterator<UClass> It; It; ++It)
        {
            UClass* Class = *It;
            if (!Class->IsChildOf(Base) || Class == Base) { continue; }
            if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) { continue; }
            if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_"))) { continue; }
            if (Class->ClassGeneratedBy) { continue; }

            FAQSQuestClassInfo Info;
            Info.Name = Class->GetName();
            Info.ClassPath = Class->GetPathName();
            Info.Description = Class->GetToolTipText(true).ToString();
            ByPath.Add(Info.ClassPath, Info);
        }

        const FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        TArray<FAssetData> Blueprints;
        Registry.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Blueprints, true);
        for (const FAssetData& Asset : Blueprints)
        {
            const FString NativeParent = FPackageName::ExportTextPathToObjectPath(Asset.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath));
            const UClass* NativeClass = NativeParent.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *NativeParent);
            if (!NativeClass || !NativeClass->IsChildOf(Base)) { continue; }

            FAQSQuestClassInfo Info;
            Info.Name = Asset.AssetName.ToString();
            Info.ClassPath = FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Info.Name);
            Info.Description = Asset.GetTagValueRef<FString>(FBlueprintTags::BlueprintDescription);
            ByPath.Add(Info.ClassPath, Info);
        }

        TArray<FAQSQuestClassInfo> Result;
        ByPath.GenerateValueArray(Result);
        Result.Sort([](const FAQSQuestClassInfo& A, const FAQSQuestClassInfo& B) { return A.Name < B.Name; });
        return Result;
    }

    /** Resolve a class by name or path among Base's subclasses. Unknown names fail with the installed set. */
    static UClass* ResolveClass(UClass* Base, const FString& NameOrPath, FString& OutError)
    {
        const FString Wanted = NameOrPath.TrimStartAndEnd();
        if (Base == UAQSQuestObjective::StaticClass()
            && (Wanted.Equals(Base->GetName(), ESearchCase::IgnoreCase)
                || Wanted.Equals(Base->GetPathName(), ESearchCase::IgnoreCase)))
        {
            return Base;
        }
        const TArray<FAQSQuestClassInfo> Classes = CollectClasses(Base);
        for (const FAQSQuestClassInfo& Info : Classes)
        {
            const bool bHit = Info.Name.Equals(Wanted, ESearchCase::IgnoreCase)
                || (Info.Name + TEXT("_C")).Equals(Wanted, ESearchCase::IgnoreCase)
                || Info.ClassPath.Equals(Wanted, ESearchCase::IgnoreCase)
                || Info.ClassPath.StartsWith(Wanted + TEXT("."), ESearchCase::IgnoreCase);
            if (!bHit) { continue; }

            UClass* Loaded = LoadClass<UObject>(nullptr, *Info.ClassPath);
            if (Loaded && Loaded->IsChildOf(Base))
            {
                return Loaded;
            }
            OutError = FString::Printf(TEXT("class '%s' resolved to '%s' but failed to load"), *Wanted, *Info.ClassPath);
            return nullptr;
        }

        TArray<FString> Names;
        for (const FAQSQuestClassInfo& Info : Classes) { Names.Add(Info.Name); }
        OutError = FString::Printf(TEXT("unknown %s class '%s'. Installed: %s"), *Base->GetName(), *Wanted,
            Names.Num() ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
        return nullptr;
    }

    // ------------------------------------------------------------------
    // Reflection
    // ------------------------------------------------------------------

    static bool IsAuthorableProperty(const FProperty* Property)
    {
        return Property->HasAnyPropertyFlags(CPF_Edit)
            && !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Deprecated | CPF_InstancedReference | CPF_ContainsInstancedReference);
    }

    static FProperty* FindAuthorableProperty(const UClass* Class, const FString& Name)
    {
        for (TFieldIterator<FProperty> It(Class); It; ++It)
        {
            if (IsAuthorableProperty(*It)
                && (It->GetName().Equals(Name, ESearchCase::IgnoreCase) || It->GetAuthoredName().Equals(Name, ESearchCase::IgnoreCase)))
            {
                return *It;
            }
        }
        return nullptr;
    }

    static FProperty* FindPropertyAnyFlags(const UObject* Target, const TCHAR* Name)
    {
        return Target ? Target->GetClass()->FindPropertyByName(FName(Name)) : nullptr;
    }

    static bool RequestTag(const FString& TagName, FGameplayTag& OutTag, FString& OutError)
    {
        OutTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName.TrimStartAndEnd()), false);
        if (!OutTag.IsValid())
        {
            OutError = FString::Printf(TEXT("GameplayTag '%s' is not registered"), *TagName);
            return false;
        }
        return true;
    }

    static TArray<FString> SplitList(const FString& Value)
    {
        TArray<FString> Items;
        Value.ParseIntoArray(Items, TEXT(","), true);
        for (FString& Item : Items) { Item.TrimStartAndEndInline(); }
        Items.RemoveAll([](const FString& Item) { return Item.IsEmpty(); });
        return Items;
    }

    static bool IsTagStruct(const FProperty* Property, const UScriptStruct* Struct)
    {
        const FStructProperty* StructProp = CastField<FStructProperty>(Property);
        return StructProp && StructProp->Struct == Struct;
    }

    /**
     * Write a property from text. GameplayTags, tag containers and tag arrays take bare tag names (comma
     * separated for lists) and are checked against the tag registry; arrays without parentheses take comma
     * separated items; everything else uses Unreal's ImportText format, as reported by DescribeClass.
     */
    static bool SetPropertyFromString(UObject* Target, FProperty* Property, const FString& Value, FString& OutError)
    {
        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);
        const FString Trimmed = Value.TrimStartAndEnd();

        if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
        {
            const bool bLocalizedSyntax = Trimmed.StartsWith(TEXT("NSLOCTEXT(")) || Trimmed.StartsWith(TEXT("NSNOTELOCTEXT("))
                || Trimmed.StartsWith(TEXT("LOCTEXT(")) || Trimmed.StartsWith(TEXT("INVTEXT("))
                || Trimmed.StartsWith(TEXT("LOCTABLE("));
            if (!bLocalizedSyntax)
            {
                if (!TextNamespaceUtil::EditTextProperty_Direct(Target->GetPackage(), ValuePtr, TextProp,
                    TextNamespaceUtil::ETextEditAction::SourceString, Value))
                {
                    TextProp->SetPropertyValue(ValuePtr, FText::FromString(Value));
                }
                return true;
            }
        }
        if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
        {
            StrProp->SetPropertyValue(ValuePtr, Value);
            return true;
        }
        if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
        {
            NameProp->SetPropertyValue(ValuePtr, FName(*Trimmed));
            return true;
        }

        if (IsTagStruct(Property, FGameplayTag::StaticStruct()) && !Trimmed.StartsWith(TEXT("(")))
        {
            FGameplayTag Tag;
            if (!Trimmed.IsEmpty() && !RequestTag(Trimmed, Tag, OutError)) { return false; }
            *static_cast<FGameplayTag*>(ValuePtr) = Tag;
            return true;
        }
        if (IsTagStruct(Property, FGameplayTagContainer::StaticStruct()) && !Trimmed.StartsWith(TEXT("(")))
        {
            FGameplayTagContainer Container;
            for (const FString& Item : SplitList(Trimmed))
            {
                FGameplayTag Tag;
                if (!RequestTag(Item, Tag, OutError)) { return false; }
                Container.AddTag(Tag);
            }
            *static_cast<FGameplayTagContainer*>(ValuePtr) = Container;
            return true;
        }
        if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property); ArrayProp && !Trimmed.StartsWith(TEXT("(")))
        {
            const TArray<FString> Items = SplitList(Trimmed);
            FScriptArrayHelper Helper(ArrayProp, ValuePtr);
            Helper.EmptyValues();
            for (const FString& Item : Items)
            {
                const int32 Index = Helper.AddValue();
                if (IsTagStruct(ArrayProp->Inner, FGameplayTag::StaticStruct()))
                {
                    FGameplayTag Tag;
                    if (!RequestTag(Item, Tag, OutError)) { return false; }
                    *reinterpret_cast<FGameplayTag*>(Helper.GetRawPtr(Index)) = Tag;
                    continue;
                }
                FCapturedErrors Errors;
                if (!ArrayProp->Inner->ImportText_Direct(*Item, Helper.GetRawPtr(Index), Target, PPF_None, &Errors))
                {
                    OutError = FString::Printf(TEXT("'%s' is not a valid %s item for '%s' %s"), *Item,
                        *ArrayProp->Inner->GetCPPType(), *Property->GetName(), *Errors.Text);
                    return false;
                }
            }
            return true;
        }

        FCapturedErrors Errors;
        if (!Property->ImportText_Direct(*Trimmed, ValuePtr, Target, PPF_None, &Errors) || !Errors.Text.IsEmpty())
        {
            OutError = FString::Printf(TEXT("'%s' is not a valid %s value for '%s' %s"), *Trimmed,
                *Property->GetCPPType(), *Property->GetName(), *Errors.Text);
            return false;
        }
        return true;
    }

    static bool SetNamedProperty(UObject* Target, const FString& Name, const FString& Value, FString& OutError)
    {
        FProperty* Property = FindAuthorableProperty(Target->GetClass(), Name);
        if (!Property)
        {
            OutError = FString::Printf(TEXT("%s has no editable property '%s' (see DescribeClass)"), *Target->GetClass()->GetName(), *Name);
            return false;
        }
        return SetPropertyFromString(Target, Property, Value, OutError);
    }

    /** Inverse of SetPropertyFromString: bare tag names, comma separated tag lists, ExportText otherwise. */
    static FString PropertyToString(const UObject* Source, const FProperty* Property)
    {
        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Source);
        if (IsTagStruct(Property, FGameplayTag::StaticStruct()))
        {
            const FGameplayTag& Tag = *static_cast<const FGameplayTag*>(ValuePtr);
            return Tag.IsValid() ? Tag.ToString() : FString();
        }
        if (IsTagStruct(Property, FGameplayTagContainer::StaticStruct()))
        {
            TArray<FString> Names;
            for (const FGameplayTag& Tag : *static_cast<const FGameplayTagContainer*>(ValuePtr))
            {
                if (Tag.IsValid()) { Names.Add(Tag.ToString()); }
            }
            return FString::Join(Names, TEXT(", "));
        }
        if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property); ArrayProp && IsTagStruct(ArrayProp->Inner, FGameplayTag::StaticStruct()))
        {
            FScriptArrayHelper Helper(ArrayProp, ValuePtr);
            TArray<FString> Names;
            for (int32 Index = 0; Index < Helper.Num(); ++Index)
            {
                const FGameplayTag& Tag = *reinterpret_cast<const FGameplayTag*>(Helper.GetRawPtr(Index));
                if (Tag.IsValid()) { Names.Add(Tag.ToString()); }
            }
            return FString::Join(Names, TEXT(", "));
        }
        FString Text;
        Property->ExportTextItem_Direct(Text, ValuePtr, nullptr, const_cast<UObject*>(Source), PPF_None);
        return Text;
    }

    static FText GetTextProperty(const UObject* Source, const TCHAR* Name)
    {
        const FTextProperty* Prop = CastField<FTextProperty>(FindPropertyAnyFlags(Source, Name));
        return Prop ? Prop->GetPropertyValue_InContainer(Source) : FText::GetEmpty();
    }

    static FString GetTextPropertyString(const UObject* Source, const TCHAR* Name)
    {
        const FProperty* Property = FindPropertyAnyFlags(Source, Name);
        return Property ? PropertyToString(Source, Property) : FString();
    }
    static FString GetPropertyString(const UObject* Source, const TCHAR* Name)
    {
        const FProperty* Prop = FindPropertyAnyFlags(Source, Name);
        return Prop ? PropertyToString(Source, Prop) : FString();
    }

    /** Authorable properties of Source that differ from its class defaults, skipping the given names. */
    static TMap<FString, FString> ExportChangedProperties(const UObject* Source, const TSet<FString>& Skip)
    {
        TMap<FString, FString> Result;
        const UObject* Defaults = Source->GetClass()->GetDefaultObject();
        for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It)
        {
            if (!IsAuthorableProperty(*It) || Skip.Contains(It->GetName())) { continue; }
            if (It->Identical_InContainer(Source, Defaults)) { continue; }
            Result.Add(It->GetAuthoredName(), PropertyToString(Source, *It));
        }
        return Result;
    }

    // ------------------------------------------------------------------
    // Node building
    // ------------------------------------------------------------------

    static UClass* GetNodeClass(const FString& Kind)
    {
        if (Kind.Equals(TEXT("start"), ESearchCase::IgnoreCase)) { return UAQSStartQuestNode::StaticClass(); }
        if (Kind.Equals(TEXT("objective"), ESearchCase::IgnoreCase)) { return UAQSObjectiveNode::StaticClass(); }
        if (Kind.Equals(TEXT("success"), ESearchCase::IgnoreCase)) { return UAQSQuestSuccededNode::StaticClass(); }
        if (Kind.Equals(TEXT("failure"), ESearchCase::IgnoreCase)) { return UAQSQuestFailedNode::StaticClass(); }
        return nullptr;
    }

    static FString GetNodeKind(const UAGSGraphNode* Node)
    {
        if (Node->IsA<UAQSStartQuestNode>()) { return TEXT("start"); }
        if (Node->IsA<UAQSObjectiveNode>()) { return TEXT("objective"); }
        if (Node->IsA<UAQSQuestSuccededNode>()) { return TEXT("success"); }
        if (Node->IsA<UAQSQuestFailedNode>()) { return TEXT("failure"); }
        return Node->GetClass()->GetName();
    }

    /** Instantiate the specs as subobjects of Owner and store them in Owner's object array property. */
    static bool SetInstancedArray(UObject* Owner, const TCHAR* PropertyName, UClass* Base, const TArray<FAQSQuestObjectSpec>& Specs, FString& OutError)
    {
        FArrayProperty* ArrayProp = CastField<FArrayProperty>(FindPropertyAnyFlags(Owner, PropertyName));
        FObjectPropertyBase* Inner = ArrayProp ? CastField<FObjectPropertyBase>(ArrayProp->Inner) : nullptr;
        if (!Inner)
        {
            OutError = FString::Printf(TEXT("%s has no object array '%s'"), *Owner->GetClass()->GetName(), PropertyName);
            return false;
        }

        FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
        Helper.EmptyValues();
        for (int32 SpecIndex = 0; SpecIndex < Specs.Num(); ++SpecIndex)
        {
            const FAQSQuestObjectSpec& Spec = Specs[SpecIndex];
            UClass* Class = ResolveClass(Base, Spec.Class, OutError);
            if (!Class)
            {
                OutError = FString::Printf(TEXT("%s[%d]: %s"), PropertyName, SpecIndex, *OutError);
                return false;
            }
            UObject* Instance = NewObject<UObject>(Owner, Class, NAME_None, RF_Transactional);
            for (const TPair<FString, FString>& Pair : Spec.Properties)
            {
                if (!SetNamedProperty(Instance, Pair.Key, Pair.Value, OutError))
                {
                    OutError = FString::Printf(TEXT("%s[%d]: %s"), PropertyName, SpecIndex, *OutError);
                    return false;
                }
            }
            const int32 Index = Helper.AddValue();
            Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Instance);
        }
        return true;
    }

    static TArray<FAQSQuestObjectSpec> GetInstancedArray(const UObject* Owner, const TCHAR* PropertyName)
    {
        TArray<FAQSQuestObjectSpec> Result;
        const FArrayProperty* ArrayProp = CastField<FArrayProperty>(FindPropertyAnyFlags(Owner, PropertyName));
        const FObjectPropertyBase* Inner = ArrayProp ? CastField<FObjectPropertyBase>(ArrayProp->Inner) : nullptr;
        if (!Inner) { return Result; }

        FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            if (const UObject* Instance = Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index)))
            {
                FAQSQuestObjectSpec Spec;
                Spec.Class = Instance->GetClass()->GetPathName();
                Spec.Properties = ExportChangedProperties(Instance, {});
                Result.Add(Spec);
            }
        }
        return Result;
    }

    static FString ResolveTargetRefType(const FAQSQuestNodeSpec& Spec)
    {
        if (!Spec.TargetRefType.IsEmpty())
        {
            return Spec.TargetRefType.StartsWith(TEXT("E")) ? Spec.TargetRefType : TEXT("E") + Spec.TargetRefType;
        }
        if (Spec.ReferencedActors.Num() > 0) { return TEXT("ESoftRef"); }
        if (Spec.ReferencedTargets.Num() > 0) { return TEXT("ETag"); }
        return FString();
    }

    /** Build one runtime node with its payload. Used for both the dry run (transient outer) and the real save. */
    static UAQSBaseNode* BuildNode(UObject* Outer, UAQSQuest* Quest, const FAQSQuestNodeSpec& Spec, TArray<FString>& OutWarnings, FString& OutError)
    {
        UClass* NodeClass = GetNodeClass(Spec.Kind);
        if (!NodeClass)
        {
            OutError = FString::Printf(TEXT("unknown kind '%s' (start | objective | success | failure)"), *Spec.Kind);
            return nullptr;
        }

        UAQSBaseNode* Node = NewObject<UAQSBaseNode>(Outer, NodeClass, NAME_None, RF_Transactional);
        Node->Graph = Quest;
        Node->GenerateNewID();

        if (!SetInstancedArray(Node, TEXT("ActivationActions"), UAGSAction::StaticClass(), Spec.ActivationActions, OutError)
            || !SetInstancedArray(Node, TEXT("OnCompletionActions"), UAGSAction::StaticClass(), Spec.CompletionActions, OutError))
        {
            return nullptr;
        }

        if (Node->IsA<UAQSFinishQuestNode>())
        {
            if (!Spec.NextQuestTag.IsEmpty() && !SetPropertyFromString(Node, FindPropertyAnyFlags(Node, TEXT("NextQuestTag")), Spec.NextQuestTag, OutError))
            {
                OutError = TEXT("NextQuestTag: ") + OutError;
                return nullptr;
            }
            return Node;
        }

        if (!Node->IsA<UAQSObjectiveNode>())
        {
            return Node;
        }

        UClass* ObjectiveClass = UAQSQuestObjective::StaticClass();
        if (!Spec.ObjectiveClass.IsEmpty()
            && !(ObjectiveClass = ResolveClass(UAQSQuestObjective::StaticClass(), Spec.ObjectiveClass, OutError)))
        {
            return nullptr;
        }
        if (ObjectiveClass == UAQSQuestObjective::StaticClass())
        {
            OutWarnings.Add(FString::Printf(TEXT("objective '%s' uses the base AQSQuestObjective, which never completes on its own; pick a class from ListClasses(\"objective\")"), *Spec.Id));
        }

        UAQSQuestObjective* Objective = NewObject<UAQSQuestObjective>(Node, ObjectiveClass, NAME_None, RF_Transactional);

        const FString RefType = ResolveTargetRefType(Spec);
        if (Spec.ReferencedTargets.Num() > 0 && RefType != TEXT("ETag"))
        {
            OutError = TEXT("ReferencedTargets need TargetRefType ETag; use ReferencedActors for ESoftRef");
            return nullptr;
        }
        if (Spec.ReferencedActors.Num() > 0 && RefType != TEXT("ESoftRef"))
        {
            OutError = TEXT("ReferencedActors need TargetRefType ESoftRef; use ReferencedTargets for ETag");
            return nullptr;
        }

        TArray<TPair<const TCHAR*, FString>> BaseValues;
        if (!Spec.ObjectiveName.IsEmpty()) { BaseValues.Emplace(TEXT("ObjectiveName"), Spec.ObjectiveName); }
        if (!Spec.ObjectiveDescription.IsEmpty()) { BaseValues.Emplace(TEXT("ObjectiveDescription"), Spec.ObjectiveDescription); }
        if (!Spec.ObjectiveTag.IsEmpty()) { BaseValues.Emplace(TEXT("ObjectiveTag"), Spec.ObjectiveTag); }
        if (!RefType.IsEmpty()) { BaseValues.Emplace(TEXT("TargetRefType"), RefType); }
        if (Spec.ReferencedTargets.Num() > 0) { BaseValues.Emplace(TEXT("ReferencedTargets"), FString::Join(Spec.ReferencedTargets, TEXT(","))); }
        if (Spec.ReferencedActors.Num() > 0) { BaseValues.Emplace(TEXT("ReferencedActors"), FString::Join(Spec.ReferencedActors, TEXT(","))); }

        for (const TPair<const TCHAR*, FString>& Pair : BaseValues)
        {
            FProperty* Property = FindPropertyAnyFlags(Objective, Pair.Key);
            if (!Property || !SetPropertyFromString(Objective, Property, Pair.Value, OutError))
            {
                OutError = FString::Printf(TEXT("%s: %s"), Pair.Key, Property ? *OutError : TEXT("property not found on the objective"));
                return nullptr;
            }
        }

        for (const TPair<FString, FString>& Pair : Spec.ObjectiveProperties)
        {
            if (!SetNamedProperty(Objective, Pair.Key, Pair.Value, OutError))
            {
                return nullptr;
            }
        }

        if (Spec.ObjectiveName.IsEmpty() && GetTextProperty(Objective, TEXT("ObjectiveName")).IsEmpty())
        {
            OutWarnings.Add(FString::Printf(TEXT("objective '%s' has no ObjectiveName; the quest tracker will show an empty label"), *Spec.Id));
        }
        if (RefType.IsEmpty() && Spec.ObjectiveProperties.Num() == 0)
        {
            OutWarnings.Add(FString::Printf(TEXT("objective '%s' has no targets"), *Spec.Id));
        }

        FObjectPropertyBase* ObjectiveProp = CastField<FObjectPropertyBase>(FindPropertyAnyFlags(Node, TEXT("Objective")));
        FIntProperty* RepetitionsProp = CastField<FIntProperty>(FindPropertyAnyFlags(Node, TEXT("Repetitions")));
        if (!ObjectiveProp || !RepetitionsProp)
        {
            OutError = TEXT("AQSObjectiveNode has no Objective/Repetitions property");
            return nullptr;
        }
        ObjectiveProp->SetObjectPropertyValue_InContainer(Node, Objective);
        RepetitionsProp->SetPropertyValue_InContainer(Node, FMath::Max(1, Spec.Repetitions));

        if (!SetInstancedArray(Node, TEXT("CompleteConditions"), UAGSCondition::StaticClass(), Spec.CompleteConditions, OutError))
        {
            return nullptr;
        }
        return Node;
    }

    static FString DescribeExistingNode(const UAGSGraphNode* Node)
    {
        FString Label;
        if (const UAQSObjectiveNode* ObjectiveNode = Cast<UAQSObjectiveNode>(Node))
        {
            if (const UAQSQuestObjective* Objective = ObjectiveNode->GetQuestObjective())
            {
                Label = Objective->GetRawName().ToString();
            }
        }
        return FString::Printf(TEXT("%s '%s' (%s)"), *GetNodeKind(Node), *Label, *Node->GetNodeId().ToString());
    }

    // ------------------------------------------------------------------
    // Validation
    // ------------------------------------------------------------------

    struct FResolvedSpec
    {
        FString PackageName;
        FString ObjectPath;
        UAQSQuest* ExistingQuest = nullptr;
        TMap<FString, FGuid> GuidById;
    };

    static FAQSQuestReport Validate(const FAQSQuestSpec& Spec, FResolvedSpec& OutResolved)
    {
        FAQSQuestReport Report;
        TArray<FString>& Errors = Report.Errors;
        TArray<FString>& Warnings = Report.Warnings;

        FString PathError;
        if (!SplitAssetPath(Spec.AssetPath, OutResolved.PackageName, OutResolved.ObjectPath, PathError))
        {
            Errors.Add(PathError);
        }
        else if (!OutResolved.PackageName.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive))
        {
            Errors.Add(TEXT("AssetPath must be under /Game/ for quest writes"));
        }
        else
        {
            Report.AssetPath = OutResolved.ObjectPath;
            if (UObject* Existing = LoadObject<UObject>(nullptr, *OutResolved.ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
            {
                OutResolved.ExistingQuest = Cast<UAQSQuest>(Existing);
                if (!OutResolved.ExistingQuest)
                {
                    Errors.Add(FString::Printf(TEXT("'%s' already holds a %s, not a quest"), *OutResolved.ObjectPath, *Existing->GetClass()->GetName()));
                }
                else if (!Spec.bOverwriteExisting)
                {
                    Errors.Add(FString::Printf(TEXT("'%s' already exists; set bOverwriteExisting to update it"), *OutResolved.ObjectPath));
                }
            }
            Report.bCreatesAsset = OutResolved.ExistingQuest == nullptr;
        }

        FGameplayTag Tag;
        FString TagError;
        if (Spec.QuestTag.IsEmpty())
        {
            Errors.Add(TEXT("QuestTag is required: StartQuest finds quests by tag"));
        }
        else if (!RequestTag(Spec.QuestTag, Tag, TagError))
        {
            Errors.Add(TEXT("QuestTag: ") + TagError);
        }

        // Graph structure.
        TSet<FString> Ids;
        int32 StartCount = 0;
        int32 ObjectiveCount = 0;
        for (const FAQSQuestNodeSpec& Node : Spec.Nodes)
        {
            if (Node.Id.IsEmpty()) { Errors.Add(TEXT("every node needs an Id")); continue; }
            if (Ids.Contains(Node.Id)) { Errors.Add(FString::Printf(TEXT("duplicate node Id '%s'"), *Node.Id)); }
            Ids.Add(Node.Id);
            StartCount += Node.Kind.Equals(TEXT("start"), ESearchCase::IgnoreCase) ? 1 : 0;
            ObjectiveCount += Node.Kind.Equals(TEXT("objective"), ESearchCase::IgnoreCase) ? 1 : 0;
        }
        if (Spec.Nodes.Num() == 0) { Errors.Add(TEXT("Nodes is empty: a quest with no graph does nothing")); }
        if (StartCount != 1) { Errors.Add(FString::Printf(TEXT("%d start nodes; exactly one is required"), StartCount)); }
        if (ObjectiveCount == 0) { Warnings.Add(TEXT("no objective nodes: the quest starts and ends with nothing to do")); }
        for (const FAQSQuestEdgeSpec& Edge : Spec.Edges)
        {
            if (!Ids.Contains(Edge.From)) { Errors.Add(FString::Printf(TEXT("edge from unknown node '%s'"), *Edge.From)); }
            if (!Ids.Contains(Edge.To)) { Errors.Add(FString::Printf(TEXT("edge to unknown node '%s'"), *Edge.To)); }
            if (Edge.From == Edge.To) { Errors.Add(FString::Printf(TEXT("self-loop on node '%s' is not allowed"), *Edge.From)); }
        }
        if (Spec.Edges.Num() == 0 && Spec.Nodes.Num() > 1) { Errors.Add(TEXT("nodes but no edges: an unconnected graph never advances")); }

        // GUIDs: which existing nodes are kept, added or removed.
        TSet<FGuid> ExistingGuids;
        if (OutResolved.ExistingQuest)
        {
            for (const UAGSGraphNode* Node : OutResolved.ExistingQuest->AllNodes)
            {
                if (Node) { ExistingGuids.Add(Node->GetNodeId()); }
            }
        }
        TSet<FGuid> KeptGuids;
        for (const FAQSQuestNodeSpec& Node : Spec.Nodes)
        {
            if (Node.NodeGuid.IsEmpty()) { ++Report.AddedNodes; continue; }
            FGuid Guid;
            if (!FGuid::Parse(Node.NodeGuid, Guid))
            {
                Errors.Add(FString::Printf(TEXT("node '%s': NodeGuid '%s' is not a GUID"), *Node.Id, *Node.NodeGuid));
                continue;
            }
            if (KeptGuids.Contains(Guid))
            {
                Errors.Add(FString::Printf(TEXT("node '%s': NodeGuid %s is used twice"), *Node.Id, *Node.NodeGuid));
                continue;
            }
            KeptGuids.Add(Guid);
            OutResolved.GuidById.Add(Node.Id, Guid);
            if (ExistingGuids.Contains(Guid))
            {
                ++Report.KeptNodes;
            }
            else
            {
                ++Report.AddedNodes;
                Warnings.Add(FString::Printf(TEXT("node '%s': NodeGuid %s is not in the current quest; it is kept as given"), *Node.Id, *Node.NodeGuid));
            }
        }
        if (OutResolved.ExistingQuest)
        {
            for (const UAGSGraphNode* Node : OutResolved.ExistingQuest->AllNodes)
            {
                if (Node && !KeptGuids.Contains(Node->GetNodeId()))
                {
                    Report.RemovedNodes.Add(DescribeExistingNode(Node));
                }
            }
            if (Report.RemovedNodes.Num() > 0)
            {
                Warnings.Add(FString::Printf(TEXT("%d existing nodes are removed; save games in progress on them lose that progress"), Report.RemovedNodes.Num()));
            }
        }

        // Dry run every node: classes, tags, targets and properties.
        for (const FAQSQuestNodeSpec& Node : Spec.Nodes)
        {
            FString NodeError;
            if (!BuildNode(GetTransientPackage(), nullptr, Node, Warnings, NodeError))
            {
                Errors.Add(FString::Printf(TEXT("node '%s': %s"), *Node.Id, *NodeError));
            }
        }

        // Runtime registration.
        if (Spec.QuestsDB.IsEmpty())
        {
            if (Report.bCreatesAsset)
            {
                Warnings.Add(TEXT("no QuestsDB: StartQuest cannot find a quest that is not listed in the quest manager's DataTable"));
            }
        }
        else
        {
            FString DBPackage, DBObjectPath, DBError;
            const bool bValidDBPath = SplitAssetPath(Spec.QuestsDB, DBPackage, DBObjectPath, DBError);
            if (bValidDBPath && !DBPackage.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive))
            {
                DBError = TEXT("QuestsDB must be under /Game/ for quest writes");
            }
            const UDataTable* QuestsDB = DBError.IsEmpty()
                ? LoadObject<UDataTable>(nullptr, *DBObjectPath) : nullptr;
            if (!DBError.IsEmpty())
            {
                Errors.Add(TEXT("QuestsDB: ") + DBError);
            }
            else if (!QuestsDB)
            {
                Errors.Add(FString::Printf(TEXT("QuestsDB '%s' is not a DataTable"), *Spec.QuestsDB));
            }
            else if (QuestsDB->GetRowStruct() != FAQSQuestData::StaticStruct())
            {
                Errors.Add(FString::Printf(TEXT("QuestsDB '%s' has row struct %s, not AQSQuestData"), *Spec.QuestsDB,
                    QuestsDB->GetRowStruct() ? *QuestsDB->GetRowStruct()->GetName() : TEXT("none")));
            }
            else
            {
                const FName RowName(*(Spec.DBRowName.IsEmpty() ? FPackageName::GetShortName(OutResolved.PackageName) : Spec.DBRowName));
                if (const FAQSQuestData* Row = QuestsDB->FindRow<FAQSQuestData>(RowName, TEXT("AQSQuestToolset"), false))
                {
                    const UAQSQuest* RowQuest = Row->Quest;
                    if (RowQuest && RowQuest->GetPathName() != OutResolved.ObjectPath && !Spec.bOverwriteExisting)
                    {
                        Errors.Add(FString::Printf(TEXT("QuestsDB row '%s' already points at '%s'; set DBRowName or bOverwriteExisting"),
                            *RowName.ToString(), *RowQuest->GetPathName()));
                    }
                }
            }
        }

        Report.bValid = Errors.Num() == 0;
        return Report;
    }

    /** Left-to-right layering from the start node when the spec gives no positions. */
    static TMap<FString, FVector2D> ComputeLayout(const FAQSQuestSpec& Spec)
    {
        TMap<FString, FVector2D> Positions;
        const bool bHasPositions = Spec.Nodes.ContainsByPredicate([](const FAQSQuestNodeSpec& Node) { return !Node.Position.IsNearlyZero(); });
        if (bHasPositions)
        {
            for (const FAQSQuestNodeSpec& Node : Spec.Nodes) { Positions.Add(Node.Id, Node.Position); }
            return Positions;
        }

        TMap<FString, int32> Depth;
        TArray<FString> Queue;
        for (const FAQSQuestNodeSpec& Node : Spec.Nodes)
        {
            if (Node.Kind.Equals(TEXT("start"), ESearchCase::IgnoreCase)) { Depth.Add(Node.Id, 0); Queue.Add(Node.Id); }
        }
        for (int32 Index = 0; Index < Queue.Num(); ++Index)
        {
            const int32 Next = Depth[Queue[Index]] + 1;
            for (const FAQSQuestEdgeSpec& Edge : Spec.Edges)
            {
                if (Edge.From == Queue[Index] && !Depth.Contains(Edge.To)) { Depth.Add(Edge.To, Next); Queue.Add(Edge.To); }
            }
        }

        int32 MaxDepth = 0;
        for (const TPair<FString, int32>& Pair : Depth) { MaxDepth = FMath::Max(MaxDepth, Pair.Value); }
        TMap<int32, int32> RowsPerDepth;
        for (const FAQSQuestNodeSpec& Node : Spec.Nodes)
        {
            const int32 NodeDepth = Depth.Contains(Node.Id) ? Depth[Node.Id] : MaxDepth + 1;
            const int32 Row = RowsPerDepth.FindOrAdd(NodeDepth)++;
            Positions.Add(Node.Id, FVector2D(NodeDepth * 350.0, Row * 200.0));
        }
        return Positions;
    }

    static UEdGraph_AGSGraph* EnsureEditorGraph(UAQSQuest* Quest)
    {
        UEdGraph_AGSGraph* EdGraph = Cast<UEdGraph_AGSGraph>(Quest->EdGraph);
        if (!EdGraph)
        {
            Quest->EdGraph = NewObject<UEdGraph_AGSGraph>(Quest, UEdGraph_AGSGraph::StaticClass(), NAME_None, RF_Transactional);
            EdGraph = Cast<UEdGraph_AGSGraph>(Quest->EdGraph);
        }
        if (EdGraph)
        {
            EdGraph->Schema = UAssetGraphSchema_AGSGraph::StaticClass();
        }
        return EdGraph;
    }

    static UEdNode_AGSGraphNode* CreateEditorNode(UEdGraph_AGSGraph* EdGraph, UAGSGraphNode* RuntimeNode, const FVector2D& Position)
    {
        UEdNode_AGSGraphNode* EdNode = NewObject<UEdNode_AGSGraphNode>(EdGraph);
        EdNode->AGSGraphNode = RuntimeNode;
        EdNode->Rename(nullptr, EdGraph);
        EdGraph->AddNode(EdNode, true, false);
        EdNode->CreateNewGuid();
        EdNode->PostPlacedNewNode();
        EdNode->AllocateDefaultPins();
        EdNode->NodePosX = Position.X;
        EdNode->NodePosY = Position.Y;
        EdNode->SetFlags(RF_Transactional);
        RuntimeNode->SetNodePosition(Position);
        return EdNode;
    }

    /** Pins[1] is Out, Pins[0] is In. The schema inserts the edge node; RebuildAGSGraph derives the runtime edges from it. */
    static bool ConnectEditorNodes(UEdNode_AGSGraphNode* From, UEdNode_AGSGraphNode* To)
    {
        if (!From || !To || From->Pins.Num() < 2 || To->Pins.Num() < 2) { return false; }
        const UEdGraphSchema* Schema = From->GetSchema();
        if (!Schema || !Schema->TryCreateConnection(From->Pins[1], To->Pins[0])) { return false; }
        From->NodeConnectionListChanged();
        To->NodeConnectionListChanged();
        return true;
    }

    static bool RegisterInDB(const FAQSQuestSpec& Spec, UAQSQuest* Quest, FAQSQuestReport& Report, FString& OutError)
    {
        FString DBPackage, DBObjectPath;
        if (!SplitAssetPath(Spec.QuestsDB, DBPackage, DBObjectPath, OutError)) { return false; }
        if (!DBPackage.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive))
        {
            OutError = TEXT("QuestsDB must be under /Game/ for quest writes");
            return false;
        }
        UDataTable* QuestsDB = LoadObject<UDataTable>(nullptr, *DBObjectPath);
        if (!QuestsDB) { OutError = TEXT("QuestsDB did not load"); return false; }

        const FName RowName(*(Spec.DBRowName.IsEmpty() ? Quest->GetName() : Spec.DBRowName));
        FAQSQuestData Row;
        Row.Quest = Quest;
        QuestsDB->Modify();
        QuestsDB->RemoveRow(RowName);
        QuestsDB->AddRow(RowName, Row);
        if (!SavePackageOf(QuestsDB, OutError)) { return false; }
        Report.bRegisteredInDB = true;
        return true;
    }
}

TArray<FAQSQuestClassInfo> UAQSQuestToolset::ListClasses(const FString& Kind)
{
    UClass* Base = AQSQuestToolsetImpl::GetKindBaseClass(Kind);
    if (!Base)
    {
        UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("Unknown kind '%s'. Use objective, action or condition."), *Kind));
        return {};
    }
    return AQSQuestToolsetImpl::CollectClasses(Base);
}

TArray<FAQSQuestPropertyInfo> UAQSQuestToolset::DescribeClass(const FString& Class)
{
    UClass* Resolved = nullptr;
    for (UClass* Base : { UAQSQuestObjective::StaticClass(), UAGSAction::StaticClass(), UAGSCondition::StaticClass() })
    {
        FString Error;
        if ((Resolved = AQSQuestToolsetImpl::ResolveClass(Base, Class, Error)) != nullptr) { break; }
    }
    if (!Resolved && (Class.Equals(TEXT("AQSQuestObjective"), ESearchCase::IgnoreCase)))
    {
        Resolved = UAQSQuestObjective::StaticClass();
    }
    if (!Resolved)
    {
        UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("'%s' is not an installed objective, action or condition class."), *Class));
        return {};
    }

    TArray<FAQSQuestPropertyInfo> Result;
    const UObject* Defaults = Resolved->GetDefaultObject();
    for (TFieldIterator<FProperty> It(Resolved); It; ++It)
    {
        if (!AQSQuestToolsetImpl::IsAuthorableProperty(*It)) { continue; }

        FAQSQuestPropertyInfo Info;
        Info.Name = It->GetAuthoredName();
        Info.Type = It->GetCPPType();
        Info.DefaultValue = AQSQuestToolsetImpl::PropertyToString(Defaults, *It);
        Info.Description = It->GetToolTipText(true).ToString();

        const UEnum* Enum = nullptr;
        if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It)) { Enum = EnumProp->GetEnum(); }
        else if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It)) { Enum = ByteProp->Enum; }
        if (Enum)
        {
            TArray<FString> Values;
            for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) { Values.Add(Enum->GetNameStringByIndex(Index)); }
            Info.Description += FString::Printf(TEXT(" Values: %s."), *FString::Join(Values, TEXT(", ")));
        }
        Result.Add(Info);
    }
    return Result;
}

FAQSQuestSpec UAQSQuestToolset::ExportQuest(const FString& QuestPath)
{
    using namespace AQSQuestToolsetImpl;

    FAQSQuestSpec Spec;
    FString PackageName, ObjectPath, Error;
    if (!SplitAssetPath(QuestPath, PackageName, ObjectPath, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Spec;
    }
    const UAQSQuest* Quest = LoadObject<UAQSQuest>(nullptr, *ObjectPath);
    if (!Quest)
    {
        UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("No quest found at '%s'."), *QuestPath));
        return Spec;
    }

    Spec.AssetPath = PackageName;
    Spec.QuestTag = GetPropertyString(Quest, TEXT("QuestTag"));
    Spec.QuestName = GetTextPropertyString(Quest, TEXT("QuestName"));
    Spec.QuestDescription = GetTextPropertyString(Quest, TEXT("QuestDescription"));
    Spec.bOverwriteExisting = true;

    TSet<FString> BaseFields;
    for (const TCHAR* Field : BaseObjectiveFields) { BaseFields.Add(Field); }

    TMap<const UAGSGraphNode*, FString> IdByNode;
    TMap<FString, int32> CountByKind;
    for (const UAGSGraphNode* Node : Quest->AllNodes)
    {
        if (!Node) { continue; }

        FAQSQuestNodeSpec NodeSpec;
        NodeSpec.Kind = GetNodeKind(Node);
        const int32 Count = ++CountByKind.FindOrAdd(NodeSpec.Kind);
        NodeSpec.Id = NodeSpec.Kind == TEXT("start") ? NodeSpec.Kind : FString::Printf(TEXT("%s_%d"), *NodeSpec.Kind, Count);
        NodeSpec.NodeGuid = Node->GetNodeId().ToString();
        NodeSpec.Position = Node->GetNodePosition();
        NodeSpec.ActivationActions = GetInstancedArray(Node, TEXT("ActivationActions"));
        NodeSpec.CompletionActions = GetInstancedArray(Node, TEXT("OnCompletionActions"));

        if (Node->IsA<UAQSFinishQuestNode>())
        {
            NodeSpec.NextQuestTag = GetPropertyString(Node, TEXT("NextQuestTag"));
        }
        if (const UAQSObjectiveNode* ObjectiveNode = Cast<UAQSObjectiveNode>(Node))
        {
            NodeSpec.Repetitions = ObjectiveNode->GetQuestRepetitions();
            NodeSpec.CompleteConditions = GetInstancedArray(Node, TEXT("CompleteConditions"));
            if (const UAQSQuestObjective* Objective = ObjectiveNode->GetQuestObjective())
            {
                NodeSpec.ObjectiveClass = Objective->GetClass()->GetPathName();
                NodeSpec.ObjectiveName = GetTextPropertyString(Objective, TEXT("ObjectiveName"));
                NodeSpec.ObjectiveDescription = GetTextPropertyString(Objective, TEXT("ObjectiveDescription"));
                NodeSpec.ObjectiveTag = GetPropertyString(Objective, TEXT("ObjectiveTag"));
                NodeSpec.TargetRefType = GetPropertyString(Objective, TEXT("TargetRefType"));
                NodeSpec.ReferencedTargets = SplitList(GetPropertyString(Objective, TEXT("ReferencedTargets")));
                if (const FArrayProperty* ActorsProp = CastField<FArrayProperty>(FindPropertyAnyFlags(Objective, TEXT("ReferencedActors"))))
                {
                    FScriptArrayHelper Helper(ActorsProp, ActorsProp->ContainerPtrToValuePtr<void>(Objective));
                    for (int32 Index = 0; Index < Helper.Num(); ++Index)
                    {
                        FString Item;
                        ActorsProp->Inner->ExportTextItem_Direct(Item, Helper.GetRawPtr(Index), nullptr, const_cast<UAQSQuestObjective*>(Objective), PPF_None);
                        NodeSpec.ReferencedActors.Add(Item.TrimQuotes());
                    }
                }
                NodeSpec.ObjectiveProperties = ExportChangedProperties(Objective, BaseFields);
            }
        }

        IdByNode.Add(Node, NodeSpec.Id);
        Spec.Nodes.Add(NodeSpec);
    }

    for (const UAGSGraphNode* Node : Quest->AllNodes)
    {
        if (!Node) { continue; }
        for (const UAGSGraphNode* Child : Node->ChildrenNodes)
        {
            if (!Child || !IdByNode.Contains(Child)) { continue; }
            FAQSQuestEdgeSpec Edge;
            Edge.From = IdByNode[Node];
            Edge.To = IdByNode[Child];
            if (const UAQSEdge* QuestEdge = Cast<UAQSEdge>(Node->Edges.FindRef(const_cast<UAGSGraphNode*>(Child))))
            {
                if (!QuestEdge->GetTransitionName().IsNone()) { Edge.TransitionName = QuestEdge->GetTransitionName().ToString(); }
            }
            Spec.Edges.Add(Edge);
        }
    }
    return Spec;
}

FAQSQuestReport UAQSQuestToolset::ValidateQuest(const FAQSQuestSpec& Spec)
{
    AQSQuestToolsetImpl::FResolvedSpec Resolved;
    return AQSQuestToolsetImpl::Validate(Spec, Resolved);
}

FAQSQuestReport UAQSQuestToolset::SaveQuest(const FAQSQuestSpec& Spec)
{
    using namespace AQSQuestToolsetImpl;

    FResolvedSpec Resolved;
    FAQSQuestReport Report = Validate(Spec, Resolved);
    if (!Report.bValid)
    {
        UKismetSystemLibrary::RaiseScriptError(TEXT("Quest spec is not valid: ") + FString::Join(Report.Errors, TEXT(" ")));
        return Report;
    }

    FString Error;
    auto Fail = [&Report](const FString& Message)
    {
        Report.bValid = false;
        Report.Errors.Add(Message);
        UKismetSystemLibrary::RaiseScriptError(Message);
        return Report;
    };

    // Stage under a graph owner in the destination package: schema edge conversion reads
    // graph settings through the editor graph's outer, and text edits need the package identity.
    UPackage* Package = CreatePackage(*Resolved.PackageName);
    UAQSQuest* StagedQuest = NewObject<UAQSQuest>(Package, NAME_None, RF_Transient);
    UAQSQuest* Quest = Resolved.ExistingQuest;
    if (Quest)
    {
        for (const TCHAR* Name : { TEXT("QuestTag"), TEXT("QuestName"), TEXT("QuestDescription") })
        {
            FProperty* Property = FindPropertyAnyFlags(Quest, Name);
            if (Property)
            {
                Property->CopyCompleteValue_InContainer(StagedQuest, Quest);
            }
        }
    }

    TArray<TPair<const TCHAR*, FString>> QuestValues;
    QuestValues.Emplace(TEXT("QuestTag"), Spec.QuestTag);
    QuestValues.Emplace(TEXT("QuestName"), Spec.QuestName);
    QuestValues.Emplace(TEXT("QuestDescription"), Spec.QuestDescription);
    for (const TPair<const TCHAR*, FString>& Pair : QuestValues)
    {
        FProperty* Property = FindPropertyAnyFlags(StagedQuest, Pair.Key);
        if (!Property || !SetPropertyFromString(StagedQuest, Property, Pair.Value, Error))
        {
            return Fail(FString::Printf(TEXT("%s: %s"), Pair.Key, Property ? *Error : TEXT("property not found on AQSQuest")));
        }
    }

    // The entire replacement, including schema connections, must succeed before live mutation.
    TArray<FString> BuildWarnings;
    TArray<UAQSBaseNode*> NewNodes;
    for (const FAQSQuestNodeSpec& NodeSpec : Spec.Nodes)
    {
        UAQSBaseNode* Node = BuildNode(StagedQuest, StagedQuest, NodeSpec, BuildWarnings, Error);
        if (!Node)
        {
            return Fail(FString::Printf(TEXT("node '%s': %s"), *NodeSpec.Id, *Error));
        }
        if (const FGuid* Guid = Resolved.GuidById.Find(NodeSpec.Id))
        {
            Node->SetNodeId(*Guid);
        }
        NewNodes.Add(Node);
    }

    UEdGraph_AGSGraph* EdGraph = EnsureEditorGraph(StagedQuest);
    if (!EdGraph)
    {
        return Fail(TEXT("could not create the quest editor graph"));
    }

    const TMap<FString, FVector2D> Layout = ComputeLayout(Spec);
    TMap<FString, UEdNode_AGSGraphNode*> EditorById;
    TMap<FString, UAQSBaseNode*> RuntimeById;
    for (int32 Index = 0; Index < Spec.Nodes.Num(); ++Index)
    {
        const FString& Id = Spec.Nodes[Index].Id;
        EditorById.Add(Id, CreateEditorNode(EdGraph, NewNodes[Index], Layout.FindRef(Id)));
        RuntimeById.Add(Id, NewNodes[Index]);
    }

    for (const FAQSQuestEdgeSpec& Edge : Spec.Edges)
    {
        if (!ConnectEditorNodes(EditorById.FindRef(Edge.From), EditorById.FindRef(Edge.To)))
        {
            return Fail(FString::Printf(TEXT("the graph schema refused the connection '%s' -> '%s'"), *Edge.From, *Edge.To));
        }
    }

    // Staging is complete. Only now replace the asset, its editor graph and runtime nodes.
    if (!Quest)
    {
        Quest = NewObject<UAQSQuest>(Package, UAQSQuest::StaticClass(), *FPackageName::GetShortName(Resolved.PackageName),
            RF_Public | RF_Standalone | RF_Transactional);
    }
    else if (GEditor)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Quest);
    }
    Quest->Modify();
    for (const TPair<const TCHAR*, FString>& Pair : QuestValues)
    {
        FProperty* Property = FindPropertyAnyFlags(Quest, Pair.Key);
        Property->CopyCompleteValue_InContainer(Quest, StagedQuest);
    }
    if (UEdGraph* OldGraph = Quest->EdGraph)
    {
        OldGraph->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    }
    for (UAGSGraphNode* OldNode : Quest->AllNodes)
    {
        if (OldNode)
        {
            for (const TPair<UAGSGraphNode*, UAGSGraphEdge*>& Pair : OldNode->Edges)
            {
                if (Pair.Value && Pair.Value->GetOuter() == Quest)
                {
                    Pair.Value->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
                }
            }
            OldNode->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        }
    }
    EdGraph->Rename(nullptr, Quest, REN_DontCreateRedirectors | REN_NonTransactional);
    Quest->EdGraph = EdGraph;
    for (UAQSBaseNode* Node : NewNodes)
    {
        Node->Rename(nullptr, Quest, REN_DontCreateRedirectors | REN_NonTransactional);
        Node->Graph = Quest;
    }
    // Same call the AQS asset editor makes on save; its graph owner is now the live quest.
    EdGraph->RebuildAGSGraph();

    for (const FAQSQuestEdgeSpec& Edge : Spec.Edges)
    {
        if (Edge.TransitionName.IsEmpty()) { continue; }
        UAQSBaseNode* From = RuntimeById.FindRef(Edge.From);
        UAGSGraphEdge* RuntimeEdge = From ? From->Edges.FindRef(RuntimeById.FindRef(Edge.To)) : nullptr;
        FNameProperty* NameProp = RuntimeEdge ? CastField<FNameProperty>(FindPropertyAnyFlags(RuntimeEdge, TEXT("TransitionName"))) : nullptr;
        if (!NameProp)
        {
            BuildWarnings.Add(FString::Printf(TEXT("edge '%s' -> '%s': TransitionName could not be set"), *Edge.From, *Edge.To));
            continue;
        }
        NameProp->SetPropertyValue_InContainer(RuntimeEdge, FName(*Edge.TransitionName));
    }
    EdGraph->NotifyGraphChanged();

    if (!SavePackageOf(Quest, Error))
    {
        return Fail(Error);
    }
    if (Report.bCreatesAsset)
    {
        FAssetRegistryModule::AssetCreated(Quest);
    }

    if (!Spec.QuestsDB.IsEmpty() && !RegisterInDB(Spec, Quest, Report, Error))
    {
        Report.Warnings.Add(TEXT("quest saved, but QuestsDB registration failed: ") + Error);
    }

    for (const FString& Warning : BuildWarnings)
    {
        Report.Warnings.AddUnique(Warning);
    }
    Report.AssetPath = Quest->GetPathName();
    return Report;
}
