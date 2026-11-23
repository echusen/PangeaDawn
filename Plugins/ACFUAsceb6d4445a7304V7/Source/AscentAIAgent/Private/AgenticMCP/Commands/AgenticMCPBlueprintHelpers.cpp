// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/AgenticMCPBlueprintHelpers.h"
#include "Commands/AgenticMCPCommonUtils.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/Paths.h"

#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"

namespace AgenticMCPBlueprintHelpers
{
    UBlueprint* LoadBlueprintOrError(const FString& Path, TSharedPtr<FJsonObject>& OutError)
    {
        UObject* Asset = LoadObject<UObject>(nullptr, *Path);
        if (!Asset)
        {
            const FString LeafName = FPaths::GetBaseFilename(Path);
            Asset = LoadObject<UObject>(nullptr, *(Path + TEXT(".") + LeafName));
        }
        if (!Asset)
        {
            OutError = FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Asset not found at %s (LoadObject returned null)"), *Path));
            return nullptr;
        }
        UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
        if (!Blueprint)
        {
            OutError = FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Expected UBlueprint, got %s at %s"),
                    *Asset->GetClass()->GetName(), *Path));
            return nullptr;
        }
        return Blueprint;
    }

    TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& InType)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("category"), InType.PinCategory.ToString());
        if (!InType.PinSubCategory.IsNone())
        {
            Obj->SetStringField(TEXT("subcategory"), InType.PinSubCategory.ToString());
        }
        if (UObject* SubObj = InType.PinSubCategoryObject.Get())
        {
            Obj->SetStringField(TEXT("subcategory_object"), SubObj->GetPathName());
        }
        switch (InType.ContainerType)
        {
            case EPinContainerType::None:  Obj->SetStringField(TEXT("container"), TEXT("None")); break;
            case EPinContainerType::Array: Obj->SetStringField(TEXT("container"), TEXT("Array")); break;
            case EPinContainerType::Set:   Obj->SetStringField(TEXT("container"), TEXT("Set")); break;
            case EPinContainerType::Map:   Obj->SetStringField(TEXT("container"), TEXT("Map")); break;
        }
        Obj->SetBoolField(TEXT("is_reference"), InType.bIsReference);
        Obj->SetBoolField(TEXT("is_const"), InType.bIsConst);
        if (InType.ContainerType == EPinContainerType::Map)
        {
            TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
            ValueObj->SetStringField(TEXT("category"), InType.PinValueType.TerminalCategory.ToString());
            if (UObject* SubObj = InType.PinValueType.TerminalSubCategoryObject.Get())
            {
                ValueObj->SetStringField(TEXT("subcategory_object"), SubObj->GetPathName());
            }
            Obj->SetObjectField(TEXT("value_type"), ValueObj);
        }
        return Obj;
    }

    UClass* ResolveClassByName(const FString& Spec)
    {
        if (Spec.IsEmpty()) return nullptr;
        if (UClass* Cls = LoadClass<UObject>(nullptr, *Spec))
        {
            return Cls;
        }
        if (UClass* Cls = UClass::TryFindTypeSlow<UClass>(Spec))
        {
            return Cls;
        }
        // Content-path fallback: load the asset; if it's a UBlueprint, return its GeneratedClass.
        if (Spec.StartsWith(TEXT("/")))
        {
            if (UObject* Asset = LoadObject<UObject>(nullptr, *Spec))
            {
                if (UBlueprint* BP = Cast<UBlueprint>(Asset))
                {
                    return BP->GeneratedClass;
                }
            }
        }
        return nullptr;
    }

    UFunction* ResolveFunctionByPath(const FString& Spec, FString& OutErr)
    {
        int32 ColonIdx = INDEX_NONE;
        if (!Spec.FindChar(TEXT(':'), ColonIdx))
        {
            OutErr = TEXT("Expected 'ClassPath:MethodName' for function spec");
            return nullptr;
        }
        const FString ClassPart = Spec.Left(ColonIdx);
        const FString FuncPart  = Spec.Mid(ColonIdx + 1);
        UClass* Cls = ResolveClassByName(ClassPart);
        if (!Cls)
        {
            OutErr = FString::Printf(TEXT("Class not found: %s"), *ClassPart);
            return nullptr;
        }
        // 1. Exact match
        UFunction* Fn = Cls->FindFunctionByName(*FuncPart);
        if (Fn) return Fn;

        // 2. K2_ prefix fallback — many BP-exposed functions have a hidden K2_ prefix
        //    (e.g. SetTimerByFunctionName → K2_SetTimerByFunctionName)
        if (!FuncPart.StartsWith(TEXT("K2_")))
        {
            Fn = Cls->FindFunctionByName(*FString::Printf(TEXT("K2_%s"), *FuncPart));
            if (Fn) return Fn;
        }

        // 3. Strip K2_ prefix if caller added it unnecessarily
        if (FuncPart.StartsWith(TEXT("K2_")))
        {
            Fn = Cls->FindFunctionByName(*FuncPart.Mid(3));
            if (Fn) return Fn;
        }

        // 4. Case-insensitive scan with best-match hint
        FString BestMatch;
        for (TFieldIterator<UFunction> It(Cls); It; ++It)
        {
            if (It->GetName().Equals(FuncPart, ESearchCase::IgnoreCase))
            {
                return *It;
            }
            if (BestMatch.IsEmpty() && It->GetName().Contains(FuncPart, ESearchCase::IgnoreCase))
            {
                BestMatch = It->GetName();
            }
        }

        OutErr = FString::Printf(TEXT("Function '%s' not found on class '%s'"), *FuncPart, *ClassPart);
        if (!BestMatch.IsEmpty())
        {
            OutErr += FString::Printf(TEXT(". Did you mean '%s'?"), *BestMatch);
        }
        return nullptr;
    }

    UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
    {
        TArray<UEdGraph*> All;
        Blueprint->GetAllGraphs(All);
        for (UEdGraph* G : All)
        {
            if (G && G->GetName() == GraphName) return G;
        }
        return nullptr;
    }

    UEdGraphNode* FindNodeByExactName(UEdGraph* Graph, const FString& NodeName)
    {
        if (!Graph) return nullptr;
        for (UEdGraphNode* N : Graph->Nodes)
        {
            if (N && N->GetName() == NodeName) return N;
        }
        return nullptr;
    }

    UEdGraphPin* FindPinByName(UEdGraphNode* Node, const FString& PinName,
                                EEdGraphPinDirection Direction)
    {
        if (!Node) return nullptr;
        for (UEdGraphPin* P : Node->Pins)
        {
            if (!P) continue;
            if (P->PinName.ToString() == PinName &&
                (Direction == EGPD_MAX || P->Direction == Direction))
            {
                return P;
            }
        }
        // Case-insensitive fallback for forgiving callers.
        for (UEdGraphPin* P : Node->Pins)
        {
            if (!P) continue;
            if (P->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) &&
                (Direction == EGPD_MAX || P->Direction == Direction))
            {
                return P;
            }
        }
        return nullptr;
    }

    TSharedPtr<FJsonObject> NodeToBriefJson(UEdGraphNode* Node)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("name"),  Node->GetName());
        O->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
        O->SetStringField(TEXT("guid"),  Node->NodeGuid.ToString());
        O->SetNumberField(TEXT("x"),     Node->NodePosX);
        O->SetNumberField(TEXT("y"),     Node->NodePosY);
        if (!Node->NodeComment.IsEmpty())
        {
            O->SetStringField(TEXT("comment"), Node->NodeComment);
        }
        return O;
    }

    TSharedPtr<FJsonObject> PinToBriefJson(UEdGraphPin* Pin)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("name"), Pin->PinName.ToString());
        O->SetStringField(TEXT("direction"),
            Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        O->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        O->SetBoolField  (TEXT("is_exec"),
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
        if (!Pin->DefaultValue.IsEmpty())
        {
            O->SetStringField(TEXT("default_value"), Pin->DefaultValue);
        }
        return O;
    }

    FString ApplyNodeConfig(UEdGraphNode* Node, UBlueprint* Blueprint,
                             const TSharedPtr<FJsonObject>& Config)
    {
        if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
        {
            if (!Config.IsValid()) return TEXT("CallFunction requires config.function = 'Class:Method'");
            FString FuncSpec;
            if (!Config->TryGetStringField(TEXT("function"), FuncSpec))
            {
                return TEXT("CallFunction config.function missing");
            }
            FString Err;
            UFunction* Fn = ResolveFunctionByPath(FuncSpec, Err);
            if (!Fn) return Err;
            CallNode->SetFromFunction(Fn);
            return FString();
        }
        // Mirror of Epic's BlueprintVariableNodeSpawner::MemberVarSetupLambda
        // (BlueprintVariableNodeSpawner.cpp:96-110): for a member variable on
        // the BP being edited (or any of its supers), bSelfContext must be true
        // so the BP compiler routes via Self instead of baking MemberParent =
        // GeneratedClass — that bake breaks compilation after recompile because
        // the post-recompile GeneratedClass is a different UObject identity.
        // Use the higher-level SetFromProperty so SelfContextInfo + VariableReference
        // are written together, matching the editor's drag-drop spawner exactly.
        auto ResolveSelfContextVarProp = [Blueprint](const FString& VarName, FProperty*& OutProp, UClass*& OutOwner, bool& bOutSelfContext) -> FString
        {
            UClass* SearchClass = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass : Blueprint->GeneratedClass;
            OutProp = FindFProperty<FProperty>(SearchClass, *VarName);
            if (!OutProp) return FString::Printf(TEXT("Variable '%s' not found on Blueprint"), *VarName);
            OutOwner = OutProp->GetOwnerClass();
            const bool bOwnerIsSelf = SearchClass && OutOwner && (SearchClass->GetAuthoritativeClass() == OutOwner || SearchClass->IsChildOf(OutOwner));
            const bool bIsFunctionLocal = OutProp->GetOwner<UFunction>() != nullptr;
            bOutSelfContext = bOwnerIsSelf && !bIsFunctionLocal;
            return FString();
        };

        if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
        {
            FString VarName;
            if (!Config.IsValid() || !Config->TryGetStringField(TEXT("variable"), VarName))
            {
                return TEXT("VariableGet config.variable missing");
            }
            FProperty* Prop = nullptr;
            UClass* OwnerClass = nullptr;
            bool bSelfContext = false;
            const FString Err = ResolveSelfContextVarProp(VarName, Prop, OwnerClass, bSelfContext);
            if (!Err.IsEmpty()) return Err;
            GetNode->SetFromProperty(Prop, bSelfContext, OwnerClass);
            return FString();
        }
        if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
        {
            FString VarName;
            if (!Config.IsValid() || !Config->TryGetStringField(TEXT("variable"), VarName))
            {
                return TEXT("VariableSet config.variable missing");
            }
            FProperty* Prop = nullptr;
            UClass* OwnerClass = nullptr;
            bool bSelfContext = false;
            const FString Err = ResolveSelfContextVarProp(VarName, Prop, OwnerClass, bSelfContext);
            if (!Err.IsEmpty()) return Err;
            SetNode->SetFromProperty(Prop, bSelfContext, OwnerClass);
            return FString();
        }
        if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
        {
            FString TgtSpec;
            if (!Config.IsValid() || !Config->TryGetStringField(TEXT("target_class"), TgtSpec))
            {
                return TEXT("DynamicCast config.target_class missing");
            }
            UClass* Tgt = ResolveClassByName(TgtSpec);
            if (!Tgt) return FString::Printf(TEXT("Target class not found: %s"), *TgtSpec);
            CastNode->TargetType = Tgt;
            if (Config.IsValid())
            {
                bool bPure = false;
                if (Config->TryGetBoolField(TEXT("pure"), bPure)) CastNode->SetPurity(bPure);
            }
            return FString();
        }
        if (UK2Node_CustomEvent* CustomEvNode = Cast<UK2Node_CustomEvent>(Node))
        {
            FString EvName;
            if (!Config.IsValid() || !Config->TryGetStringField(TEXT("event_name"), EvName))
            {
                return TEXT("CustomEvent config.event_name missing");
            }
            CustomEvNode->CustomFunctionName = FName(*EvName);
            return FString();
        }
        if (UK2Node_Event* EvNode = Cast<UK2Node_Event>(Node))
        {
            FString EvName;
            if (!Config.IsValid() || !Config->TryGetStringField(TEXT("event_name"), EvName))
            {
                return TEXT("Event config.event_name missing (override target)");
            }
            const FName EvFName(*EvName);

            // Resolve override target across the full UE editor "Override Function"
            // surface: parent-class chain + implemented interfaces. The editor's
            // dropdown lists both in one menu — we mirror that. Three steps in
            // order: (1) explicit interface_class for disambiguation, (2) walk
            // the parent class chain (FindFunctionByName walks supers by default),
            // (3) walk ImplementedInterfaces in array order.
            //
            // OwningClass at the end of this block is what gets baked into
            // EventReference — SetExternalMember keeps bSelfContext=false for
            // both parent overrides and interface overrides, so the BP compiler
            // routes correctly either way.
            UClass* OwningClass = nullptr;
            UFunction* Fn = nullptr;

            // (1) Explicit interface_class config — for disambiguation when
            //     multiple interfaces expose the same function name.
            FString ExplicitInterfacePath;
            if (Config->TryGetStringField(TEXT("interface_class"), ExplicitInterfacePath) && !ExplicitInterfacePath.IsEmpty())
            {
                UClass* ExplicitIface = LoadClass<UInterface>(nullptr, *ExplicitInterfacePath);
                if (!ExplicitIface)
                {
                    // Fall back: caller passed a Blueprint asset path for a BPI.
                    if (UBlueprint* IfaceBP = LoadObject<UBlueprint>(nullptr, *ExplicitInterfacePath))
                    {
                        ExplicitIface = IfaceBP->GeneratedClass;
                    }
                }
                if (!ExplicitIface)
                {
                    return FString::Printf(TEXT("interface_class '%s' did not resolve to a UClass (tried LoadClass<UInterface> and LoadObject<UBlueprint>)"),
                                           *ExplicitInterfacePath);
                }
                Fn = ExplicitIface->FindFunctionByName(EvFName);
                if (!Fn)
                {
                    return FString::Printf(TEXT("Override target '%s' not on interface class '%s'"),
                                           *EvName, *ExplicitIface->GetName());
                }
                OwningClass = ExplicitIface;
            }

            // (2) Parent-class chain. FindFunctionByName walks supers by
            //     default, so a single call covers the whole inheritance line.
            if (!Fn)
            {
                if (UClass* ParentCls = Blueprint->ParentClass)
                {
                    if (UFunction* Found = ParentCls->FindFunctionByName(EvFName))
                    {
                        Fn = Found;
                        OwningClass = ParentCls;
                    }
                }
            }

            // (3) ImplementedInterfaces — Blueprint Interface (BPI) event
            //     overrides like GazeLookAtSettings live here, not on the parent
            //     class chain. Walk in array order; first match wins. Skip nulled
            //     entries (asset-redirector leftovers).
            if (!Fn)
            {
                for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
                {
                    UClass* IfaceCls = Iface.Interface;
                    if (!IfaceCls) continue;
                    if (UFunction* Found = IfaceCls->FindFunctionByName(EvFName))
                    {
                        Fn = Found;
                        OwningClass = IfaceCls;
                        break;
                    }
                }
            }

            if (!Fn || !OwningClass)
            {
                // Build a useful diagnostic listing what we searched.
                TArray<FString> SearchedClasses;
                if (Blueprint->ParentClass) { SearchedClasses.Add(Blueprint->ParentClass->GetName()); }
                for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
                {
                    if (Iface.Interface) { SearchedClasses.Add(Iface.Interface->GetName()); }
                }
                return FString::Printf(TEXT("Override target '%s' not found on parent class chain or implemented interfaces. Searched: %s"),
                                       *EvName,
                                       SearchedClasses.Num() > 0 ? *FString::Join(SearchedClasses, TEXT(", ")) : TEXT("(none)"));
            }

            EvNode->EventReference.SetExternalMember(EvFName, OwningClass);
            EvNode->bOverrideFunction = true;
            return FString();
        }
        if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
        {
            FString MacroPath;
            if (!Config.IsValid() || !Config->TryGetStringField(TEXT("macro_graph"), MacroPath))
            {
                return TEXT("MacroInstance config.macro_graph missing");
            }
            UEdGraph* MG = LoadObject<UEdGraph>(nullptr, *MacroPath);
            if (!MG) return FString::Printf(TEXT("Macro graph not found: %s"), *MacroPath);
            MacroNode->SetMacroGraph(MG);
            return FString();
        }
        if (UK2Node_MakeStruct* MakeStructNode = Cast<UK2Node_MakeStruct>(Node))
        {
            FString StructPath;
            if (Config.IsValid() && Config->TryGetStringField(TEXT("struct"), StructPath))
            {
                UScriptStruct* SS = LoadObject<UScriptStruct>(nullptr, *StructPath);
                if (!SS) return FString::Printf(TEXT("Struct not found: %s"), *StructPath);
                MakeStructNode->StructType = SS;
            }
            return FString();
        }
        if (UK2Node_BreakStruct* BreakStructNode = Cast<UK2Node_BreakStruct>(Node))
        {
            FString StructPath;
            if (Config.IsValid() && Config->TryGetStringField(TEXT("struct"), StructPath))
            {
                UScriptStruct* SS = LoadObject<UScriptStruct>(nullptr, *StructPath);
                if (!SS) return FString::Printf(TEXT("Struct not found: %s"), *StructPath);
                BreakStructNode->StructType = SS;
            }
            return FString();
        }
        // K2Node_IfThenElse, K2Node_ExecutionSequence, K2Node_Self, K2Node_Knot,
        // K2Node_MakeArray, K2Node_EnumLiteral — no required pre-pin config.
        return FString();
    }
}
