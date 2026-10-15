// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAIToolsUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompilerModule.h"
#include "Misc/OutputDevice.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectIterator.h"

namespace ACFAIToolsUtils
{
namespace
{
	constexpr int32 MaxStructDepth = 3;
	constexpr int32 MaxInlineDepth = 6;
	constexpr int32 MaxArrayIndex = 255;

	class FCapturedErrors : public FOutputDevice
	{
	public:
		FString Text;
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override
		{
			Text += Message;
		}
	};

	bool IsEditableProperty(const FProperty* Property)
	{
		return Property->HasAnyPropertyFlags(CPF_Edit) && !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Deprecated);
	}

	bool IsInlineObjectProperty(const FProperty* Property)
	{
		const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property);
		return ObjectProp && !CastField<FClassProperty>(Property) && !CastField<FSoftClassProperty>(Property)
			&& (ObjectProp->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance)
				|| (ObjectProp->PropertyClass && ObjectProp->PropertyClass->HasAnyClassFlags(CLASS_DefaultToInstanced)));
	}

	/** True for inline objects and for containers or structs that hold them; those cannot be written as text. */
	bool HoldsInlineObjects(const FProperty* Property)
	{
		return Property->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_PersistentInstance)
			|| IsInlineObjectProperty(Property);
	}

	bool IsAuthorableProperty(const FProperty* Property)
	{
		return IsEditableProperty(Property) && !HoldsInlineObjects(Property);
	}

	/** Structs from ACF modules are flattened into member paths; engine structs are written whole. */
	bool IsExpandableStruct(const UScriptStruct* Struct)
	{
		static const TSet<FString> AtomicPackages = {
			TEXT("/Script/CoreUObject"), TEXT("/Script/Engine"), TEXT("/Script/GameplayTags"), TEXT("/Script/GameplayAbilities")
		};
		if (AtomicPackages.Contains(Struct->GetOutermost()->GetName()))
		{
			return false;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (IsAuthorableProperty(*It))
			{
				return true;
			}
		}
		return false;
	}

	bool MatchesName(const FProperty* Property, const FString& Name)
	{
		return Property->GetName().Equals(Name, ESearchCase::IgnoreCase) || Property->GetAuthoredName().Equals(Name, ESearchCase::IgnoreCase);
	}

	FProperty* FindEditable(const UStruct* Struct, const FString& Name)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (IsEditableProperty(*It) && MatchesName(*It, Name))
			{
				return *It;
			}
		}
		return nullptr;
	}

	struct FResolvedPath
	{
		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FProperty* TopLevel = nullptr;
		/** Object that owns the value: the target itself or the inline object the path went through. */
		UObject* Owner = nullptr;
	};

	/** Splits "Name[3]" into "Name" and 3; a segment without brackets yields INDEX_NONE. */
	bool ParseSegment(const FString& Segment, FString& OutName, int32& OutIndex)
	{
		OutIndex = INDEX_NONE;
		int32 Open = INDEX_NONE;
		if (!Segment.FindChar(TEXT('['), Open))
		{
			OutName = Segment;
			return !OutName.IsEmpty();
		}
		const FString Digits = Segment.Mid(Open + 1, Segment.Len() - Open - 2);
		if (!Segment.EndsWith(TEXT("]")) || Digits.IsEmpty() || !Digits.IsNumeric() || Digits.Contains(TEXT("-")) || Digits.Contains(TEXT(".")))
		{
			return false;
		}
		OutName = Segment.Left(Open);
		OutIndex = FCString::Atoi(*Digits);
		return !OutName.IsEmpty();
	}

	/**
	 * Resolves "A.B[2].C" on Target through structs, array elements and existing inline objects.
	 * With bGrow, an index past the end grows the array to reach it.
	 */
	bool ResolvePath(UObject* Target, const FString& Path, const TSet<FName>& Skip, bool bGrow, FResolvedPath& Out, FString& OutError)
	{
		TArray<FString> Segments;
		Path.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.IsEmpty())
		{
			OutError = TEXT("empty property path");
			return false;
		}

		UObject* Owner = Target;
		const UStruct* Struct = Target->GetClass();
		void* Container = Target;
		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			FString Name;
			int32 ArrayIndex = INDEX_NONE;
			if (!ParseSegment(Segments[Index].TrimStartAndEnd(), Name, ArrayIndex))
			{
				OutError = FString::Printf(TEXT("'%s': '%s' is not a valid path segment"), *Path, *Segments[Index]);
				return false;
			}
			FProperty* Property = FindEditable(Struct, Name);
			if (!Property || (Index == 0 && Skip.Contains(Property->GetFName())))
			{
				OutError = FString::Printf(TEXT("'%s': %s has no writable property '%s' (see the Describe tool)"), *Path, *Struct->GetName(), *Name);
				return false;
			}
			if (Index == 0)
			{
				Out.TopLevel = Property;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
			if (ArrayIndex != INDEX_NONE)
			{
				FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
				if (!ArrayProp)
				{
					OutError = FString::Printf(TEXT("'%s': '%s' is not an array"), *Path, *Name);
					return false;
				}
				FScriptArrayHelper Helper(ArrayProp, ValuePtr);
				if (ArrayIndex >= Helper.Num())
				{
					if (!bGrow || ArrayIndex > MaxArrayIndex)
					{
						OutError = FString::Printf(TEXT("'%s': index %d is out of range, '%s' has %d elements"), *Path, ArrayIndex, *Name, Helper.Num());
						return false;
					}
					Helper.Resize(ArrayIndex + 1);
				}
				Property = ArrayProp->Inner;
				ValuePtr = Helper.GetRawPtr(ArrayIndex);
			}

			if (Index == Segments.Num() - 1)
			{
				Out.Property = Property;
				Out.ValuePtr = ValuePtr;
				Out.Owner = Owner;
				return true;
			}

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				Struct = StructProp->Struct;
				Container = ValuePtr;
				continue;
			}
			if (IsInlineObjectProperty(Property))
			{
				UObject* Object = CastField<FObjectPropertyBase>(Property)->GetObjectPropertyValue(ValuePtr);
				if (!Object)
				{
					OutError = FString::Printf(TEXT("'%s': '%s' is empty; create it first with an InlineObjects entry"), *Path, *Segments[Index]);
					return false;
				}
				Owner = Object;
				Struct = Object->GetClass();
				Container = Object;
				continue;
			}
			OutError = FString::Printf(TEXT("'%s': '%s' is not a struct or an inline object, so it has no members"), *Path, *Segments[Index]);
			return false;
		}
		return false;
	}

	bool RequestTag(const FString& TagName, FGameplayTag& OutTag, FString& OutError)
	{
		OutTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName.TrimStartAndEnd()), false);
		if (!OutTag.IsValid())
		{
			OutError = FString::Printf(TEXT("GameplayTag '%s' is not registered"), *TagName);
			return false;
		}
		return true;
	}

	TArray<FString> SplitList(const FString& Value)
	{
		TArray<FString> Items;
		Value.ParseIntoArray(Items, TEXT(","), true);
		for (FString& Item : Items)
		{
			Item.TrimStartAndEndInline();
		}
		Items.RemoveAll([](const FString& Item) { return Item.IsEmpty(); });
		return Items;
	}

	bool IsStructOf(const FProperty* Property, const UScriptStruct* Struct)
	{
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		return StructProp && StructProp->Struct == Struct;
	}

	/**
	 * GameplayTags, tag containers and tag arrays take bare tag names (comma separated for lists) checked
	 * against the registry; texts and strings are taken verbatim; other arrays without parentheses take
	 * comma separated items; everything else uses Unreal's ImportText format.
	 */
	bool SetValue(FProperty* Property, void* ValuePtr, UObject* Owner, const FString& Value, FString& OutError)
	{
		const FString Trimmed = Value.TrimStartAndEnd();

		if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			const bool bLocalizedSyntax = Trimmed.StartsWith(TEXT("NSLOCTEXT(")) || Trimmed.StartsWith(TEXT("LOCTEXT("))
				|| Trimmed.StartsWith(TEXT("INVTEXT(")) || Trimmed.StartsWith(TEXT("LOCTABLE("));
			if (!bLocalizedSyntax)
			{
				const FText& Current = *static_cast<const FText*>(ValuePtr);
				if (Current.ToString() == Value)
				{
					return true;
				}
				if (!TextNamespaceUtil::EditTextProperty_Direct(Owner->GetPackage(), ValuePtr, TextProp, TextNamespaceUtil::ETextEditAction::SourceString, Value))
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

		if (IsStructOf(Property, FGameplayTag::StaticStruct()) && !Trimmed.StartsWith(TEXT("(")))
		{
			FGameplayTag Tag;
			if (!Trimmed.IsEmpty() && !RequestTag(Trimmed, Tag, OutError))
			{
				return false;
			}
			*static_cast<FGameplayTag*>(ValuePtr) = Tag;
			return true;
		}
		if (IsStructOf(Property, FGameplayTagContainer::StaticStruct()) && !Trimmed.StartsWith(TEXT("(")))
		{
			FGameplayTagContainer Container;
			for (const FString& Item : SplitList(Trimmed))
			{
				FGameplayTag Tag;
				if (!RequestTag(Item, Tag, OutError))
				{
					return false;
				}
				Container.AddTag(Tag);
			}
			*static_cast<FGameplayTagContainer*>(ValuePtr) = Container;
			return true;
		}
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property); ArrayProp && !Trimmed.StartsWith(TEXT("(")))
		{
			FScriptArrayHelper Helper(ArrayProp, ValuePtr);
			Helper.EmptyValues();
			for (const FString& Item : SplitList(Trimmed))
			{
				const int32 Index = Helper.AddValue();
				if (IsStructOf(ArrayProp->Inner, FGameplayTag::StaticStruct()))
				{
					FGameplayTag Tag;
					if (!RequestTag(Item, Tag, OutError))
					{
						return false;
					}
					*reinterpret_cast<FGameplayTag*>(Helper.GetRawPtr(Index)) = Tag;
					continue;
				}
				FCapturedErrors Errors;
				if (!ArrayProp->Inner->ImportText_Direct(*Item, Helper.GetRawPtr(Index), Owner, PPF_None, &Errors))
				{
					OutError = FString::Printf(TEXT("'%s' is not a valid %s item for '%s' %s"), *Item,
						*ArrayProp->Inner->GetCPPType(), *Property->GetName(), *Errors.Text);
					return false;
				}
			}
			return true;
		}

		FCapturedErrors Errors;
		if (!Property->ImportText_Direct(*Trimmed, ValuePtr, Owner, PPF_None, &Errors) || !Errors.Text.IsEmpty())
		{
			OutError = FString::Printf(TEXT("'%s' is not a valid %s value for '%s' %s"), *Trimmed,
				*Property->GetCPPType(), *Property->GetName(), *Errors.Text);
			return false;
		}
		return true;
	}

	/** Inverse of SetValue. */
	FString ValueToString(const FProperty* Property, const void* ValuePtr, const UObject* Owner)
	{
		if (IsStructOf(Property, FGameplayTag::StaticStruct()))
		{
			const FGameplayTag& Tag = *static_cast<const FGameplayTag*>(ValuePtr);
			return Tag.IsValid() ? Tag.ToString() : FString();
		}
		if (IsStructOf(Property, FGameplayTagContainer::StaticStruct()))
		{
			TArray<FString> Names;
			for (const FGameplayTag& Tag : *static_cast<const FGameplayTagContainer*>(ValuePtr))
			{
				Names.Add(Tag.ToString());
			}
			return FString::Join(Names, TEXT(", "));
		}
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property); ArrayProp && IsStructOf(ArrayProp->Inner, FGameplayTag::StaticStruct()))
		{
			FScriptArrayHelper Helper(ArrayProp, ValuePtr);
			TArray<FString> Names;
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				Names.Add(reinterpret_cast<const FGameplayTag*>(Helper.GetRawPtr(Index))->ToString());
			}
			return FString::Join(Names, TEXT(", "));
		}
		if (CastField<FTextProperty>(Property))
		{
			return static_cast<const FText*>(ValuePtr)->ToString();
		}
		if (CastField<FStrProperty>(Property))
		{
			return *static_cast<const FString*>(ValuePtr);
		}
		if (CastField<FNameProperty>(Property))
		{
			return static_cast<const FName*>(ValuePtr)->ToString();
		}
		FString Text;
		Property->ExportTextItem_Direct(Text, ValuePtr, nullptr, const_cast<UObject*>(Owner), PPF_None);
		return Text;
	}

	FString DescribeType(const FProperty* Property)
	{
		FString Extended;
		FString Type = Property->GetCPPType(&Extended, CPPF_None) + Extended;

		if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			Type += FString::Printf(TEXT(" (subclass of %s)"), *ClassProp->MetaClass->GetName());
		}
		else if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
		{
			Type += FString::Printf(TEXT(" (subclass of %s)"), *SoftClassProp->MetaClass->GetName());
		}

		const UEnum* Enum = nullptr;
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProp->GetEnum();
		}
		else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			Enum = ByteProp->Enum;
		}
		if (Enum)
		{
			TArray<FString> Names;
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
			{
				if (!Enum->HasMetaData(TEXT("Hidden"), Index))
				{
					Names.Add(Enum->GetNameStringByIndex(Index));
				}
			}
			Type += FString::Printf(TEXT(" {%s}"), *FString::Join(Names, TEXT(", ")));
		}

		const FProperty* Inner = Property;
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			Inner = ArrayProp->Inner;
		}
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Inner);
			StructProp && StructProp->Struct != FGameplayTag::StaticStruct() && StructProp->Struct != FGameplayTagContainer::StaticStruct())
		{
			TArray<FString> Fields;
			for (TFieldIterator<FProperty> It(StructProp->Struct); It; ++It)
			{
				if (IsAuthorableProperty(*It))
				{
					Fields.Add(It->GetAuthoredName());
				}
			}
			if (Fields.Num())
			{
				Type += FString::Printf(TEXT(" fields: %s"), *FString::Join(Fields, TEXT(", ")));
			}
		}

		const FString Categories = Property->GetMetaData(TEXT("Categories"));
		if (!Categories.IsEmpty())
		{
			Type += FString::Printf(TEXT(" (tags under %s)"), *Categories);
		}
		return Type;
	}

	FString DescribeInlineSlot(const FProperty* Property)
	{
		const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property);
		return FString::Printf(TEXT("inline object, subclass of %s (set it with an InlineObjects entry; see ListInlineObjectClasses)"),
			ObjectProp && ObjectProp->PropertyClass ? *ObjectProp->PropertyClass->GetName() : TEXT("UObject"));
	}

	void DescribeStruct(const UStruct* Struct, const void* Container, const UObject* Owner, const FString& Prefix, int32 Depth,
		const TSet<FName>& Skip, bool bIncludeInline, TArray<FACFToolPropertyInfo>& Out);

	/** Inline object slots, and members reachable through structs or arrays that hold inline objects. */
	void DescribeInline(const FProperty* Property, const void* ValuePtr, const UObject* Owner, const FString& Path, int32 Depth,
		TArray<FACFToolPropertyInfo>& Out)
	{
		if (Depth > MaxInlineDepth)
		{
			return;
		}
		FACFToolPropertyInfo Info;
		Info.Description = Property->GetToolTipText(true).ToString();

		if (IsInlineObjectProperty(Property))
		{
			const UObject* Current = CastField<FObjectPropertyBase>(Property)->GetObjectPropertyValue(ValuePtr);
			Info.Name = Path;
			Info.Type = DescribeInlineSlot(Property);
			Info.DefaultValue = Current ? Current->GetClass()->GetName() : TEXT("None");
			Out.Add(Info);
			return;
		}
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			if (IsInlineObjectProperty(ArrayProp->Inner))
			{
				Info.Name = Path + TEXT("[i]");
				Info.Type = DescribeInlineSlot(ArrayProp->Inner);
				Info.DefaultValue = FString::Printf(TEXT("%d elements"), FScriptArrayHelper(ArrayProp, ValuePtr).Num());
				Out.Add(Info);
			}
			else if (const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
			{
				FStructOnScope Element(InnerStruct->Struct);
				DescribeStruct(InnerStruct->Struct, Element.GetStructMemory(), Owner, Path + TEXT("[i]."), Depth + 1, {}, true, Out);
			}
			return;
		}
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			DescribeStruct(StructProp->Struct, ValuePtr, Owner, Path + TEXT("."), Depth + 1, {}, true, Out);
		}
	}

	void DescribeStruct(const UStruct* Struct, const void* Container, const UObject* Owner, const FString& Prefix, int32 Depth,
		const TSet<FName>& Skip, bool bIncludeInline, TArray<FACFToolPropertyInfo>& Out)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (!IsEditableProperty(Property) || (Depth == 0 && Skip.Contains(Property->GetFName())))
			{
				continue;
			}
			const FString Path = Prefix + Property->GetAuthoredName();
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
			if (HoldsInlineObjects(Property))
			{
				if (bIncludeInline)
				{
					DescribeInline(Property, ValuePtr, Owner, Path, Depth, Out);
				}
				continue;
			}

			const FStructProperty* StructProp = CastField<FStructProperty>(Property);
			if (StructProp && Depth < MaxStructDepth && IsExpandableStruct(StructProp->Struct))
			{
				DescribeStruct(StructProp->Struct, ValuePtr, Owner, Path + TEXT("."), Depth + 1, Skip, bIncludeInline, Out);
				continue;
			}

			FACFToolPropertyInfo Info;
			Info.Name = Path;
			Info.Type = DescribeType(Property);
			Info.DefaultValue = ValueToString(Property, ValuePtr, Owner);
			Info.Description = Property->GetToolTipText(true).ToString();
			Out.Add(Info);
		}
	}

	/**
	 * Collects inline objects as path-addressed specs; paths are absolute from the exported asset.
	 * Each rebuilt inline object owns its relative clears, applied after construction and before its descendants.
	 */
	struct FInlineExport
	{
		TArray<FACFToolInlineObjectSpec>* Objects = nullptr;
		TArray<FString>* Cleared = nullptr;
		FString ClearPrefix;

		FInlineExport Nested() const { return *this; }
		void Clear(const FString& Path) const
		{
			if (Cleared)
			{
				Cleared->Add(Path.RightChop(ClearPrefix.Len()));
			}
		}
	};

	void ExportStruct(const UStruct* Struct, const void* Source, const void* Defaults, const UObject* Owner, const FString& Prefix,
		const FString& AbsolutePrefix, int32 Depth, const TSet<FName>& Skip, const FInlineExport* Inline, TMap<FString, FString>& Out);

	void ExportInlineObject(const UObject* Object, const FString& AbsolutePath, int32 Depth, const FInlineExport& Inline)
	{
		const int32 SpecIndex = Inline.Objects->AddDefaulted();
		(*Inline.Objects)[SpecIndex].Path = AbsolutePath;
		(*Inline.Objects)[SpecIndex].Class = Object->GetClass()->GetPathName();
		TMap<FString, FString> Properties;
		TArray<FString> Cleared;
		const FInlineExport Nested{ Inline.Objects, &Cleared, AbsolutePath + TEXT(".") };
		ExportStruct(Object->GetClass(), Object, Object->GetClass()->GetDefaultObject(), Object, FString(), AbsolutePath + TEXT("."),
			Depth + 1, {}, &Nested, Properties);
		(*Inline.Objects)[SpecIndex].Properties = MoveTemp(Properties);
		(*Inline.Objects)[SpecIndex].ClearedProperties = MoveTemp(Cleared);
	}

	/**
	 * Arrays that hold inline objects are listed in Cleared and re-exported element by element with every field,
	 * so saving the export rebuilds them exactly.
	 */
	void ExportInlineValue(const FProperty* Property, const void* SourceValue, const void* DefaultValue, const UObject* Owner,
		const FString& Path, const FString& AbsolutePath, int32 Depth, const FInlineExport& Inline, TMap<FString, FString>& Out)
	{
		if (Depth > MaxInlineDepth)
		{
			return;
		}
		if (IsInlineObjectProperty(Property))
		{
			const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property);
			const UObject* Object = ObjectProp->GetObjectPropertyValue(SourceValue);
			const UObject* DefaultObject = DefaultValue ? ObjectProp->GetObjectPropertyValue(DefaultValue) : nullptr;
			if (Object)
			{
				ExportInlineObject(Object, AbsolutePath, Depth, Inline);
			}
			else if (DefaultObject)
			{
				Inline.Clear(AbsolutePath);
			}
			return;
		}
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Source(ArrayProp, const_cast<void*>(SourceValue));
			const int32 DefaultNum = DefaultValue ? FScriptArrayHelper(ArrayProp, const_cast<void*>(DefaultValue)).Num() : 0;
			if (Source.Num() == 0 && DefaultNum == 0)
			{
				return;
			}
			Inline.Clear(AbsolutePath);
			const FInlineExport Nested = Inline.Nested();
			const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
			for (int32 Index = 0; Index < Source.Num(); ++Index)
			{
				const FString Element = FString::Printf(TEXT("[%d]"), Index);
				if (IsInlineObjectProperty(ArrayProp->Inner))
				{
					if (const UObject* Object = CastField<FObjectPropertyBase>(ArrayProp->Inner)->GetObjectPropertyValue(Source.GetRawPtr(Index)))
					{
						ExportInlineObject(Object, AbsolutePath + Element, Depth, Nested);
					}
				}
				else if (InnerStruct)
				{
					ExportStruct(InnerStruct->Struct, Source.GetRawPtr(Index), nullptr, Owner, Path + Element + TEXT("."),
						AbsolutePath + Element + TEXT("."), Depth + 1, {}, &Nested, Out);
				}
			}
			return;
		}
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			ExportStruct(StructProp->Struct, SourceValue, DefaultValue, Owner, Path + TEXT("."), AbsolutePath + TEXT("."), Depth + 1, {}, &Inline, Out);
		}
	}

	void ExportStruct(const UStruct* Struct, const void* Source, const void* Defaults, const UObject* Owner, const FString& Prefix,
		const FString& AbsolutePrefix, int32 Depth, const TSet<FName>& Skip, const FInlineExport* Inline, TMap<FString, FString>& Out)
	{
		const UClass* DefaultsClass = Defaults && Struct->IsA<UClass>() ? static_cast<const UObject*>(Defaults)->GetClass() : nullptr;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (!IsEditableProperty(Property) || (Depth == 0 && Skip.Contains(Property->GetFName())))
			{
				continue;
			}
			const FString Path = Prefix + Property->GetAuthoredName();
			const FString AbsolutePath = AbsolutePrefix + Property->GetAuthoredName();
			const void* SourceValue = Property->ContainerPtrToValuePtr<void>(Source);
			// Blueprint-only properties are missing from a native parent's defaults.
			const bool bDefaultsHaveIt = Defaults && (!DefaultsClass || DefaultsClass->IsChildOf(Property->GetOwnerClass()));
			const void* DefaultValue = bDefaultsHaveIt ? Property->ContainerPtrToValuePtr<void>(Defaults) : nullptr;

			if (HoldsInlineObjects(Property))
			{
				if (Inline)
				{
					ExportInlineValue(Property, SourceValue, DefaultValue, Owner, Path, AbsolutePath, Depth, *Inline, Out);
				}
				continue;
			}

			const FStructProperty* StructProp = CastField<FStructProperty>(Property);
			if (StructProp && Depth < MaxStructDepth && IsExpandableStruct(StructProp->Struct))
			{
				ExportStruct(StructProp->Struct, SourceValue, DefaultValue, Owner, Path + TEXT("."), AbsolutePath + TEXT("."), Depth + 1, Skip, Inline, Out);
				continue;
			}
			if (!DefaultValue || !Property->Identical(SourceValue, DefaultValue, PPF_None))
			{
				Out.Add(Path, ValueToString(Property, SourceValue, Owner));
			}
		}
	}

	EObjectFlags InstancedFlagsFor(const UObject* Outer)
	{
		return Outer->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
			? RF_Transactional | RF_ArchetypeObject | RF_Public
			: RF_Transactional;
	}

	FArrayProperty* FindObjectArray(const UObject* Owner, FName PropertyName, FObjectPropertyBase*& OutInner)
	{
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Owner->GetClass()->FindPropertyByName(PropertyName));
		OutInner = ArrayProp ? CastField<FObjectPropertyBase>(ArrayProp->Inner) : nullptr;
		return OutInner ? ArrayProp : nullptr;
	}

	bool SaveAsset(UObject* Asset, FString& OutError)
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

	TArray<UObject*> GetObjectArray(const UObject* Owner, FName PropertyName)
	{
		TArray<UObject*> Result;
		FObjectPropertyBase* Inner = nullptr;
		if (FArrayProperty* ArrayProp = FindObjectArray(Owner, PropertyName, Inner))
		{
			FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				if (UObject* Object = Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index)))
				{
					Result.Add(Object);
				}
			}
		}
		return Result;
	}
}

