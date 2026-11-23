// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/AgenticMCPDataCommands.h"
#include "Commands/AgenticMCPCommonUtils.h"
#include "AgenticMCPModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/Blueprint.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsManager.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "UObject/Package.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectIterator.h"
#include "Misc/FileHelper.h"

// ============================================================================
// Shared Helpers
// ============================================================================
namespace AgenticDataUtils
{

static FString StripBlueprintHashSuffix(const FString& InName)
{
	const int32 UnderscoreIdx = InName.Find(TEXT("_"), ESearchCase::CaseSensitive);
	if (UnderscoreIdx > 0)
	{
		const FString After = InName.Mid(UnderscoreIdx + 1);
		const int32 SecondUnderscore = After.Find(TEXT("_"), ESearchCase::CaseSensitive);
		if (SecondUnderscore > 0 && After.Left(SecondUnderscore).IsNumeric())
		{
			return InName.Left(UnderscoreIdx);
		}
	}
	return InName;
}

static TSharedPtr<FJsonValue> PropertyToJson(const FProperty* Prop, const void* ContainerPtr);
static void SetPropertyValueFromJson(
	const FProperty* FinalProp,
	void* ValuePtr,
	const TSharedPtr<FJsonValue>& JsonValue,
	bool& bOutSuccess,
	FString& OutError);

static TSharedPtr<FJsonObject> StructToJsonObject(const UScriptStruct* ScriptStruct, const void* StructPtr)
{
	TSharedPtr<FJsonObject> StructJson = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
	{
		const FProperty* InnerProp = *It;
		const FString CleanName = StripBlueprintHashSuffix(InnerProp->GetName());
		TSharedPtr<FJsonValue> Val = PropertyToJson(InnerProp, StructPtr);
		if (Val.IsValid())
		{
			StructJson->SetField(CleanName, Val);
		}
	}
	return StructJson;
}

static TSharedPtr<FJsonValue> PropertyToJson(const FProperty* Prop, const void* ContainerPtr)
{
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue_InContainer(ContainerPtr));
	}
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
	{
		return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue_InContainer(ContainerPtr));
	}
	if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
	{
		return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue_InContainer(ContainerPtr));
	}
	if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
	{
		return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue_InContainer(ContainerPtr));
	}
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
	{
		return MakeShared<FJsonValueString>(StrProp->GetPropertyValue_InContainer(ContainerPtr));
	}
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
	{
		return MakeShared<FJsonValueString>(NameProp->GetPropertyValue_InContainer(ContainerPtr).ToString());
	}
	if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue_InContainer(ContainerPtr).ToString());
	}
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		const FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		const int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(ContainerPtr));
		if (const UEnum* Enum = EnumProp->GetEnum())
		{
			return MakeShared<FJsonValueString>(Enum->GetNameStringByValue(EnumValue));
		}
		return MakeShared<FJsonValueNumber>(EnumValue);
	}
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		if (ByteProp->Enum)
		{
			const uint8 Val = ByteProp->GetPropertyValue_InContainer(ContainerPtr);
			return MakeShared<FJsonValueString>(ByteProp->Enum->GetNameStringByValue(Val));
		}
		return MakeShared<FJsonValueNumber>(ByteProp->GetPropertyValue_InContainer(ContainerPtr));
	}
	if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
	{
		const UObject* Obj = ObjProp->GetObjectPropertyValue_InContainer(ContainerPtr);
		return MakeShared<FJsonValueString>(Obj ? Obj->GetPathName() : TEXT("None"));
	}
	if (const FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Prop))
	{
		const FSoftObjectPtr& SoftPtr = *SoftObjProp->GetPropertyValuePtr_InContainer(ContainerPtr);
		return MakeShared<FJsonValueString>(SoftPtr.ToString());
	}
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		const void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		const UScriptStruct* ScriptStruct = StructProp->Struct;

		if (ScriptStruct == TBaseStructure<FVector>::Get())
		{
			const FVector& V = *static_cast<const FVector*>(StructPtr);
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetNumberField(TEXT("X"), V.X);
			Obj->SetNumberField(TEXT("Y"), V.Y);
			Obj->SetNumberField(TEXT("Z"), V.Z);
			return MakeShared<FJsonValueObject>(Obj);
		}
		if (ScriptStruct == TBaseStructure<FRotator>::Get())
		{
			const FRotator& R = *static_cast<const FRotator*>(StructPtr);
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetNumberField(TEXT("Pitch"), R.Pitch);
			Obj->SetNumberField(TEXT("Yaw"), R.Yaw);
			Obj->SetNumberField(TEXT("Roll"), R.Roll);
			return MakeShared<FJsonValueObject>(Obj);
		}

		return MakeShared<FJsonValueObject>(StructToJsonObject(ScriptStruct, StructPtr));
	}
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr));
		for (int32 Idx = 0; Idx < ArrayHelper.Num(); ++Idx)
		{
			const void* ElemPtr = ArrayHelper.GetRawPtr(Idx);
			if (const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
			{
				JsonArray.Add(MakeShared<FJsonValueObject>(StructToJsonObject(InnerStruct->Struct, ElemPtr)));
			}
			else
			{
				TSharedPtr<FJsonValue> Val = PropertyToJson(ArrayProp->Inner, ElemPtr);
				if (Val.IsValid())
				{
					JsonArray.Add(Val);
				}
			}
		}
		return MakeShared<FJsonValueArray>(JsonArray);
	}

	FString ExportedValue;
	Prop->ExportTextItem_Direct(ExportedValue, Prop->ContainerPtrToValuePtr<void>(ContainerPtr), nullptr, nullptr, PPF_None);
	return MakeShared<FJsonValueString>(ExportedValue);
}

static const FProperty* FindPropertyByCleanName(const UStruct* Struct, const FString& CleanName)
{
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FProperty* Prop = *It;
		if (StripBlueprintHashSuffix(Prop->GetName()) == CleanName)
		{
			return Prop;
		}
	}
	return nullptr;
}

// Every GameplayTag DataTable the Asset Registry knows, wherever it lives: the project, ACF Ultimate, any other plugin. This module
// never names a product's content, so it behaves the same with or without ACF installed.
static TArray<FString> DiscoverGameplayTagTablePaths()
{
	FARFilter Filter;
	Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
	Filter.TagsAndValues.Add(FName(TEXT("RowStructure")), FGameplayTagTableRow::StaticStruct()->GetPathName());

	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);

	TArray<FString> Paths;
	Paths.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		Paths.Add(Asset.GetSoftObjectPath().ToString());
	}
	return Paths;
}

