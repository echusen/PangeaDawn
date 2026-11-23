// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/AgenticMCPCommonUtils.h"
#include "AgenticMCPModule.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"

TSharedPtr<FJsonObject> FAgenticMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FAgenticMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);

    if (Data.IsValid())
    {
        for (const auto& Field : Data->Values)
        {
            ResponseObject->Values.Add(Field.Key, Field.Value);
        }
    }

    return ResponseObject;
}

FVector FAgenticMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result = FVector::ZeroVector;

    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = static_cast<float>((*JsonArray)[0]->AsNumber());
        Result.Y = static_cast<float>((*JsonArray)[1]->AsNumber());
        Result.Z = static_cast<float>((*JsonArray)[2]->AsNumber());
    }

    return Result;
}

FRotator FAgenticMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result = FRotator::ZeroRotator;

    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = static_cast<float>((*JsonArray)[0]->AsNumber());
        Result.Yaw = static_cast<float>((*JsonArray)[1]->AsNumber());
        Result.Roll = static_cast<float>((*JsonArray)[2]->AsNumber());
    }

    return Result;
}

UBlueprint* FAgenticMCPCommonUtils::FindBlueprintByName(const FString& InBlueprintName)
{
    const FString ObjectPath = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *InBlueprintName, *InBlueprintName);

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
    if (Blueprint)
    {
        return Blueprint;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
    if (AssetData.IsValid())
    {
        Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
        if (Blueprint)
        {
            return Blueprint;
        }
    }

    const FString PackagePath = TEXT("/Game/Blueprints/") + InBlueprintName;
    Blueprint = FindObject<UBlueprint>(nullptr, *PackagePath);
    if (!Blueprint)
    {
        UE_LOG(LogAgenticMCP, Error, TEXT("FindBlueprintByName: Failed to find blueprint: %s"), *InBlueprintName);
    }

    return Blueprint;
}

UK2Node_Event* FAgenticMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }

    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }

    // Return existing event node if one already exists
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            return EventNode;
        }
    }

    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    if (!EventFunction)
    {
        UE_LOG(LogAgenticMCP, Error, TEXT("CreateEventNode: Function not found: %s"), *EventName);
        return nullptr;
    }

    UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
    EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
    EventNode->NodePosX = Position.X;
    EventNode->NodePosY = Position.Y;
    Graph->AddNode(EventNode, true);
    EventNode->PostPlacedNewNode();
    EventNode->AllocateDefaultPins();
    return EventNode;
}

UEdGraphPin* FAgenticMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }

    // Exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            return Pin;
        }
    }

    // Case-insensitive fallback
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) &&
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            return Pin;
        }
    }

    // For variable get nodes, fall back to first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                return Pin;
            }
        }
    }

    UE_LOG(LogAgenticMCP, Warning, TEXT("FindPin: No matching pin '%s' on node '%s'"), *PinName, *Node->GetName());
    return nullptr;
}