bool SplitAssetPath(const FString& InPath, FString& OutPackage, FString& OutObjectPath, FString& OutError)
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

UObject* FindExistingAsset(const FString& Package, const FString& ObjectPath)
{
	if (UObject* Found = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath))
	{
		return Found;
	}
	if (!FPackageName::DoesPackageExist(Package))
	{
		return nullptr;
	}
	return LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn);
}

TArray<FACFToolClassInfo> CollectClasses(UClass* Base)
{
	return CollectClasses(Base, [](const UClass*) { return true; });
}

TArray<FACFToolClassInfo> CollectClasses(UClass* Base, TFunctionRef<bool(const UClass* NativeClass)> NativeFilter)
{
	TMap<FString, FACFToolClassInfo> ByPath;
	const EClassFlags Excluded = EClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(Base) || Class->HasAnyClassFlags(Excluded) || Class->ClassGeneratedBy || !NativeFilter(Class))
		{
			continue;
		}
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		FACFToolClassInfo Info;
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
		if (!NativeClass || !NativeClass->IsChildOf(Base) || !NativeFilter(NativeClass))
		{
			continue;
		}
		FACFToolClassInfo Info;
		Info.Name = Asset.AssetName.ToString();
		Info.ClassPath = FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Info.Name);
		Info.Description = Asset.GetTagValueRef<FString>(FBlueprintTags::BlueprintDescription);
		ByPath.Add(Info.ClassPath, Info);
	}

	TArray<FACFToolClassInfo> Result;
	ByPath.GenerateValueArray(Result);
	Result.Sort([](const FACFToolClassInfo& A, const FACFToolClassInfo& B) { return A.Name < B.Name; });
	return Result;
}