static UDataTable* LoadGameplayTagTable(const FString& TablePath)
{
	UDataTable* TagTable = FindObject<UDataTable>(nullptr, *TablePath);
	if (!TagTable)
	{
		TagTable = LoadObject<UDataTable>(nullptr, *TablePath);
	}
	if (!TagTable || TagTable->GetRowStruct() != FGameplayTagTableRow::StaticStruct())
	{
		return nullptr;
	}
	return TagTable;
}

static bool GameplayTagTableContains(const FString& TablePath, const FName& TagName)
{
	UDataTable* TagTable = LoadGameplayTagTable(TablePath);
	if (!TagTable)
	{
		return false;
	}

	TArray<FGameplayTagTableRow*> Rows;
	TagTable->GetAllRows<FGameplayTagTableRow>(TEXT("Yarin GameplayTag validation"), Rows);
	for (const FGameplayTagTableRow* Row : Rows)
	{
		if (Row && Row->Tag == TagName)
		{
			return true;
		}
	}
	return false;
}

static bool IsConfiguredGameplayTag(const FString& TagString)
{
	const FName TagName(*TagString);
	const UGameplayTagsSettings* Settings = GetDefault<UGameplayTagsSettings>();
	if (!Settings)
	{
		return false;
	}

	for (const FGameplayTagTableRow& TagRow : Settings->GameplayTagList)
	{
		if (TagRow.Tag == TagName)
		{
			return true;
		}
	}

	for (const FSoftObjectPath& TablePath : Settings->GameplayTagTableList)
	{
		if (GameplayTagTableContains(TablePath.ToString(), TagName))
		{
			return true;
		}
	}

	for (const FString& TablePath : DiscoverGameplayTagTablePaths())
	{
		if (GameplayTagTableContains(TablePath, TagName))
		{
			return true;
		}
	}

	return false;
}

static FGameplayTag ResolveGameplayTagForBridge(const FString& TagString)
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
	const FName TagName(*TagString);
	FGameplayTag Tag = TagManager.RequestGameplayTag(TagName, false);
	if (Tag.IsValid())
	{
		return Tag;
	}

	if (IsConfiguredGameplayTag(TagString))
	{
		return TagManager.AddNativeGameplayTag(
			TagName,
			TEXT("Registered by Yarin after resolving a configured GameplayTag DataTable row"));
	}

	return FGameplayTag();
}

struct FDiscoveredGameplayTag
{
	FString Tag;
	FString Source;
	FString SourcePath;
	FString RowName;
	FString DevComment;
};

static FString NormalizeGameplayTagRoot(FString Root)
{
	Root.TrimStartAndEndInline();
	if (Root.EndsWith(TEXT(".*")))
	{
		Root.LeftChopInline(2);
	}
	while (Root.EndsWith(TEXT(".")))
	{
		Root.LeftChopInline(1);
	}
	return Root;
}

static void AddUniqueString(TArray<FString>& Values, const FString& Value)
{
	if (Value.IsEmpty())
	{
		return;
	}
	for (const FString& Existing : Values)
	{
		if (Existing.Equals(Value, ESearchCase::IgnoreCase))
		{
			return;
		}
	}
	Values.Add(Value);
}

static TArray<FString> InferGameplayTagRootsFromHint(const FString& PropertyHint)
{
	TArray<FString> Roots;
	const FString Hint = PropertyHint.ToLower();
	if (Hint.IsEmpty())
	{
		return Roots;
	}

	if (Hint.Contains(TEXT("team")) || Hint.Contains(TEXT("faction")))
	{
		AddUniqueString(Roots, TEXT("Teams"));
	}
	if (Hint.Contains(TEXT("weapon")) || Hint.Contains(TEXT("itemtype")) || Hint.Contains(TEXT("item type")))
	{
		if (Hint.Contains(TEXT("weapon")))
		{
			AddUniqueString(Roots, TEXT("Item.Weapon"));
		}
		AddUniqueString(Roots, TEXT("Item"));
	}
	if (Hint.Contains(TEXT("item")) && !Hint.Contains(TEXT("itemslot")) && !Hint.Contains(TEXT("item slot")))
	{
		AddUniqueString(Roots, TEXT("Item"));
	}
	if (Hint.Contains(TEXT("slot")))
	{
		AddUniqueString(Roots, TEXT("ItemSlot"));
		AddUniqueString(Roots, TEXT("EquipSlot"));
	}
	if (Hint.Contains(TEXT("moveset")))
	{
		AddUniqueString(Roots, TEXT("Moveset"));
	}
	if (Hint.Contains(TEXT("stance")))
	{
		AddUniqueString(Roots, TEXT("Stances"));
	}
	if (Hint.Contains(TEXT("overlay")))
	{
		AddUniqueString(Roots, TEXT("Overlay"));
	}
	if (Hint.Contains(TEXT("action")) || Hint.Contains(TEXT("ability")))
	{
		AddUniqueString(Roots, TEXT("Actions"));
		AddUniqueString(Roots, TEXT("Ability"));
	}
	if (Hint.Contains(TEXT("stat")) || Hint.Contains(TEXT("attribute")) || Hint.Contains(TEXT("cost")))
	{
		AddUniqueString(Roots, TEXT("RPG.Statistics"));
		AddUniqueString(Roots, TEXT("RPG.Attributes"));
		AddUniqueString(Roots, TEXT("RPG.PrimaryAttributes"));
	}
	if (Hint.Contains(TEXT("rarity")))
	{
		AddUniqueString(Roots, TEXT("Rarity"));
	}
	if (Hint.Contains(TEXT("status")) || Hint.Contains(TEXT("buff")) || Hint.Contains(TEXT("effect")))
	{
		AddUniqueString(Roots, TEXT("Status"));
		AddUniqueString(Roots, TEXT("StatusEffect"));
	}
	if (Hint.Contains(TEXT("cue")))
	{
		AddUniqueString(Roots, TEXT("GameplayCue"));
	}
	if (Hint.Contains(TEXT("ai")) || Hint.Contains(TEXT("state")))
	{
		AddUniqueString(Roots, TEXT("AIState"));
		AddUniqueString(Roots, TEXT("AI"));
	}
	if (Hint.Contains(TEXT("craft")) || Hint.Contains(TEXT("recipe")) || Hint.Contains(TEXT("build")))
	{
		AddUniqueString(Roots, TEXT("Crafting"));
		AddUniqueString(Roots, TEXT("Building"));
	}

	return Roots;
}

static bool GameplayTagMatchesRoot(const FString& Tag, const FString& Root)
{
	const FString CleanRoot = NormalizeGameplayTagRoot(Root);
	return CleanRoot.IsEmpty()
		|| Tag.Equals(CleanRoot, ESearchCase::IgnoreCase)
		|| Tag.StartsWith(CleanRoot + TEXT("."), ESearchCase::IgnoreCase);
}