static int64 ResolveEnumValueFromJson(UEnum* EnumDef, const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (Value->Type == EJson::Number)
    {
        return static_cast<int64>(Value->AsNumber());
    }

    if (Value->Type != EJson::String)
    {
        OutErrorMessage = TEXT("Enum value must be a number or string");
        return INDEX_NONE;
    }

    FString EnumValueName = Value->AsString();

    if (EnumValueName.IsNumeric())
    {
        return FCString::Atoi64(*EnumValueName);
    }

    // Strip qualified prefix (e.g., "EAutoReceiveInput::Player0" -> "Player0")
    if (EnumValueName.Contains(TEXT("::")))
    {
        EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
    }

    int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
    if (EnumValue == INDEX_NONE)
    {
        EnumValue = EnumDef->GetValueByNameString(Value->AsString());
    }

    if (EnumValue == INDEX_NONE)
    {
        UE_LOG(LogAgenticMCP, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
        for (int32 EnumIdx = 0; EnumIdx < EnumDef->NumEnums(); ++EnumIdx)
        {
            UE_LOG(LogAgenticMCP, Warning, TEXT("  - %s (value: %d)"),
                *EnumDef->GetNameStringByIndex(EnumIdx), EnumDef->GetValueByIndex(EnumIdx));
        }
        OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
    }

    return EnumValue;
}

// Leaf-value setter: given a resolved FProperty + container pointer, set the value from JSON.
// This is the type-dispatch logic extracted from SetObjectProperty so that both flat and
// dot-path resolution can share it.
static bool SetLeafPropertyFromJson(FProperty* Property, void* PropertyAddr,
    const TSharedPtr<FJsonValue>& Value, const FString& PropertyName, FString& OutErrorMessage)
{
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        BoolProp->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }

    if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        IntProp->SetPropertyValue(PropertyAddr, static_cast<int32>(Value->AsNumber()));
        return true;
    }

    if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        FloatProp->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }

    if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        DoubleProp->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }

    if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
    {
        Int64Prop->SetPropertyValue(PropertyAddr, static_cast<int64>(Value->AsNumber()));
        return true;
    }

    if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        StrProp->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }

    if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        NameProp->SetPropertyValue(PropertyAddr, FName(*Value->AsString()));
        return true;
    }

    if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        TextProp->SetPropertyValue(PropertyAddr, FText::FromString(Value->AsString()));
        return true;
    }

    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        UScriptStruct* Struct = StructProp->Struct;

        // FLinearColor — accepts {R,G,B,A} object or [r,g,b,a] array.
        if (Struct == TBaseStructure<FLinearColor>::Get())
        {
            FLinearColor Color = FLinearColor::Black;
            const TSharedPtr<FJsonObject>* AsObj = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* AsArr = nullptr;
            if (Value->TryGetObject(AsObj) && AsObj && AsObj->IsValid())
            {
                double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
                (*AsObj)->TryGetNumberField(TEXT("R"), R);
                (*AsObj)->TryGetNumberField(TEXT("G"), G);
                (*AsObj)->TryGetNumberField(TEXT("B"), B);
                (*AsObj)->TryGetNumberField(TEXT("A"), A);
                Color = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
            }
            else if (Value->TryGetArray(AsArr) && AsArr)
            {
                if (AsArr->Num() >= 1) Color.R = static_cast<float>((*AsArr)[0]->AsNumber());
                if (AsArr->Num() >= 2) Color.G = static_cast<float>((*AsArr)[1]->AsNumber());
                if (AsArr->Num() >= 3) Color.B = static_cast<float>((*AsArr)[2]->AsNumber());
                if (AsArr->Num() >= 4) Color.A = static_cast<float>((*AsArr)[3]->AsNumber()); else Color.A = 1.0f;
            }
            else
            {
                OutErrorMessage = FString::Printf(TEXT("FLinearColor expects object {R,G,B,A} or array [r,g,b,a] for %s"), *PropertyName);
                return false;
            }
            *static_cast<FLinearColor*>(PropertyAddr) = Color;
            return true;
        }

        // FVector / FVector3f — accepts {X,Y,Z} object or [x,y,z] array.
        // FVector is the LWC double-precision variant; FVector3f is the single-precision variant
        // (used by e.g. UMaterialExpressionConstant3Vector::Constant). Parse into double,
        // store into the appropriate underlying type.
        const bool bIsVector = (Struct == TBaseStructure<FVector>::Get());
        const bool bIsVector3f = (Struct == TVariantStructure<FVector3f>::Get());
        if (bIsVector || bIsVector3f)
        {
            double X = 0.0, Y = 0.0, Z = 0.0;
            const TSharedPtr<FJsonObject>* AsObj = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* AsArr = nullptr;
            if (Value->TryGetObject(AsObj) && AsObj && AsObj->IsValid())
            {
                (*AsObj)->TryGetNumberField(TEXT("X"), X);
                (*AsObj)->TryGetNumberField(TEXT("Y"), Y);
                (*AsObj)->TryGetNumberField(TEXT("Z"), Z);
            }
            else if (Value->TryGetArray(AsArr) && AsArr)
            {
                if (AsArr->Num() >= 1) X = (*AsArr)[0]->AsNumber();
                if (AsArr->Num() >= 2) Y = (*AsArr)[1]->AsNumber();
                if (AsArr->Num() >= 3) Z = (*AsArr)[2]->AsNumber();
            }
            else
            {
                OutErrorMessage = FString::Printf(TEXT("%s expects {X,Y,Z} or [x,y,z] for %s"),
                    bIsVector ? TEXT("FVector") : TEXT("FVector3f"), *PropertyName);
                return false;
            }
            if (bIsVector)
            {
                *static_cast<FVector*>(PropertyAddr) = FVector(X, Y, Z);
            }
            else
            {
                *static_cast<FVector3f*>(PropertyAddr) = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }
            return true;
        }

        // FVector2D / FVector2f — accepts {X,Y} object or [x,y] array.
        const bool bIsVector2D = (Struct == TBaseStructure<FVector2D>::Get());
        const bool bIsVector2f = (Struct == TVariantStructure<FVector2f>::Get());
        if (bIsVector2D || bIsVector2f)
        {
            double X = 0.0, Y = 0.0;
            const TSharedPtr<FJsonObject>* AsObj = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* AsArr = nullptr;
            if (Value->TryGetObject(AsObj) && AsObj && AsObj->IsValid())
            {
                (*AsObj)->TryGetNumberField(TEXT("X"), X);
                (*AsObj)->TryGetNumberField(TEXT("Y"), Y);
            }
            else if (Value->TryGetArray(AsArr) && AsArr)
            {
                if (AsArr->Num() >= 1) X = (*AsArr)[0]->AsNumber();
                if (AsArr->Num() >= 2) Y = (*AsArr)[1]->AsNumber();
            }
            else
            {
                OutErrorMessage = FString::Printf(TEXT("%s expects {X,Y} or [x,y] for %s"),
                    bIsVector2D ? TEXT("FVector2D") : TEXT("FVector2f"), *PropertyName);
                return false;
            }
            if (bIsVector2D)
            {
                *static_cast<FVector2D*>(PropertyAddr) = FVector2D(X, Y);
            }
            else
            {
                *static_cast<FVector2f*>(PropertyAddr) = FVector2f(static_cast<float>(X), static_cast<float>(Y));
            }
            return true;
        }

        // FVector4 / FVector4f — accepts {X,Y,Z,W} object or [x,y,z,w] array.
        // FVector4 is the LWC double-precision variant; FVector4f is the single-precision variant
        // (used by e.g. UMaterialExpressionFunctionInput::PreviewValue). Parse into double,
        // store into the appropriate underlying type.
        const bool bIsVector4 = (Struct == TBaseStructure<FVector4>::Get());
        const bool bIsVector4f = (Struct == TVariantStructure<FVector4f>::Get());
        if (bIsVector4 || bIsVector4f)
        {
            double X = 0.0, Y = 0.0, Z = 0.0, W = 0.0;
            const TSharedPtr<FJsonObject>* AsObj = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* AsArr = nullptr;
            if (Value->TryGetObject(AsObj) && AsObj && AsObj->IsValid())
            {
                (*AsObj)->TryGetNumberField(TEXT("X"), X);
                (*AsObj)->TryGetNumberField(TEXT("Y"), Y);
                (*AsObj)->TryGetNumberField(TEXT("Z"), Z);
                (*AsObj)->TryGetNumberField(TEXT("W"), W);
            }
            else if (Value->TryGetArray(AsArr) && AsArr)
            {
                if (AsArr->Num() >= 1) X = (*AsArr)[0]->AsNumber();
                if (AsArr->Num() >= 2) Y = (*AsArr)[1]->AsNumber();
                if (AsArr->Num() >= 3) Z = (*AsArr)[2]->AsNumber();
                if (AsArr->Num() >= 4) W = (*AsArr)[3]->AsNumber();
            }
            else
            {
                OutErrorMessage = FString::Printf(TEXT("%s expects {X,Y,Z,W} or [x,y,z,w] for %s"),
                    bIsVector4 ? TEXT("FVector4") : TEXT("FVector4f"), *PropertyName);
                return false;
            }
            if (bIsVector4)
            {
                *static_cast<FVector4*>(PropertyAddr) = FVector4(X, Y, Z, W);
            }
            else
            {
                *static_cast<FVector4f*>(PropertyAddr) = FVector4f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z), static_cast<float>(W));
            }
            return true;
        }

        // Generic struct fallback — use UE's reflection-based JSON converter for any USTRUCT.
        // Handles FLayerBlendInput, FExpressionInput, FRotator, FTransform, custom user structs, etc.
        const TSharedPtr<FJsonObject>* AsObj = nullptr;
        if (Value->TryGetObject(AsObj) && AsObj && AsObj->IsValid())
        {
            if (FJsonObjectConverter::JsonObjectToUStruct(AsObj->ToSharedRef(), Struct, PropertyAddr, 0, 0))
            {
                return true;
            }
            OutErrorMessage = FString::Printf(TEXT("FJsonObjectConverter failed to populate struct '%s' for property '%s'"),
                *Struct->GetName(), *PropertyName);
            return false;
        }

        OutErrorMessage = FString::Printf(TEXT("Struct '%s' for property '%s' expects JSON object (handlers: LinearColor/Vector(3f)/Vector2D(2f)/Vector4(4f) short forms accept {field:value} or [x,y,z,w]; any USTRUCT via {field:value, ...})"),
            *Struct->GetName(), *PropertyName);
        return false;
    }

    // Asset references (UMaterialFunction, UTexture, UPhysicalMaterial, etc.) — accept content-path string;
    // empty/null clears the reference. Validates that the loaded asset is compatible with the property's class.
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        FString AssetPath;
        if (!Value->TryGetString(AssetPath))
        {
            // Allow explicit null via JSON null
            if (Value->Type == EJson::Null)
            {
                ObjProp->SetObjectPropertyValue(PropertyAddr, nullptr);
                return true;
            }
            OutErrorMessage = FString::Printf(TEXT("ObjectProperty '%s' expects content-path string or null (got %d)"),
                *PropertyName, static_cast<int32>(Value->Type));
            return false;
        }

        if (AssetPath.IsEmpty())
        {
            ObjProp->SetObjectPropertyValue(PropertyAddr, nullptr);
            return true;
        }

        UObject* Asset = StaticLoadObject(ObjProp->PropertyClass, nullptr, *AssetPath);
        if (!Asset)
        {
            OutErrorMessage = FString::Printf(TEXT("Asset not found at '%s' for property '%s' (expected %s)"),
                *AssetPath, *PropertyName, *ObjProp->PropertyClass->GetName());
            return false;
        }
        if (!Asset->IsA(ObjProp->PropertyClass))
        {
            OutErrorMessage = FString::Printf(TEXT("Asset '%s' is %s, not compatible with property '%s' (expects %s)"),
                *AssetPath, *Asset->GetClass()->GetName(), *PropertyName, *ObjProp->PropertyClass->GetName());
            return false;
        }
        ObjProp->SetObjectPropertyValue(PropertyAddr, Asset);
        return true;
    }

    // Generic array (TArray<T>) — populate each element via UE's JSON converter, which recursively handles
    // structs, primitives, asset references via path strings, etc. Used by LandscapeLayerBlend.Layers,
    // MakeMaterialAttributes per-pin defaults, struct array UPROPERTYs, and many more.
    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        const TArray<TSharedPtr<FJsonValue>>* JsonArrayPtr = nullptr;
        if (!Value->TryGetArray(JsonArrayPtr) || !JsonArrayPtr)
        {
            OutErrorMessage = FString::Printf(TEXT("ArrayProperty '%s' expects JSON array"), *PropertyName);
            return false;
        }

        FScriptArrayHelper Helper(ArrayProp, PropertyAddr);
        Helper.Resize(JsonArrayPtr->Num());

        FProperty* InnerProp = ArrayProp->Inner;
        for (int32 i = 0; i < JsonArrayPtr->Num(); ++i)
        {
            void* ElementAddr = Helper.GetRawPtr(i);
            const TSharedPtr<FJsonValue>& Element = (*JsonArrayPtr)[i];
            if (!FJsonObjectConverter::JsonValueToUProperty(Element, InnerProp, ElementAddr, 0, 0))
            {
                OutErrorMessage = FString::Printf(TEXT("Failed to convert element [%d] of '%s' to %s"),
                    i, *PropertyName, *InnerProp->GetClass()->GetName());
                return false;
            }
        }
        return true;
    }

    if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
        if (EnumDef)
        {
            int64 EnumValue = ResolveEnumValueFromJson(EnumDef, Value, OutErrorMessage);
            if (EnumValue == INDEX_NONE)
            {
                return false;
            }
            ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
            return true;
        }

        ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(Value->AsNumber()));
        return true;
    }

    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        UEnum* EnumDef = EnumProp->GetEnum();
        FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
        if (EnumDef && UnderlyingProp)
        {
            int64 EnumValue = ResolveEnumValueFromJson(EnumDef, Value, OutErrorMessage);
            if (EnumValue == INDEX_NONE)
            {
                return false;
            }
            UnderlyingProp->SetIntPropertyValue(PropertyAddr, EnumValue);
            return true;
        }
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
        *Property->GetClass()->GetName(), *PropertyName);
    return false;
}