UClass* ResolveClass(UClass* Base, const FString& NameOrPath, FString& OutError)
{
	const FString Wanted = NameOrPath.TrimStartAndEnd();
	if (Base->GetName().Equals(Wanted, ESearchCase::IgnoreCase) || Base->GetPathName().Equals(Wanted, ESearchCase::IgnoreCase))
	{
		return Base;
	}

	const TArray<FACFToolClassInfo> Classes = CollectClasses(Base);
	for (const FACFToolClassInfo& Info : Classes)
	{
		const bool bHit = Info.Name.Equals(Wanted, ESearchCase::IgnoreCase)
			|| (Info.Name + TEXT("_C")).Equals(Wanted, ESearchCase::IgnoreCase)
			|| Info.ClassPath.Equals(Wanted, ESearchCase::IgnoreCase)
			|| Info.ClassPath.StartsWith(Wanted + TEXT("."), ESearchCase::IgnoreCase);
		if (!bHit)
		{
			continue;
		}
		UClass* Loaded = LoadClass<UObject>(nullptr, *Info.ClassPath);
		if (Loaded && Loaded->IsChildOf(Base))
		{
			return Loaded;
		}
		OutError = FString::Printf(TEXT("class '%s' resolved to '%s' but failed to load"), *Wanted, *Info.ClassPath);
		return nullptr;
	}

	TArray<FString> Names;
	for (const FACFToolClassInfo& Info : Classes)
	{
		Names.Add(Info.Name);
	}
	OutError = FString::Printf(TEXT("unknown %s class '%s'. Installed: %s"), *Base->GetName(), *Wanted,
		Names.Num() ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
	return nullptr;
}

TArray<FACFToolPropertyInfo> DescribeProperties(const UClass* Class, const TSet<FName>& Skip, bool bIncludeInline)
{
	TArray<FACFToolPropertyInfo> Result;
	const UObject* Defaults = Class->GetDefaultObject();
	DescribeStruct(Class, Defaults, Defaults, FString(), 0, Skip, bIncludeInline, Result);
	return Result;
}

void ApplyProperties(UObject* Target, const TMap<FString, FString>& Properties, const TSet<FName>& Skip,
	const FString& ErrorPrefix, TArray<FString>& OutApplied, TArray<FString>& OutErrors)
{
	for (const TPair<FString, FString>& Pair : Properties)
	{
		FResolvedPath Resolved;
		FString Error;
		if (!ResolvePath(Target, Pair.Key, Skip, true, Resolved, Error))
		{
			OutErrors.Add(ErrorPrefix + Error);
			continue;
		}
		if (HoldsInlineObjects(Resolved.Property))
		{
			OutErrors.Add(ErrorPrefix + FString::Printf(TEXT("'%s' holds inline objects and cannot be written as text; set its members by path and its objects with InlineObjects"), *Pair.Key));
			continue;
		}
		if (!SetValue(Resolved.Property, Resolved.ValuePtr, Resolved.Owner, Pair.Value, Error))
		{
			OutErrors.Add(ErrorPrefix + Error);
			continue;
		}
		FPropertyChangedEvent ChangedEvent(Resolved.TopLevel, EPropertyChangeType::ValueSet);
		Target->PostEditChangeProperty(ChangedEvent);
		OutApplied.Add(ErrorPrefix + Pair.Key);
	}
}

void ClearProperties(UObject* Target, const TArray<FString>& Paths, const TSet<FName>& Skip,
	TArray<FString>& OutApplied, TArray<FString>& OutErrors)
{
	for (const FString& Path : Paths)
	{
		FResolvedPath Resolved;
		FString Error;
		if (!ResolvePath(Target, Path, Skip, true, Resolved, Error))
		{
			OutErrors.Add(TEXT("ClearProperties: ") + Error);
			continue;
		}
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Resolved.Property))
		{
			FScriptArrayHelper(ArrayProp, Resolved.ValuePtr).EmptyValues();
		}
		else if (FMapProperty* MapProp = CastField<FMapProperty>(Resolved.Property))
		{
			FScriptMapHelper(MapProp, Resolved.ValuePtr).EmptyValues();
		}
		else if (FSetProperty* SetProp = CastField<FSetProperty>(Resolved.Property))
		{
			FScriptSetHelper(SetProp, Resolved.ValuePtr).EmptyElements();
		}
		else
		{
			Resolved.Property->ClearValue(Resolved.ValuePtr);
		}
		FPropertyChangedEvent ChangedEvent(Resolved.TopLevel, EPropertyChangeType::ValueSet);
		Target->PostEditChangeProperty(ChangedEvent);
		OutApplied.Add(TEXT("cleared ") + Path);
	}
}