static bool GameplayTagMatchesAnyRoot(const FString& Tag, const TArray<FString>& Roots)
{
	if (Roots.Num() == 0)
	{
		return true;
	}

	for (const FString& Root : Roots)
	{
		if (GameplayTagMatchesRoot(Tag, Root))
		{
			return true;
		}
	}
	return false;
}

static void UpsertDiscoveredGameplayTag(
	TArray<FDiscoveredGameplayTag>& Tags,
	TMap<FString, int32>& IndexByTag,
	FDiscoveredGameplayTag Candidate)
{
	Candidate.Tag.TrimStartAndEndInline();
	if (Candidate.Tag.IsEmpty() || Candidate.Tag == TEXT("None"))
	{
		return;
	}

	const FString Key = Candidate.Tag.ToLower();
	if (const int32* ExistingIndex = IndexByTag.Find(Key))
	{
		FDiscoveredGameplayTag& Existing = Tags[*ExistingIndex];
		const bool bCandidateHasBetterSource = Existing.Source == TEXT("registry") && Candidate.Source != TEXT("registry");
		if (bCandidateHasBetterSource)
		{
			Existing.Source = Candidate.Source;
			Existing.SourcePath = Candidate.SourcePath;
			Existing.RowName = Candidate.RowName;
		}
		if (Existing.DevComment.IsEmpty() && !Candidate.DevComment.IsEmpty())
		{
			Existing.DevComment = Candidate.DevComment;
		}
		if (Existing.SourcePath.IsEmpty() && !Candidate.SourcePath.IsEmpty())
		{
			Existing.SourcePath = Candidate.SourcePath;
		}
		return;
	}

	IndexByTag.Add(Key, Tags.Num());
	Tags.Add(MoveTemp(Candidate));
}

static void CollectGameplayTagsFromTablePath(
	const FString& TablePath,
	TArray<FDiscoveredGameplayTag>& Tags,
	TMap<FString, int32>& IndexByTag)
{
	UDataTable* TagTable = LoadGameplayTagTable(TablePath);
	if (!TagTable)
	{
		return;
	}

	for (const auto& Pair : TagTable->GetRowMap())
	{
		const FGameplayTagTableRow* TagRow = reinterpret_cast<const FGameplayTagTableRow*>(Pair.Value);
		if (!TagRow)
		{
			continue;
		}

		FDiscoveredGameplayTag Row;
		Row.Tag = TagRow->Tag.ToString();
		Row.Source = TEXT("table");
		Row.SourcePath = TagTable->GetPathName();
		Row.RowName = Pair.Key.ToString();
		Row.DevComment = TagRow->DevComment;
		UpsertDiscoveredGameplayTag(Tags, IndexByTag, MoveTemp(Row));
	}
}

static TArray<FDiscoveredGameplayTag> CollectGameplayTags()
{
	TArray<FDiscoveredGameplayTag> Tags;
	TMap<FString, int32> IndexByTag;

	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
	FGameplayTagContainer RegistryTags;
	TagManager.RequestAllGameplayTags(RegistryTags, false);
	TArray<FGameplayTag> RegistryTagArray;
	RegistryTags.GetGameplayTagArray(RegistryTagArray);
	for (const FGameplayTag& Tag : RegistryTagArray)
	{
		if (!Tag.IsValid())
		{
			continue;
		}

		FDiscoveredGameplayTag Row;
		Row.Tag = Tag.ToString();
		Row.Source = TEXT("registry");
		UpsertDiscoveredGameplayTag(Tags, IndexByTag, MoveTemp(Row));
	}

	const UGameplayTagsSettings* Settings = GetDefault<UGameplayTagsSettings>();
	if (Settings)
	{
		for (const FGameplayTagTableRow& TagRow : Settings->GameplayTagList)
		{
			FDiscoveredGameplayTag Row;
			Row.Tag = TagRow.Tag.ToString();
			Row.Source = TEXT("settings");
			Row.SourcePath = TEXT("DefaultGameplayTags.ini");
			Row.RowName = Row.Tag;
			Row.DevComment = TagRow.DevComment;
			UpsertDiscoveredGameplayTag(Tags, IndexByTag, MoveTemp(Row));
		}

		for (const FSoftObjectPath& TablePath : Settings->GameplayTagTableList)
		{
			CollectGameplayTagsFromTablePath(TablePath.ToString(), Tags, IndexByTag);
		}
	}

	for (const FString& TablePath : DiscoverGameplayTagTablePaths())
	{
		CollectGameplayTagsFromTablePath(TablePath, Tags, IndexByTag);
	}

	Tags.Sort([](const FDiscoveredGameplayTag& A, const FDiscoveredGameplayTag& B)
	{
		return A.Tag < B.Tag;
	});
	return Tags;
}

static int32 ScoreGameplayTagMatch(
	const FDiscoveredGameplayTag& Row,
	const FString& Query,
	const TArray<FString>& Roots)
{
	int32 Score = 0;
	if (!Query.IsEmpty())
	{
		FString Leaf = Row.Tag;
		int32 LastDot = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), LastDot) && LastDot < Leaf.Len() - 1)
		{
			Leaf = Leaf.Mid(LastDot + 1);
		}

		if (Row.Tag.Equals(Query, ESearchCase::IgnoreCase))
		{
			Score += 1000;
		}
		else if (Leaf.Equals(Query, ESearchCase::IgnoreCase))
		{
			Score += 850;
		}
		else if (Row.Tag.StartsWith(Query, ESearchCase::IgnoreCase))
		{
			Score += 600;
		}
		else if (Leaf.StartsWith(Query, ESearchCase::IgnoreCase))
		{
			Score += 450;
		}
		else if (Row.Tag.Contains(Query, ESearchCase::IgnoreCase))
		{
			Score += 300;
		}
	}
	for (const FString& Root : Roots)
	{
		if (GameplayTagMatchesRoot(Row.Tag, Root))
		{
			Score += 120;
			break;
		}
	}
	if (Row.Source != TEXT("registry"))
	{
		Score += 20;
	}
	if (!Row.DevComment.IsEmpty())
	{
		Score += 5;
	}
	return Score;
}

static TSharedPtr<FJsonObject> GameplayTagToJson(
	const FDiscoveredGameplayTag& Row,
	const FString& Query,
	const TArray<FString>& Roots)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("tag"), Row.Tag);
	Obj->SetStringField(TEXT("source"), Row.Source);
	if (!Row.SourcePath.IsEmpty())
	{
		Obj->SetStringField(TEXT("source_path"), Row.SourcePath);
	}
	if (!Row.RowName.IsEmpty())
	{
		Obj->SetStringField(TEXT("row_name"), Row.RowName);
	}
	if (!Row.DevComment.IsEmpty())
	{
		Obj->SetStringField(TEXT("dev_comment"), Row.DevComment);
	}
	Obj->SetNumberField(TEXT("score"), ScoreGameplayTagMatch(Row, Query, Roots));
	return Obj;
}