// Walk a dot-path (e.g. "Node.BlendMode" or "LayerSetup[0].BranchFilters") resolving
// through struct properties and array indices. When an array index exceeds the current
// size, the array is grown to accommodate (default-initialized elements).
static bool ResolvePropertyPath(void* RootContainer, UStruct* RootStruct, const FString& DotPath,
    FProperty*& OutLeafProp, void*& OutLeafContainer, FString& OutError)
{
    TArray<FString> Segments;
    DotPath.ParseIntoArray(Segments, TEXT("."), true);

    void* CurrentContainer = RootContainer;
    UStruct* CurrentStruct = RootStruct;

    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        FString Segment = Segments[i];
        const bool bIsLeaf = (i == Segments.Num() - 1);

        int32 ArrayIndex = INDEX_NONE;
        int32 OpenBracket = INDEX_NONE;
        if (Segment.FindChar('[', OpenBracket))
        {
            int32 CloseBracket = INDEX_NONE;
            if (!Segment.FindChar(']', CloseBracket) || CloseBracket <= OpenBracket)
            {
                OutError = FString::Printf(TEXT("malformed array index in segment '%s'"), *Segment);
                return false;
            }
            ArrayIndex = FCString::Atoi(*Segment.Mid(OpenBracket + 1, CloseBracket - OpenBracket - 1));
            Segment = Segment.Left(OpenBracket);
        }

        FProperty* Prop = CurrentStruct->FindPropertyByName(FName(*Segment));
        if (!Prop)
        {
            OutError = FString::Printf(TEXT("property '%s' not found on '%s' (full path: '%s')"),
                *Segment, *CurrentStruct->GetName(), *DotPath);
            return false;
        }

        if (ArrayIndex >= 0)
        {
            FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
            if (!ArrayProp)
            {
                OutError = FString::Printf(TEXT("'%s' is not an array property but [%d] was used"), *Segment, ArrayIndex);
                return false;
            }
            FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CurrentContainer));
            if (ArrayIndex >= Helper.Num())
            {
                Helper.Resize(ArrayIndex + 1);
            }
            CurrentContainer = Helper.GetRawPtr(ArrayIndex);
            Prop = ArrayProp->Inner;
        }

        if (bIsLeaf)
        {
            OutLeafProp = Prop;
            OutLeafContainer = CurrentContainer;
            return true;
        }

        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
        {
            CurrentContainer = StructProp->ContainerPtrToValuePtr<void>(CurrentContainer);
            CurrentStruct = StructProp->Struct;
            continue;
        }

        OutError = FString::Printf(TEXT("cannot descend into non-struct property '%s' (full path: '%s')"),
            *Segment, *DotPath);
        return false;
    }

    OutError = TEXT("empty property path");
    return false;
}