void ApplyInlineObjects(UObject* Target, const TArray<FACFToolInlineObjectSpec>& Specs, const TSet<FName>& Skip,
	TArray<FString>& OutApplied, TArray<FString>& OutErrors)
{
	for (int32 Index = 0; Index < Specs.Num(); ++Index)
	{
		const FACFToolInlineObjectSpec& Spec = Specs[Index];
		const FString Prefix = FString::Printf(TEXT("InlineObjects[%d] '%s': "), Index, *Spec.Path);
		FResolvedPath Resolved;
		FString Error;
		if (!ResolvePath(Target, Spec.Path, Skip, true, Resolved, Error))
		{
			OutErrors.Add(Prefix + Error);
			continue;
		}
		if (!IsInlineObjectProperty(Resolved.Property))
		{
			OutErrors.Add(Prefix + TEXT("this path is not an inline object slot (see the Describe tool)"));
			continue;
		}

		FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Resolved.Property);
		UObject* Object = ObjectProp->GetObjectPropertyValue(Resolved.ValuePtr);
		if (!Spec.Class.IsEmpty())
		{
			UClass* Class = ResolveClass(ObjectProp->PropertyClass, Spec.Class, Error);
			if (!Class)
			{
				OutErrors.Add(Prefix + Error);
				continue;
			}
			if (Class->HasAnyClassFlags(CLASS_Abstract))
			{
				OutErrors.Add(Prefix + FString::Printf(TEXT("%s is abstract; pick one of its subclasses"), *Class->GetName()));
				continue;
			}
			Object = NewObject<UObject>(Resolved.Owner, Class, NAME_None, InstancedFlagsFor(Resolved.Owner));
			ObjectProp->SetObjectPropertyValue(Resolved.ValuePtr, Object);
		}
		else if (!Object)
		{
			OutErrors.Add(Prefix + TEXT("the slot is empty, so Class is required"));
			continue;
		}

		ClearProperties(Object, Spec.ClearedProperties, {}, OutApplied, OutErrors);
		ApplyProperties(Object, Spec.Properties, {}, Prefix, OutApplied, OutErrors);
		FPropertyChangedEvent ChangedEvent(Resolved.TopLevel, EPropertyChangeType::ValueSet);
		Target->PostEditChangeProperty(ChangedEvent);
		OutApplied.Add(Spec.Path);
	}
}