static UObject* LoadAssetOrError(const FString& AssetPath, TSharedPtr<FJsonObject>& OutError)
{
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		const FString LeafName = FPaths::GetBaseFilename(AssetPath);
		Asset = LoadObject<UObject>(nullptr, *(AssetPath + TEXT(".") + LeafName));
	}
	if (!Asset)
	{
		OutError = FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
	}
	return Asset;
}

static UObject* ResolveEditableObject(UObject* Asset, UBlueprint*& OutBlueprint, FString& OutError)
{
	OutBlueprint = nullptr;
	OutError.Reset();

	if (!Asset)
	{
		OutError = TEXT("Asset is null");
		return nullptr;
	}

	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		OutBlueprint = Blueprint;
		if (!Blueprint->GeneratedClass)
		{
			OutError = FString::Printf(TEXT("Blueprint has no GeneratedClass: %s"), *Blueprint->GetPathName());
			return nullptr;
		}
		UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
		if (!CDO)
		{
			OutError = FString::Printf(TEXT("Blueprint has no class default object: %s"), *Blueprint->GetPathName());
			return nullptr;
		}
		return CDO;
	}

	if (UClass* Class = Cast<UClass>(Asset))
	{
		UObject* CDO = Class->GetDefaultObject();
		if (!CDO)
		{
			OutError = FString::Printf(TEXT("Class has no default object: %s"), *Class->GetPathName());
			return nullptr;
		}
		return CDO;
	}

	return Asset;
}

static bool TryResolveGameplayAttributeProperty(const FString& AttributePath, FProperty*& OutProperty, FString& OutError)
{
	OutProperty = nullptr;

	FString ClassPath;
	FString PropertyName;
	if (!AttributePath.Split(TEXT(":"), &ClassPath, &PropertyName) || ClassPath.IsEmpty() || PropertyName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("GameplayAttribute value '%s' must look like '/Script/Module.Class:Property'"), *AttributePath);
		return false;
	}

	UClass* OwnerClass = FindObject<UClass>(nullptr, *ClassPath);
	if (!OwnerClass)
	{
		OwnerClass = LoadObject<UClass>(nullptr, *ClassPath);
	}
	if (!OwnerClass)
	{
		FString ModulePath;
		FString ClassName;
		if (ClassPath.Split(TEXT("."), &ModulePath, &ClassName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			OwnerClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
		}
	}
	if (!OwnerClass)
	{
		OutError = FString::Printf(TEXT("Could not resolve GameplayAttribute owner class '%s'"), *ClassPath);
		return false;
	}

	OutProperty = FindFProperty<FProperty>(OwnerClass, *PropertyName);
	if (!OutProperty)
	{
		OutError = FString::Printf(TEXT("Could not resolve GameplayAttribute property '%s' on '%s'"), *PropertyName, *OwnerClass->GetName());
		return false;
	}

	return true;
}

static bool TrySetGameplayAttributeFromJson(void* ValuePtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
	FString AttributePath;
	if (JsonValue->TryGetString(AttributePath))
	{
		// accepted below
	}
	else
	{
		const TSharedPtr<FJsonObject>* AttributeObject = nullptr;
		if (!JsonValue->TryGetObject(AttributeObject) || !AttributeObject || !AttributeObject->IsValid())
		{
			OutError = TEXT("GameplayAttribute expects a string path or an object containing Attribute");
			return false;
		}

		(*AttributeObject)->TryGetStringField(TEXT("Attribute"), AttributePath);
		if (AttributePath.IsEmpty())
		{
			FString AttributeOwner;
			FString AttributeName;
			(*AttributeObject)->TryGetStringField(TEXT("AttributeOwner"), AttributeOwner);
			(*AttributeObject)->TryGetStringField(TEXT("AttributeName"), AttributeName);
			if (!AttributeOwner.IsEmpty() && !AttributeName.IsEmpty())
			{
				AttributePath = FString::Printf(TEXT("%s:%s"), *AttributeOwner, *AttributeName);
			}
		}
	}

	FProperty* AttributeProperty = nullptr;
	if (!TryResolveGameplayAttributeProperty(AttributePath, AttributeProperty, OutError))
	{
		return false;
	}

	*static_cast<FGameplayAttribute*>(ValuePtr) = FGameplayAttribute(AttributeProperty);
	return true;
}