bool FAgenticMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName,
    const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    if (PropertyName.Contains(TEXT(".")))
    {
        FProperty* LeafProp = nullptr;
        void* LeafContainer = nullptr;
        if (!ResolvePropertyPath(Object, Object->GetClass(), PropertyName, LeafProp, LeafContainer, OutErrorMessage))
        {
            return false;
        }
        void* LeafAddr = LeafProp->ContainerPtrToValuePtr<void>(LeafContainer);
        return SetLeafPropertyFromJson(LeafProp, LeafAddr, Value, PropertyName, OutErrorMessage);
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    return SetLeafPropertyFromJson(Property, PropertyAddr, Value, PropertyName, OutErrorMessage);
}

bool FAgenticMCPCommonUtils::BuildPinTypeFromJson(const TSharedPtr<FJsonObject>& Spec,
                                                   FEdGraphPinType& OutPinType,
                                                   FString& OutErr)
{
    if (!Spec.IsValid())
    {
        OutErr = TEXT("type spec missing");
        return false;
    }
    FString Category;
    if (!Spec->TryGetStringField(TEXT("category"), Category))
    {
        OutErr = TEXT("type.category missing");
        return false;
    }
    OutPinType.PinCategory = FName(*Category);

    FString SubCategory;
    if (Spec->TryGetStringField(TEXT("subcategory"), SubCategory))
    {
        OutPinType.PinSubCategory = FName(*SubCategory);
    }
    if (OutPinType.PinCategory == UEdGraphSchema_K2::PC_Real
        && OutPinType.PinSubCategory.IsNone())
    {
        OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    }

    FString SubObjectPath;
    if (Spec->TryGetStringField(TEXT("subcategory_object"), SubObjectPath) && !SubObjectPath.IsEmpty())
    {
        UObject* SubObj = LoadObject<UObject>(nullptr, *SubObjectPath);
        if (!SubObj)
        {
            OutErr = FString::Printf(TEXT("subcategory_object not found: %s"), *SubObjectPath);
            return false;
        }
        OutPinType.PinSubCategoryObject = SubObj;
    }

    FString Container;
    if (Spec->TryGetStringField(TEXT("container"), Container))
    {
        if (Container == TEXT("Array"))      OutPinType.ContainerType = EPinContainerType::Array;
        else if (Container == TEXT("Set"))   OutPinType.ContainerType = EPinContainerType::Set;
        else if (Container == TEXT("Map"))   OutPinType.ContainerType = EPinContainerType::Map;
        else                                 OutPinType.ContainerType = EPinContainerType::None;
    }

    bool bRef = false, bConst = false;
    Spec->TryGetBoolField(TEXT("is_reference"), bRef);
    Spec->TryGetBoolField(TEXT("is_const"),     bConst);
    OutPinType.bIsReference = bRef;
    OutPinType.bIsConst     = bConst;
    return true;
}