TMap<FString, FString> ExportChangedProperties(const UObject* Source, const UObject* Defaults, const TSet<FName>& Skip,
	TArray<FACFToolInlineObjectSpec>* OutInlineObjects, TArray<FString>* OutCleared)
{
	TMap<FString, FString> Result;
	const FInlineExport Inline{ OutInlineObjects, OutCleared };
	ExportStruct(Source->GetClass(), Source, Defaults, Source, FString(), FString(), 0, Skip, OutInlineObjects ? &Inline : nullptr, Result);
	return Result;
}

bool BuildInstancedObjects(UObject* Outer, UClass* Base, const TArray<FACFToolObjectSpec>& Specs,
	const FString& ErrorPrefix, TArray<UObject*>& OutObjects, TArray<FString>& OutApplied, TArray<FString>& OutErrors)
{
	const int32 ErrorsBefore = OutErrors.Num();
	for (int32 Index = 0; Index < Specs.Num(); ++Index)
	{
		const FString Prefix = FString::Printf(TEXT("%s[%d]: "), *ErrorPrefix, Index);
		FString Error;
		UClass* Class = ResolveClass(Base, Specs[Index].Class, Error);
		if (!Class)
		{
			OutErrors.Add(Prefix + Error);
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			OutErrors.Add(Prefix + FString::Printf(TEXT("%s is abstract; pick one of its subclasses"), *Class->GetName()));
			continue;
		}
		UObject* Object = NewObject<UObject>(Outer, Class, NAME_None, InstancedFlagsFor(Outer));
		ClearProperties(Object, Specs[Index].ClearedProperties, {}, OutApplied, OutErrors);
		ApplyInlineObjects(Object, Specs[Index].InlineObjects, {}, OutApplied, OutErrors);
		ApplyProperties(Object, Specs[Index].Properties, {}, Prefix, OutApplied, OutErrors);
		OutObjects.Add(Object);
	}
	return OutErrors.Num() == ErrorsBefore;
}