static int32 SetStructFieldsFromJson(
	const UScriptStruct* ScriptStruct,
	void* StructData,
	const TSharedPtr<FJsonObject>& FieldsJson,
	TArray<FString>& OutErrors)
{
	int32 FieldsSet = 0;
	for (const auto& Pair : FieldsJson->Values)
	{
		const FString PairKeyStr(*Pair.Key);
		if (PairKeyStr.Equals(TEXT("row_name"), ESearchCase::IgnoreCase) ||
			PairKeyStr.Equals(TEXT("RowName"), ESearchCase::IgnoreCase) ||
			PairKeyStr.Equals(TEXT("name"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FProperty* Prop = FindPropertyByCleanName(ScriptStruct, PairKeyStr);
		if (!Prop)
		{
			OutErrors.Add(FString::Printf(TEXT("Field '%s' not found in struct '%s'"), *Pair.Key, *ScriptStruct->GetName()));
			continue;
		}

		void* ValuePtr = static_cast<uint8*>(StructData) + Prop->GetOffset_ForInternal();
		bool bSuccess = true;
		FString Error;
		SetPropertyValueFromJson(Prop, ValuePtr, Pair.Value, bSuccess, Error);
		if (bSuccess)
		{
			FieldsSet++;
		}
		else
		{
			OutErrors.Add(Error);
		}
	}
	return FieldsSet;
}

static void MarkEditableObjectChanged(UObject* Asset, UObject* EditableObject, UBlueprint* Blueprint)
{
	if (Asset)
	{
		Asset->Modify();
		Asset->MarkPackageDirty();
	}
	if (EditableObject && EditableObject != Asset)
	{
		EditableObject->Modify();
		EditableObject->MarkPackageDirty();
	}
	if (Blueprint)
	{
		Blueprint->Modify();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
	}
}

static void SetPropertyValueFromJson(
	const FProperty* FinalProp,
	void* ValuePtr,
	const TSharedPtr<FJsonValue>& JsonValue,
	bool& bOutSuccess,
	FString& OutError)
{
	bOutSuccess = true;

	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(FinalProp))
	{
		bool bNewVal = false;
		if (JsonValue->Type == EJson::Boolean)
		{
			bNewVal = JsonValue->AsBool();
		}
		else if (JsonValue->Type == EJson::String)
		{
			const FString Str = JsonValue->AsString().ToLower();
			bNewVal = (Str == TEXT("true") || Str == TEXT("1"));
		}
		else if (JsonValue->Type == EJson::Number)
		{
			bNewVal = JsonValue->AsNumber() != 0.0;
		}
		BoolProp->SetPropertyValue(ValuePtr, bNewVal);
	}
	else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(FinalProp))
	{
		FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(JsonValue->AsNumber()));
	}
	else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(FinalProp))
	{
		DoubleProp->SetPropertyValue(ValuePtr, JsonValue->AsNumber());
	}
	else if (const FIntProperty* IntProp = CastField<FIntProperty>(FinalProp))
	{
		IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(JsonValue->AsNumber()));
	}
	else if (const FStrProperty* StrProp = CastField<FStrProperty>(FinalProp))
	{
		StrProp->SetPropertyValue(ValuePtr, JsonValue->AsString());
	}
	else if (const FNameProperty* NameProp = CastField<FNameProperty>(FinalProp))
	{
		NameProp->SetPropertyValue(ValuePtr, FName(*JsonValue->AsString()));
	}
	else if (const FStructProperty* StructProp = CastField<FStructProperty>(FinalProp))
	{
		if (StructProp->Struct == FGameplayTag::StaticStruct())
		{
			FString TagString;
			if (!JsonValue->TryGetString(TagString))
			{
				const TSharedPtr<FJsonObject>* TagObject = nullptr;
				if (JsonValue->TryGetObject(TagObject) && TagObject && TagObject->IsValid())
				{
					(*TagObject)->TryGetStringField(TEXT("TagName"), TagString);
					if (TagString.IsEmpty())
					{
						(*TagObject)->TryGetStringField(TEXT("tag"), TagString);
					}
				}
			}
			const FGameplayTag Tag = ResolveGameplayTagForBridge(TagString);
			if (!Tag.IsValid())
			{
				bOutSuccess = false;
				OutError = FString::Printf(TEXT("Invalid GameplayTag '%s' for property '%s'"), *TagString, *FinalProp->GetName());
				return;
			}
			*static_cast<FGameplayTag*>(ValuePtr) = Tag;
		}
		else if (StructProp->Struct == FGameplayAttribute::StaticStruct())
		{
			if (!TrySetGameplayAttributeFromJson(ValuePtr, JsonValue, OutError))
			{
				bOutSuccess = false;
			}
		}
		else
		{
			const TSharedPtr<FJsonObject>* StructObject = nullptr;
			if (JsonValue->TryGetObject(StructObject) && StructObject && StructObject->IsValid())
			{
				TArray<FString> StructErrors;
				SetStructFieldsFromJson(StructProp->Struct, ValuePtr, *StructObject, StructErrors);
				if (StructErrors.Num() == 0)
				{
					return;
				}

				if (FJsonObjectConverter::JsonObjectToUStruct(StructObject->ToSharedRef(), StructProp->Struct, ValuePtr, 0, 0))
				{
					return;
				}

				bOutSuccess = false;
				OutError = FString::Join(StructErrors, TEXT("; "));
				return;
			}

			FString ValueStr;
			if (!JsonValue->TryGetString(ValueStr))
			{
				bOutSuccess = false;
				OutError = FString::Printf(TEXT("Cannot set struct property '%s' of type %s from non-object JSON value"),
					*FinalProp->GetName(), *FinalProp->GetCPPType());
				return;
			}
			if (!FinalProp->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None))
			{
				bOutSuccess = false;
				OutError = FString::Printf(TEXT("Cannot set struct property '%s' of type %s from value '%s'"),
					*FinalProp->GetName(), *FinalProp->GetCPPType(), *ValueStr);
			}
		}
	}
	else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(FinalProp))
	{
		const FString ValueStr = JsonValue->AsString();
		const UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		if (Enum && UnderlyingProp)
		{
			const int64 EnumVal = Enum->GetValueByNameString(ValueStr);
			if (EnumVal != INDEX_NONE)
			{
				UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumVal);
			}
			else
			{
				UnderlyingProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(FCString::Atoi(*ValueStr)));
			}
		}
	}
	else if (const FByteProperty* ByteProp = CastField<FByteProperty>(FinalProp))
	{
		if (ByteProp->Enum)
		{
			const int64 EnumVal = ByteProp->Enum->GetValueByNameString(JsonValue->AsString());
			*static_cast<uint8*>(ValuePtr) = (EnumVal != INDEX_NONE) ? static_cast<uint8>(EnumVal) : static_cast<uint8>(JsonValue->AsNumber());
		}
		else
		{
			*static_cast<uint8*>(ValuePtr) = static_cast<uint8>(JsonValue->AsNumber());
		}
	}
	else if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(FinalProp))
	{
		const FString ObjPath = JsonValue->AsString();
		UObject* NewObj = (ObjPath == TEXT("None") || ObjPath.IsEmpty()) ? nullptr : LoadObject<UObject>(nullptr, *ObjPath);
		ObjProp->SetObjectPropertyValue(ValuePtr, NewObj);
	}
	else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(FinalProp))
	{
		const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
		if (JsonValue->TryGetArray(JsonArray) && JsonArray)
		{
			FScriptArrayHelper Helper(ArrayProp, ValuePtr);
			Helper.Resize(JsonArray->Num());

			for (int32 Index = 0; Index < JsonArray->Num(); ++Index)
			{
				bool bElementSuccess = true;
				FString ElementError;
				SetPropertyValueFromJson(ArrayProp->Inner, Helper.GetRawPtr(Index), (*JsonArray)[Index], bElementSuccess, ElementError);
				if (!bElementSuccess)
				{
					bOutSuccess = false;
					OutError = FString::Printf(TEXT("Cannot set array property '%s' element %d: %s"),
						*FinalProp->GetName(), Index, *ElementError);
					return;
				}
			}
			return;
		}

		FString ValueStr;
		if (!JsonValue->TryGetString(ValueStr) ||
			!FinalProp->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None))
		{
			bOutSuccess = false;
			OutError = FString::Printf(TEXT("Cannot set array property '%s' of type %s from JSON value"),
				*FinalProp->GetName(), *FinalProp->GetCPPType());
		}
	}
	else
	{
		const FString ValueStr = JsonValue->AsString();
		if (!FinalProp->ImportText_Direct(*ValueStr, ValuePtr, nullptr, PPF_None))
		{
			bOutSuccess = false;
			OutError = FString::Printf(TEXT("Cannot set property '%s' of type %s from value '%s'"),
				*FinalProp->GetName(), *FinalProp->GetCPPType(), *ValueStr);
		}
	}
}

} // namespace AgenticDataUtils


// ============================================================================
// Command Router
// ============================================================================

TSharedPtr<FJsonObject> FAgenticMCPDataCommands::HandleCommand(
	const FString& InCommandType,
	const TSharedPtr<FJsonObject>& InParams)
{
	// DataAsset
	if (InCommandType == TEXT("da_inspect"))
		return HandleDaInspect(InParams);
	if (InCommandType == TEXT("da_set_array_property"))
		return HandleDaSetArrayProperty(InParams);

	// Gameplay Tags
	if (InCommandType == TEXT("gt_list"))
		return HandleGtList(InParams);
	if (InCommandType == TEXT("gt_search"))
		return HandleGtSearch(InParams);
	if (InCommandType == TEXT("gt_validate"))
		return HandleGtValidate(InParams);

	return FAgenticMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown data command: %s"), *InCommandType));
}


// ============================================================================
// DataAsset Handlers (da_*)
// ============================================================================

TSharedPtr<FJsonObject> FAgenticMCPDataCommands::HandleDaInspect(const TSharedPtr<FJsonObject>& InParams)
{
	FString AssetPath;
	if (!InParams->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
	}

	TSharedPtr<FJsonObject> LoadError;
	UObject* Asset = AgenticDataUtils::LoadAssetOrError(AssetPath, LoadError);
	if (!Asset) return LoadError;

	UBlueprint* Blueprint = nullptr;
	FString EditableError;
	UObject* EditableObject = AgenticDataUtils::ResolveEditableObject(Asset, Blueprint, EditableError);
	if (!EditableObject)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(EditableError);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset_path"), Asset->GetPathName());
	Result->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
	Result->SetStringField(TEXT("inspected_object_path"), EditableObject->GetPathName());
	Result->SetStringField(TEXT("class"), EditableObject->GetClass()->GetName());
	Result->SetBoolField(TEXT("inspected_blueprint_cdo"), Blueprint != nullptr);

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(EditableObject->GetClass()); It; ++It)
	{
		const FProperty* Prop = *It;
		if (Prop->GetOwnerClass() == UObject::StaticClass())
		{
			continue;
		}
		const FString CleanName = AgenticDataUtils::StripBlueprintHashSuffix(Prop->GetName());
		TSharedPtr<FJsonValue> Val = AgenticDataUtils::PropertyToJson(Prop, EditableObject);
		if (Val.IsValid())
		{
			Properties->SetField(CleanName, Val);
		}
	}

	Result->SetObjectField(TEXT("properties"), Properties);
	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FAgenticMCPDataCommands::HandleDaSetArrayProperty(const TSharedPtr<FJsonObject>& InParams)
{
	FString AssetPath;
	if (!InParams->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: asset_path"));
	}

	FString ArrayPropertyName;
	if (!InParams->TryGetStringField(TEXT("array_property"), ArrayPropertyName))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: array_property"));
	}

	FString FieldName;
	if (!InParams->TryGetStringField(TEXT("field_name"), FieldName))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: field_name"));
	}

	FString NewValueStr;
	if (!InParams->TryGetStringField(TEXT("new_value"), NewValueStr))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: new_value"));
	}

	FString IndicesStr;
	InParams->TryGetStringField(TEXT("indices"), IndicesStr);

	TSharedPtr<FJsonObject> LoadError;
	UObject* Asset = AgenticDataUtils::LoadAssetOrError(AssetPath, LoadError);
	if (!Asset) return LoadError;

	UBlueprint* Blueprint = nullptr;
	FString EditableError;
	UObject* EditableObject = AgenticDataUtils::ResolveEditableObject(Asset, Blueprint, EditableError);
	if (!EditableObject)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(EditableError);
	}

	FProperty* Prop = EditableObject->GetClass()->FindPropertyByName(*ArrayPropertyName);
	if (!Prop)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Property '%s' not found on %s"), *ArrayPropertyName, *EditableObject->GetClass()->GetName()));
	}

	FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
	if (!ArrayProp)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Property '%s' is not an array"), *ArrayPropertyName));
	}

	FStructProperty* InnerProp = CastField<FStructProperty>(ArrayProp->Inner);
	if (!InnerProp)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Array inner type is not a struct"));
	}

	TArray<FString> FieldPath;
	FieldName.ParseIntoArray(FieldPath, TEXT("."));

	UScriptStruct* CurrentStruct = InnerProp->Struct;
	FProperty* FieldProp = nullptr;
	int32 FieldOffsetFromElement = 0;

	for (int32 i = 0; i < FieldPath.Num(); ++i)
	{
		FProperty* StepProp = CurrentStruct->FindPropertyByName(*FieldPath[i]);
		if (!StepProp)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(
				TEXT("Field '%s' not found in struct '%s'"), *FieldPath[i], *CurrentStruct->GetName()));
		}

		if (i < FieldPath.Num() - 1)
		{
			FStructProperty* StructStep = CastField<FStructProperty>(StepProp);
			if (!StructStep)
			{
				return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(
					TEXT("'%s' is not a struct, cannot descend further"), *FieldPath[i]));
			}
			FieldOffsetFromElement += StepProp->GetOffset_ForInternal();
			CurrentStruct = StructStep->Struct;
		}
		else
		{
			FieldProp = StepProp;
			FieldOffsetFromElement += StepProp->GetOffset_ForInternal();
		}
	}

	if (!FieldProp)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Could not resolve field path"));
	}

	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(EditableObject));
	const int32 ArrayNum = ArrayHelper.Num();

	TArray<int32> TargetIndices;
	if (IndicesStr.IsEmpty() || IndicesStr == TEXT("all"))
	{
		for (int32 i = 0; i < ArrayNum; ++i)
		{
			TargetIndices.Add(i);
		}
	}
	else
	{
		TArray<FString> Parts;
		IndicesStr.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			const int32 Idx = FCString::Atoi(*Part.TrimStartAndEnd());
			if (Idx >= 0 && Idx < ArrayNum)
			{
				TargetIndices.Add(Idx);
			}
		}
	}

	AgenticDataUtils::MarkEditableObjectChanged(Asset, EditableObject, Blueprint);

	FString DetectedType;
	int32 Modified = 0;

	for (const int32 Idx : TargetIndices)
	{
		void* ElementPtr = ArrayHelper.GetRawPtr(Idx);
		void* FieldPtr = static_cast<uint8*>(ElementPtr) + FieldOffsetFromElement;

		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(FieldProp))
		{
			DetectedType = TEXT("Object");
			UObject* NewObj = StaticLoadObject(UObject::StaticClass(), nullptr, *NewValueStr);
			if (!NewObj && !NewValueStr.Equals(TEXT("None"), ESearchCase::IgnoreCase))
			{
				return FAgenticMCPCommonUtils::CreateErrorResponse(
					FString::Printf(TEXT("Failed to load object: %s"), *NewValueStr));
			}
			ObjProp->SetObjectPropertyValue(FieldPtr, NewObj);
			Modified++;
		}
		else if (CastField<FFloatProperty>(FieldProp))
		{
			DetectedType = TEXT("Float");
			*static_cast<float*>(FieldPtr) = FCString::Atof(*NewValueStr);
			Modified++;
		}
		else if (CastField<FDoubleProperty>(FieldProp))
		{
			DetectedType = TEXT("Double");
			*static_cast<double*>(FieldPtr) = FCString::Atod(*NewValueStr);
			Modified++;
		}
		else if (CastField<FIntProperty>(FieldProp))
		{
			DetectedType = TEXT("Int32");
			*static_cast<int32*>(FieldPtr) = FCString::Atoi(*NewValueStr);
			Modified++;
		}
		else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(FieldProp))
		{
			DetectedType = TEXT("Bool");
			const bool Val = NewValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || NewValueStr == TEXT("1");
			BoolProp->SetPropertyValue(FieldPtr, Val);
			Modified++;
		}
		else if (CastField<FStrProperty>(FieldProp))
		{
			DetectedType = TEXT("String");
			*static_cast<FString*>(FieldPtr) = NewValueStr;
			Modified++;
		}
		else if (CastField<FNameProperty>(FieldProp))
		{
			DetectedType = TEXT("Name");
			*static_cast<FName*>(FieldPtr) = FName(*NewValueStr);
			Modified++;
		}
		else if (FStructProperty* StructProp = CastField<FStructProperty>(FieldProp))
		{
			DetectedType = FString::Printf(TEXT("Struct(%s)"), *StructProp->Struct->GetName());

			if (StructProp->Struct == TBaseStructure<FVector>::Get())
			{
				TArray<FString> Components;
				FString Cleaned = NewValueStr.Replace(TEXT("["), TEXT("")).Replace(TEXT("]"), TEXT("")).TrimStartAndEnd();
				Cleaned.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() >= 3)
				{
					FVector* Vec = static_cast<FVector*>(FieldPtr);
					Vec->X = FCString::Atod(*Components[0].TrimStartAndEnd());
					Vec->Y = FCString::Atod(*Components[1].TrimStartAndEnd());
					Vec->Z = FCString::Atod(*Components[2].TrimStartAndEnd());
					Modified++;
				}
				else
				{
					return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(
						TEXT("Vector needs 3 components [x,y,z], got %d"), Components.Num()));
				}
			}
			else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
			{
				TArray<FString> Components;
				FString Cleaned = NewValueStr.Replace(TEXT("["), TEXT("")).Replace(TEXT("]"), TEXT("")).TrimStartAndEnd();
				Cleaned.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() >= 3)
				{
					FRotator* Rot = static_cast<FRotator*>(FieldPtr);
					Rot->Pitch = FCString::Atod(*Components[0].TrimStartAndEnd());
					Rot->Yaw = FCString::Atod(*Components[1].TrimStartAndEnd());
					Rot->Roll = FCString::Atod(*Components[2].TrimStartAndEnd());
					Modified++;
				}
				else
				{
					return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(
						TEXT("Rotator needs 3 components [pitch,yaw,roll], got %d"), Components.Num()));
				}
			}
			else
			{
				const TCHAR* ImportResult = StructProp->ImportText_Direct(*NewValueStr, FieldPtr, nullptr, PPF_None);
				if (ImportResult)
				{
					Modified++;
				}
				else
				{
					return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(
						TEXT("ImportText failed for struct '%s' with value: %s"),
						*StructProp->Struct->GetName(), *NewValueStr));
				}
			}
		}
		else if (FByteProperty* ByteProp = CastField<FByteProperty>(FieldProp))
		{
			DetectedType = TEXT("Byte/Enum");
			if (ByteProp->Enum)
			{
				const int64 EnumVal = ByteProp->Enum->GetValueByNameString(NewValueStr);
				if (EnumVal != INDEX_NONE)
				{
					*static_cast<uint8*>(FieldPtr) = static_cast<uint8>(EnumVal);
				}
				else
				{
					*static_cast<uint8*>(FieldPtr) = static_cast<uint8>(FCString::Atoi(*NewValueStr));
				}
			}
			else
			{
				*static_cast<uint8*>(FieldPtr) = static_cast<uint8>(FCString::Atoi(*NewValueStr));
			}
			Modified++;
		}
		else
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(
				TEXT("Unsupported field type: %s"), *FieldProp->GetClass()->GetName()));
		}
	}

	AgenticDataUtils::MarkEditableObjectChanged(Asset, EditableObject, Blueprint);
	UEditorAssetLibrary::SaveLoadedAsset(Asset);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
	ResultObj->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
	ResultObj->SetStringField(TEXT("edited_object"), EditableObject->GetPathName());
	ResultObj->SetStringField(TEXT("edited_class"), EditableObject->GetClass()->GetName());
	ResultObj->SetBoolField(TEXT("edited_blueprint_cdo"), Blueprint != nullptr);
	ResultObj->SetStringField(TEXT("array_property"), ArrayPropertyName);
	ResultObj->SetStringField(TEXT("field_name"), FieldName);
	ResultObj->SetStringField(TEXT("new_value"), NewValueStr);
	ResultObj->SetStringField(TEXT("detected_type"), DetectedType);
	ResultObj->SetNumberField(TEXT("array_size"), ArrayNum);
	ResultObj->SetNumberField(TEXT("modified_count"), Modified);
	return FAgenticMCPCommonUtils::CreateSuccessResponse(ResultObj);
}