FACFToolObjectSpec ExportInstancedObject(const UObject* Object)
{
	FACFToolObjectSpec Spec;
	if (Object)
	{
		Spec.Class = Object->GetClass()->GetPathName();
		Spec.Properties = ExportChangedProperties(Object, Object->GetClass()->GetDefaultObject(), {},
			&Spec.InlineObjects, &Spec.ClearedProperties);
	}
	return Spec;
}

bool SetObjectArray(UObject* Owner, FName PropertyName, const TArray<UObject*>& Objects, FString& OutError)
{
	FObjectPropertyBase* Inner = nullptr;
	FArrayProperty* ArrayProp = FindObjectArray(Owner, PropertyName, Inner);
	if (!ArrayProp)
	{
		OutError = FString::Printf(TEXT("%s has no object array '%s'"), *Owner->GetClass()->GetName(), *PropertyName.ToString());
		return false;
	}
	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Owner));
	Helper.EmptyValues();
	for (UObject* Object : Objects)
	{
		if (!Object->IsA(Inner->PropertyClass))
		{
			OutError = FString::Printf(TEXT("%s is not a %s"), *Object->GetClass()->GetName(), *Inner->PropertyClass->GetName());
			return false;
		}
		const int32 Index = Helper.AddValue();
		Inner->SetObjectPropertyValue(Helper.GetRawPtr(Index), Object);
	}
	FPropertyChangedEvent ChangedEvent(ArrayProp, EPropertyChangeType::ValueSet);
	Owner->PostEditChangeProperty(ChangedEvent);
	return true;
}

FACFToolReport Finish(FACFToolReport Report, bool bRaise)
{
	Report.bValid = Report.Errors.IsEmpty();
	if (!Report.bValid && bRaise)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Join(Report.Errors, TEXT("\n")));
	}
	return Report;
}

FACFToolReport SaveChecked(TFunctionRef<FACFToolReport(bool bSave)> Run)
{
	FACFToolReport Validation = Run(false);
	if (!Validation.Errors.IsEmpty())
	{
		return Finish(Validation, true);
	}
	FACFToolReport Report = Run(true);
	Report.Warnings = Validation.Warnings;
	return Finish(Report, true);
}

FACFToolReport RunBlueprintRequest(const FBlueprintRequest& Request, bool bSave, TFunctionRef<void(UObject* Defaults, FACFToolReport& Report)> ExtraChecks)
{
	FACFToolReport Report;
	FString Package, ObjectPath, Error;
	if (!SplitAssetPath(Request.AssetPath, Package, ObjectPath, Error))
	{
		Report.Errors.Add(Error);
		return Report;
	}
	Report.AssetPath = ObjectPath;
	if (!Package.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive))
	{
		Report.Errors.Add(FString::Printf(TEXT("'%s' is outside /Game/; writing engine or plugin content is not supported"), *Request.AssetPath));
		return Report;
	}

	UObject* ExistingObject = FindExistingAsset(Package, ObjectPath);
	UBlueprint* Existing = Cast<UBlueprint>(ExistingObject);
	UClass* Parent = nullptr;
	if (ExistingObject && !Existing)
	{
		Report.Errors.Add(FString::Printf(TEXT("'%s' already exists and is a %s, not a Blueprint"), *ObjectPath, *ExistingObject->GetClass()->GetName()));
	}
	else if (Existing)
	{
		if (!Request.bOverwriteExisting)
		{
			Report.Errors.Add(FString::Printf(TEXT("'%s' already exists; set bOverwriteExisting to update it"), *ObjectPath));
		}
		if (!Existing->GeneratedClass || !Existing->GeneratedClass->IsChildOf(Request.RequiredBase))
		{
			Report.Errors.Add(FString::Printf(TEXT("'%s' is not a %s Blueprint"), *ObjectPath, *Request.RequiredBase->GetName()));
		}
		else if (Existing->GeneratedClass->HasAnyClassFlags(CLASS_Abstract))
		{
			Report.Errors.Add(FString::Printf(TEXT("'%s' generates an abstract class and cannot be edited here"), *ObjectPath));
		}
		Parent = Existing->ParentClass;
		if (!Request.ParentClass.IsEmpty())
		{
			UClass* Wanted = ResolveClass(Request.RequiredBase, Request.ParentClass, Error);
			if (!Wanted)
			{
				Report.Errors.Add(Error);
			}
			else if (Wanted != Parent)
			{
				Report.Errors.Add(FString::Printf(TEXT("reparenting is not supported: '%s' derives from %s"), *ObjectPath, Parent ? *Parent->GetName() : TEXT("nothing")));
			}
		}
	}
	else
	{
		Report.bCreatesAsset = true;
		if (Request.ParentClass.IsEmpty())
		{
			Report.Errors.Add(TEXT("ParentClass is required to create a new Blueprint"));
		}
		else if (!(Parent = ResolveClass(Request.RequiredBase, Request.ParentClass, Error)))
		{
			Report.Errors.Add(Error);
		}
		else if (Parent->HasAnyClassFlags(CLASS_Abstract) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(Parent))
		{
			Report.Errors.Add(FString::Printf(TEXT("%s cannot be used as a Blueprint parent here"), *Parent->GetName()));
		}
	}
	if (!Report.Errors.IsEmpty())
	{
		return Report;
	}

	UBlueprint* Blueprint = Existing;
	UObject* Defaults = nullptr;
	if (bSave)
	{
		if (!Blueprint)
		{
			UPackage* NewPackage = CreatePackage(*Package);
			UClass* BlueprintClass = UBlueprint::StaticClass();
			UClass* GeneratedClass = UBlueprintGeneratedClass::StaticClass();
			FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(TEXT("KismetCompiler")).GetBlueprintTypesForClass(Parent, BlueprintClass, GeneratedClass);
			Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, NewPackage, FName(*FPackageName::GetShortName(Package)), BPTYPE_Normal, BlueprintClass, GeneratedClass);
			if (!Blueprint || !Blueprint->GeneratedClass)
			{
				Report.Errors.Add(FString::Printf(TEXT("failed to create a %s Blueprint at '%s'"), *Parent->GetName(), *ObjectPath));
				return Report;
			}
			FAssetRegistryModule::AssetCreated(Blueprint);
		}
		Blueprint->Modify();
		Defaults = Blueprint->GeneratedClass->GetDefaultObject();
		Defaults->Modify();
	}
	else
	{
		UClass* TemplateClass = Existing ? Existing->GeneratedClass.Get() : Parent;
		Defaults = NewObject<UObject>(GetTransientPackage(), TemplateClass);
	}

	if (Request.Properties)
	{
		ApplyProperties(Defaults, *Request.Properties, { Request.InstancedArrayProperty }, FString(), Report.AppliedProperties, Report.Errors);
	}
	if (!Request.InstancedArrayProperty.IsNone() && Request.InstancedSpecs && (Request.InstancedSpecs->Num() > 0 || Request.bClearInstanced))
	{
		TArray<UObject*> Objects;
		if (BuildInstancedObjects(Defaults, Request.InstancedBase, *Request.InstancedSpecs, Request.InstancedArrayProperty.ToString(),
				Objects, Report.AppliedProperties, Report.Errors)
			&& !SetObjectArray(Defaults, Request.InstancedArrayProperty, Objects, Error))
		{
			Report.Errors.Add(Error);
		}
	}
	ExtraChecks(Defaults, Report);

	if (bSave && Report.Errors.IsEmpty())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
		{
			Report.Errors.Add(FString::Printf(TEXT("Blueprint '%s' failed to compile; open it in the Blueprint editor"), *ObjectPath));
			return Report;
		}
		if (!SaveAsset(Blueprint, Error))
		{
			Report.Errors.Add(Error);
		}
	}
	return Report;
}