// ============================================================================
// Gameplay Tag Handlers (gt_*)
// ============================================================================

TSharedPtr<FJsonObject> FAgenticMCPDataCommands::HandleGtList(const TSharedPtr<FJsonObject>& InParams)
{
	return HandleGtSearch(InParams);
}

TSharedPtr<FJsonObject> FAgenticMCPDataCommands::HandleGtSearch(const TSharedPtr<FJsonObject>& InParams)
{
	FString Query;
	InParams->TryGetStringField(TEXT("query"), Query);
	Query.TrimStartAndEndInline();

	FString Root;
	InParams->TryGetStringField(TEXT("root"), Root);
	Root = AgenticDataUtils::NormalizeGameplayTagRoot(Root);

	FString PropertyHint;
	InParams->TryGetStringField(TEXT("property_hint"), PropertyHint);

	int32 Limit = 50;
	double LimitNumber = 0.0;
	if (InParams->TryGetNumberField(TEXT("limit"), LimitNumber))
	{
		Limit = FMath::Clamp(static_cast<int32>(LimitNumber), 1, 500);
	}

	TArray<FString> RootFilters;
	if (!Root.IsEmpty())
	{
		AgenticDataUtils::AddUniqueString(RootFilters, Root);
	}
	else
	{
		RootFilters = AgenticDataUtils::InferGameplayTagRootsFromHint(PropertyHint);
	}

	TArray<AgenticDataUtils::FDiscoveredGameplayTag> AllTags = AgenticDataUtils::CollectGameplayTags();
	TArray<AgenticDataUtils::FDiscoveredGameplayTag> Matches;
	for (const AgenticDataUtils::FDiscoveredGameplayTag& Row : AllTags)
	{
		if (!AgenticDataUtils::GameplayTagMatchesAnyRoot(Row.Tag, RootFilters))
		{
			continue;
		}
		if (!Query.IsEmpty() && !Row.Tag.Contains(Query, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Matches.Add(Row);
	}

	Matches.Sort([&Query, &RootFilters](const AgenticDataUtils::FDiscoveredGameplayTag& A, const AgenticDataUtils::FDiscoveredGameplayTag& B)
	{
		const int32 ScoreA = AgenticDataUtils::ScoreGameplayTagMatch(A, Query, RootFilters);
		const int32 ScoreB = AgenticDataUtils::ScoreGameplayTagMatch(B, Query, RootFilters);
		if (ScoreA != ScoreB)
		{
			return ScoreA > ScoreB;
		}
		return A.Tag < B.Tag;
	});

	TArray<TSharedPtr<FJsonValue>> Items;
	for (int32 Index = 0; Index < Matches.Num() && Index < Limit; ++Index)
	{
		Items.Add(MakeShared<FJsonValueObject>(
			AgenticDataUtils::GameplayTagToJson(Matches[Index], Query, RootFilters)));
	}

	TArray<TSharedPtr<FJsonValue>> RootValues;
	for (const FString& Filter : RootFilters)
	{
		RootValues.Add(MakeShared<FJsonValueString>(Filter));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("query"), Query);
	Result->SetStringField(TEXT("root"), Root);
	Result->SetStringField(TEXT("property_hint"), PropertyHint);
	Result->SetNumberField(TEXT("total_tag_count"), AllTags.Num());
	Result->SetNumberField(TEXT("match_count"), Matches.Num());
	Result->SetNumberField(TEXT("returned_count"), Items.Num());
	Result->SetArrayField(TEXT("root_filters"), RootValues);
	Result->SetArrayField(TEXT("tags"), Items);
	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FAgenticMCPDataCommands::HandleGtValidate(const TSharedPtr<FJsonObject>& InParams)
{
	FString Tag;
	if (!InParams->TryGetStringField(TEXT("tag"), Tag))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'tag' parameter"));
	}
	Tag.TrimStartAndEndInline();

	FString PropertyHint;
	InParams->TryGetStringField(TEXT("property_hint"), PropertyHint);

	FString Root;
	InParams->TryGetStringField(TEXT("root"), Root);
	Root = AgenticDataUtils::NormalizeGameplayTagRoot(Root);

	TArray<FString> RootFilters;
	if (!Root.IsEmpty())
	{
		AgenticDataUtils::AddUniqueString(RootFilters, Root);
	}
	else
	{
		RootFilters = AgenticDataUtils::InferGameplayTagRootsFromHint(PropertyHint);
	}

	TArray<AgenticDataUtils::FDiscoveredGameplayTag> AllTags = AgenticDataUtils::CollectGameplayTags();
	TArray<TSharedPtr<FJsonValue>> Matches;
	for (const AgenticDataUtils::FDiscoveredGameplayTag& Row : AllTags)
	{
		if (Row.Tag.Equals(Tag, ESearchCase::CaseSensitive))
		{
			Matches.Add(MakeShared<FJsonValueObject>(
				AgenticDataUtils::GameplayTagToJson(Row, Tag, RootFilters)));
		}
	}

	TArray<AgenticDataUtils::FDiscoveredGameplayTag> Suggestions;
	if (Matches.Num() == 0)
	{
		FString Leaf = Tag;
		int32 LastDot = INDEX_NONE;
		if (Tag.FindLastChar(TEXT('.'), LastDot) && LastDot < Tag.Len() - 1)
		{
			Leaf = Tag.Mid(LastDot + 1);
		}

		for (const AgenticDataUtils::FDiscoveredGameplayTag& Row : AllTags)
		{
			if (!AgenticDataUtils::GameplayTagMatchesAnyRoot(Row.Tag, RootFilters))
			{
				continue;
			}
			if (Row.Tag.Contains(Tag, ESearchCase::IgnoreCase)
				|| Row.Tag.Contains(Leaf, ESearchCase::IgnoreCase))
			{
				Suggestions.Add(Row);
			}
		}

		if (Suggestions.Num() == 0 && RootFilters.Num() > 0)
		{
			for (const AgenticDataUtils::FDiscoveredGameplayTag& Row : AllTags)
			{
				if (AgenticDataUtils::GameplayTagMatchesAnyRoot(Row.Tag, RootFilters))
				{
					Suggestions.Add(Row);
				}
			}
		}

		Suggestions.Sort([&Tag, &RootFilters](const AgenticDataUtils::FDiscoveredGameplayTag& A, const AgenticDataUtils::FDiscoveredGameplayTag& B)
		{
			const int32 ScoreA = AgenticDataUtils::ScoreGameplayTagMatch(A, Tag, RootFilters);
			const int32 ScoreB = AgenticDataUtils::ScoreGameplayTagMatch(B, Tag, RootFilters);
			if (ScoreA != ScoreB)
			{
				return ScoreA > ScoreB;
			}
			return A.Tag < B.Tag;
		});
	}

	TArray<TSharedPtr<FJsonValue>> SuggestionValues;
	for (int32 Index = 0; Index < Suggestions.Num() && Index < 10; ++Index)
	{
		SuggestionValues.Add(MakeShared<FJsonValueObject>(
			AgenticDataUtils::GameplayTagToJson(Suggestions[Index], Tag, RootFilters)));
	}

	TArray<TSharedPtr<FJsonValue>> RootValues;
	for (const FString& Filter : RootFilters)
	{
		RootValues.Add(MakeShared<FJsonValueString>(Filter));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("tag"), Tag);
	Result->SetStringField(TEXT("root"), Root);
	Result->SetStringField(TEXT("property_hint"), PropertyHint);
	Result->SetBoolField(TEXT("valid"), Matches.Num() > 0);
	Result->SetBoolField(TEXT("is_registered"), Matches.Num() > 0);
	Result->SetNumberField(TEXT("total_tag_count"), AllTags.Num());
	Result->SetArrayField(TEXT("root_filters"), RootValues);
	Result->SetArrayField(TEXT("matches"), Matches);
	Result->SetArrayField(TEXT("suggestions"), SuggestionValues);
	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