bool ExportBlueprint(const FString& AssetPath, UClass* RequiredBase, FName InstancedArrayProperty,
	FString& OutObjectPath, FString& OutParentClass, TMap<FString, FString>& OutProperties,
	TArray<FACFToolObjectSpec>& OutInstanced, FString& OutError)
{
	FString Package;
	if (!SplitAssetPath(AssetPath, Package, OutObjectPath, OutError))
	{
		return false;
	}
	const UBlueprint* Blueprint = Cast<UBlueprint>(FindExistingAsset(Package, OutObjectPath));
	if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(RequiredBase))
	{
		OutError = FString::Printf(TEXT("'%s' is not a %s Blueprint"), *AssetPath, *RequiredBase->GetName());
		return false;
	}

	const UObject* Defaults = Blueprint->GeneratedClass->GetDefaultObject();
	const UObject* ParentDefaults = Blueprint->ParentClass ? Blueprint->ParentClass->GetDefaultObject() : nullptr;
	OutParentClass = Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString();
	OutProperties = ExportChangedProperties(Defaults, ParentDefaults, { InstancedArrayProperty });
	if (!InstancedArrayProperty.IsNone())
	{
		for (const UObject* Object : GetObjectArray(Defaults, InstancedArrayProperty))
		{
			OutInstanced.Add(ExportInstancedObject(Object));
		}
	}
	return true;
}

FACFToolReport RunDataAssetRequest(const FString& AssetPath, UClass* Class, bool bOverwriteExisting, bool bSave,
	TFunctionRef<void(UObject* Target, FACFToolReport& Report)> Apply)
{
	FACFToolReport Report;
	FString Package, ObjectPath, Error;
	if (!SplitAssetPath(AssetPath, Package, ObjectPath, Error))
	{
		Report.Errors.Add(Error);
		return Report;
	}
	Report.AssetPath = ObjectPath;
	if (!Package.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive))
	{
		Report.Errors.Add(FString::Printf(TEXT("'%s' is outside /Game/; writing engine or plugin content is not supported"), *AssetPath));
		return Report;
	}

	UObject* Existing = FindExistingAsset(Package, ObjectPath);
	if (Existing && !Existing->IsA(Class))
	{
		Report.Errors.Add(FString::Printf(TEXT("'%s' already exists and is a %s, not a %s"), *ObjectPath, *Existing->GetClass()->GetName(), *Class->GetName()));
	}
	else if (Existing && !bOverwriteExisting)
	{
		Report.Errors.Add(FString::Printf(TEXT("'%s' already exists; set bOverwriteExisting to update it"), *ObjectPath));
	}
	if (!Existing && Class->HasAnyClassFlags(CLASS_Abstract))
	{
		Report.Errors.Add(FString::Printf(TEXT("%s is abstract"), *Class->GetName()));
	}
	Report.bCreatesAsset = Existing == nullptr;
	if (!Report.Errors.IsEmpty())
	{
		return Report;
	}

	UObject* Target = nullptr;
	if (bSave)
	{
		if (Existing)
		{
			Target = Existing;
			Target->Modify();
		}
		else
		{
			UPackage* NewPackage = CreatePackage(*Package);
			Target = NewObject<UObject>(NewPackage, Class, FName(*FPackageName::GetShortName(Package)), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Target);
		}
	}
	else
	{
		if (Existing)
		{
			FObjectDuplicationParameters Parameters(Existing, GetTransientPackage());
			Parameters.FlagMask &= ~(RF_Standalone | RF_Public);
			Target = StaticDuplicateObjectEx(Parameters);
		}
		else
		{
			Target = NewObject<UObject>(GetTransientPackage(), Class);
		}
	}

	Apply(Target, Report);

	if (bSave && Report.Errors.IsEmpty())
	{
		Target->PostEditChange();
		if (!SaveAsset(Target, Error))
		{
			Report.Errors.Add(Error);
		}
	}
	return Report;
}
}
