// Copyright Insodimension Studios. All Rights Reserved.
//
// ============================================================================
//  Atomic Blueprint authoring — design contract
// ============================================================================
//
//  bp_actions_search (discovery) and bp_author_graph (build) form one loop:
//  the caller searches the live FBlueprintActionDatabase for a handle + pin
//  signature, then submits one declarative spec that this file turns into a
//  whole graph in a single call. The invariants below are load-bearing — a
//  change that breaks one reintroduces a class of silent, undiagnosable
//  failures this surface exists to eliminate:
//
//   1. SINGLE WALK. bp_actions_search walks the (multi-second cold) registry
//      exactly once per call; batch queries score that one walk and the
//      per-result template spawn is de-duplicated across queries.
//   2. PRE-FLIGHT BEFORE TRANSACTION. The full spec is validated and
//      auto-repaired before any FScopedTransaction opens. A hard error
//      aborts with ZERO mutation; soft issues warn and proceed.
//   3. CANONICAL SPAWN ONLY. Every node is created via
//      UBlueprintNodeSpawner::Invoke (the editor's own Add-Node path).
//      Hand-rolling NewObject + AllocateDefaultPins crashes class-bound
//      nodes — never reintroduce a direct-construct path.
//   4. DEFERRED PIN DEFAULTS. Defaults are applied AFTER the link pass:
//      a wildcard/container pin only resolves its type once wired, so a
//      pre-link default would be wiped. Numeric defaults are formatted
//      per resolved pin category (int pins reject float-formatted text).
//   5. HARD CONTRACT. Any node/link/pin-default failure or a compile error
//      yields success:false WITH the full data envelope still attached —
//      the response is never collapsed, so a self-correcting caller always
//      has the diagnostics and never has to re-run blind.
//
// ============================================================================

#include "Commands/AgenticMCPBpAuthorCommands.h"
#include "Commands/AgenticMCPCommonUtils.h"
#include "Commands/AgenticMCPBlueprintHelpers.h"
#include "AgenticMCPModule.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"

#include "K2Node.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Self.h"
#include "K2Node_Knot.h"
#include "K2Node_FormatText.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallDelegate.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_Timeline.h"

#include "Templates/Function.h"
#include "UObject/Package.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"

#include "UObject/UnrealType.h"

namespace
{
    /** Map well-known control-flow / structural K2Node classes to flow.* / node.* handles. */
    FString GenericHandleForNodeClass(UClass* NodeClass)
    {
        if (!NodeClass)
        {
            return FString();
        }
        if (NodeClass->IsChildOf(UK2Node_IfThenElse::StaticClass()))
        {
            return TEXT("flow.branch");
        }
        if (NodeClass->IsChildOf(UK2Node_ExecutionSequence::StaticClass()))
        {
            return TEXT("flow.sequence");
        }
        if (NodeClass->IsChildOf(UK2Node_Self::StaticClass()))
        {
            return TEXT("self");
        }
        if (NodeClass->IsChildOf(UK2Node_Knot::StaticClass()))
        {
            return TEXT("knot");
        }
        if (NodeClass->IsChildOf(UK2Node_FormatText::StaticClass()))
        {
            return TEXT("text.format");
        }
        if (NodeClass->IsChildOf(UK2Node_DynamicCast::StaticClass()))
        {
            // Template has no bound target type; the LLM appends the class
            // (cast.<Class>) at author time. Surface the base handle.
            return TEXT("cast.<Class>");
        }
        if (NodeClass->IsChildOf(UK2Node_MacroInstance::StaticClass()))
        {
            // Resolved more specifically from the template node (see
            // ResolveMacroHandle); only hit here when no template available.
            return TEXT("flow.<macro>");
        }
        return FString::Printf(TEXT("node.%s"), *NodeClass->GetName());
    }

    /** Map a StandardMacros graph name to a stable flow.* handle. */
    FString ResolveMacroHandle(UEdGraphNode* TemplateNode)
    {
        UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(TemplateNode);
        if (!MacroNode)
        {
            return TEXT("flow.<macro>");
        }
        UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
        if (!MacroGraph)
        {
            return TEXT("flow.<macro>");
        }
        const FString Name = MacroGraph->GetName();
        static const TMap<FString, FString> Known = {
            { TEXT("ForEachLoop"),            TEXT("flow.foreach") },
            { TEXT("ForEachLoopWithBreak"),   TEXT("flow.foreach.break") },
            { TEXT("ReverseForEachLoop"),     TEXT("flow.reverse_foreach") },
            { TEXT("ForLoop"),                TEXT("flow.for") },
            { TEXT("ForLoopWithBreak"),       TEXT("flow.for.break") },
            { TEXT("WhileLoop"),              TEXT("flow.while") },
            { TEXT("IsValid"),                TEXT("flow.valid") },
            { TEXT("Gate"),                   TEXT("flow.gate") },
            { TEXT("DoOnce"),                 TEXT("flow.once") },
            { TEXT("Do N"),                   TEXT("flow.don") },
            { TEXT("DoN"),                    TEXT("flow.don") },
            { TEXT("FlipFlop"),               TEXT("flow.flipflop") },
        };
        if (const FString* Mapped = Known.Find(Name))
        {
            return *Mapped;
        }
        return FString::Printf(TEXT("flow.macro.%s"), *Name);
    }

    /**
     * Cheap, template-free name for a spawner — the bound field's name.
     * Used in Pass-1 scoring so freshly-loaded BP variables/functions
     * surface even before the editor's priming tick fills MenuName.
     */
    FString GetCheapSpawnerName(UBlueprintNodeSpawner* Spawner)
    {
        if (const UBlueprintFunctionNodeSpawner* Fn = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
        {
            if (const UFunction* F = Fn->GetFunction())
            {
                return F->GetName();
            }
        }
        if (const UBlueprintVariableNodeSpawner* Var = Cast<UBlueprintVariableNodeSpawner>(Spawner))
        {
            if (const FProperty* P = Var->GetVarProperty())
            {
                return P->GetName();
            }
        }
        if (const UBlueprintEventNodeSpawner* Ev = Cast<UBlueprintEventNodeSpawner>(Spawner))
        {
            if (const UFunction* EvFn = Ev->GetEventFunction())
            {
                return EvFn->GetName();
            }
        }
        if (const UBlueprintDelegateNodeSpawner* Del = Cast<UBlueprintDelegateNodeSpawner>(Spawner))
        {
            if (const FMulticastDelegateProperty* DP = Del->GetDelegateProperty())
            {
                return DP->GetName();
            }
        }
        return FString();
    }

    /** Synthesize the author-tool handle for a spawner (fn./var./set./ev./custom./dispatch./flow./node.). */
    FString SynthesizeHandle(UBlueprintNodeSpawner* Spawner, UEdGraphNode* TemplateNode)
    {
        if (const UBlueprintFunctionNodeSpawner* FnSpawner = Cast<UBlueprintFunctionNodeSpawner>(Spawner))
        {
            if (const UFunction* Fn = FnSpawner->GetFunction())
            {
                const UClass* OwnerClass = Fn->GetOwnerClass();
                const FString OwnerName = OwnerClass ? OwnerClass->GetName() : TEXT("Unknown");
                return FString::Printf(TEXT("fn.%s.%s"), *OwnerName, *Fn->GetName());
            }
        }
        if (const UBlueprintVariableNodeSpawner* VarSpawner = Cast<UBlueprintVariableNodeSpawner>(Spawner))
        {
            FString VarName;
            if (const FProperty* Prop = VarSpawner->GetVarProperty())
            {
                VarName = Prop->GetName();
            }
            if (VarName.IsEmpty())
            {
                return FString();
            }
            const bool bIsSetter = Spawner->NodeClass
                && Spawner->NodeClass->IsChildOf(UK2Node_VariableSet::StaticClass());
            return FString::Printf(TEXT("%s.%s"), bIsSetter ? TEXT("set") : TEXT("var"), *VarName);
        }
        if (const UBlueprintEventNodeSpawner* EvSpawner = Cast<UBlueprintEventNodeSpawner>(Spawner))
        {
            if (EvSpawner->IsForCustomEvent())
            {
                return TEXT("custom.<Name>");
            }
            if (const UFunction* EvFn = EvSpawner->GetEventFunction())
            {
                return FString::Printf(TEXT("ev.%s"), *EvFn->GetName());
            }
            return TEXT("ev.<Name>");
        }
        if (const UBlueprintDelegateNodeSpawner* DelSpawner = Cast<UBlueprintDelegateNodeSpawner>(Spawner))
        {
            if (const FMulticastDelegateProperty* DelProp = DelSpawner->GetDelegateProperty())
            {
                return FString::Printf(TEXT("dispatch.%s"), *DelProp->GetName());
            }
            return TEXT("dispatch.<Name>");
        }
        if (Spawner->NodeClass
            && Spawner->NodeClass->IsChildOf(UK2Node_MacroInstance::StaticClass())
            && TemplateNode)
        {
            return ResolveMacroHandle(TemplateNode);
        }
        return GenericHandleForNodeClass(Spawner->NodeClass);
    }

    /** Compact pin descriptor matching the documented bp_actions_search schema. */
    TSharedPtr<FJsonObject> PinToActionJson(UEdGraphPin* Pin)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("name"), Pin->PinName.ToString());
        O->SetStringField(TEXT("direction"),
            Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        O->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());

        if (const UObject* SubObj = Pin->PinType.PinSubCategoryObject.Get())
        {
            O->SetStringField(TEXT("subcategory_object"), SubObj->GetPathName());
        }
        if (Pin->PinType.ContainerType != EPinContainerType::None)
        {
            O->SetStringField(TEXT("container"),
                Pin->PinType.ContainerType == EPinContainerType::Array ? TEXT("array")
                : Pin->PinType.ContainerType == EPinContainerType::Set ? TEXT("set")
                : TEXT("map"));
        }
        if (!Pin->DefaultValue.IsEmpty())
        {
            O->SetStringField(TEXT("default"), Pin->DefaultValue);
        }
        return O;
    }

    /** Lowercased haystack-contains test. */
    bool ContainsCI(const FString& Haystack, const FString& Needle)
    {
        return Haystack.Contains(Needle, ESearchCase::IgnoreCase);
    }

    /**
     * Zero-cost intent aliases for control-flow / structural K2 nodes whose
     * DefaultMenuSignature is empty until the editor's background priming tick
     * fills it. Keyed by NodeClass name (always free — no template spawn).
     * Lets "branch"/"loop"/"foreach"/"sequence"/"cast"/"select" resolve on a
     * cold action DB. Value = space-separated search synonyms.
     */
    const TMap<FString, FString>& GetFlowIntentAliases()
    {
        static const TMap<FString, FString> Aliases = {
            { TEXT("K2Node_IfThenElse"),         TEXT("branch if then else condition flow") },
            { TEXT("K2Node_ExecutionSequence"),  TEXT("sequence then series flow") },
            { TEXT("K2Node_MacroInstance"),      TEXT("macro foreach for each loop while gate flipflop doonce reverse") },
            { TEXT("K2Node_DynamicCast"),        TEXT("cast casting as convert downcast") },
            { TEXT("K2Node_Self"),               TEXT("self this owner") },
            { TEXT("K2Node_Knot"),               TEXT("knot reroute") },
            { TEXT("K2Node_Select"),             TEXT("select pick choose ternary") },
            { TEXT("K2Node_MultiGate"),          TEXT("multigate gate sequence") },
            { TEXT("K2Node_DoOnceMultiInput"),   TEXT("doonce do once") },
            { TEXT("K2Node_FormatText"),         TEXT("format text string build") },
            { TEXT("K2Node_MakeArray"),          TEXT("make array literal") },
            { TEXT("K2Node_MakeMap"),            TEXT("make map literal") },
            { TEXT("K2Node_MakeSet"),            TEXT("make set literal") },
            { TEXT("K2Node_GetArrayItem"),       TEXT("array get item index element") },
            { TEXT("K2Node_Timeline"),           TEXT("timeline curve interp") },
            { TEXT("K2Node_SpawnActorFromClass"),TEXT("spawn actor from class") },
            { TEXT("K2Node_AddComponent"),       TEXT("add component") },
            { TEXT("K2Node_CreateDelegate"),     TEXT("create delegate event bind") },
            { TEXT("K2Node_SwitchEnum"),         TEXT("switch enum case") },
            { TEXT("K2Node_SwitchInteger"),      TEXT("switch int integer case") },
            { TEXT("K2Node_SwitchString"),       TEXT("switch string case") },
            { TEXT("K2Node_SwitchName"),         TEXT("switch name case") },
        };
        return Aliases;
    }

    /** Score a single name string against the query (exact > prefix > contains). */
    int32 ScoreName(const FString& Name, const FString& Query)
    {
        if (Name.IsEmpty() || Query.IsEmpty())
        {
            return 0;
        }
        if (Name.Equals(Query, ESearchCase::IgnoreCase))
        {
            return 1000;
        }
        if (Name.StartsWith(Query, ESearchCase::IgnoreCase))
        {
            return 600 - FMath::Min(Name.Len(), 300);
        }
        if (ContainsCI(Name, Query))
        {
            return 300 - FMath::Min(Name.Len(), 200);
        }
        return 0;
    }

    /** True when Query exactly equals one whitespace-delimited token in Synonyms. */
    bool HasExactToken(const FString& Synonyms, const FString& Query)
    {
        TArray<FString> Tokens;
        Synonyms.ParseIntoArray(Tokens, TEXT(" "), true);
        for (const FString& Tok : Tokens)
        {
            if (Tok.Equals(Query, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    /**
     * Cheap relevance score using only the spawner's pre-populated
     * DefaultMenuSignature (no template-node spawn). Empty query → 1
     * (registry order, capped by max_results).
     */
    int32 ScoreSpawner(const FBlueprintActionUiSpec& Ui, const FString& Query)
    {
        if (Query.IsEmpty())
        {
            return 1;
        }
        const int32 MenuScore = ScoreName(Ui.MenuName.ToString(), Query);
        if (MenuScore > 0)
        {
            return MenuScore;
        }
        if (ContainsCI(Ui.Keywords.ToString(), Query))
        {
            return 120;
        }
        if (ContainsCI(Ui.Category.ToString(), Query))
        {
            return 60;
        }
        return 0;
    }

    // ========================================================================
    //  Phase B helpers — handle resolution + atomic build
    // ========================================================================

    /** Nodes that survive clear_first (auto-created or user-structural). */
    bool IsPreservedOnClear(UEdGraphNode* Node)
    {
        if (!Node)
        {
            return true;
        }
        if (Node->IsA<UK2Node_FunctionEntry>() ||
            Node->IsA<UK2Node_FunctionResult>() ||
            Node->IsA<UEdGraphNode_Comment>() ||
            Node->IsA<UK2Node_Timeline>())
        {
            return true;
        }
        if (UK2Node_Event* Ev = Cast<UK2Node_Event>(Node))
        {
            return Ev->bOverrideFunction; // keep real event overrides
        }
        return false;
    }

    /** Day-2 pin finder: exact -> case-insensitive -> exec/self aliases. */
    UEdGraphPin* FindPinFlexible(UEdGraphNode* Node, const FString& PinName,
                                  EEdGraphPinDirection Dir)
    {
        if (!Node)
        {
            return nullptr;
        }
        // exact
        for (UEdGraphPin* P : Node->Pins)
        {
            if (P && P->Direction == Dir && P->PinName.ToString().Equals(PinName, ESearchCase::CaseSensitive))
            {
                return P;
            }
        }
        // case-insensitive (name or friendly name)
        for (UEdGraphPin* P : Node->Pins)
        {
            if (P && P->Direction == Dir &&
                (P->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) ||
                 P->PinFriendlyName.ToString().Equals(PinName, ESearchCase::IgnoreCase)))
            {
                return P;
            }
        }
        // exec aliases: exec/execute/in/then/out -> primary exec pin of Dir
        const bool bExecAlias =
            PinName.Equals(TEXT("exec"), ESearchCase::IgnoreCase) ||
            PinName.Equals(TEXT("execute"), ESearchCase::IgnoreCase) ||
            PinName.Equals(TEXT("in"), ESearchCase::IgnoreCase) ||
            PinName.Equals(TEXT("then"), ESearchCase::IgnoreCase) ||
            PinName.Equals(TEXT("out"), ESearchCase::IgnoreCase) ||
            PinName.Equals(TEXT("Completed"), ESearchCase::IgnoreCase);
        if (bExecAlias)
        {
            for (UEdGraphPin* P : Node->Pins)
            {
                if (P && P->Direction == Dir && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    return P;
                }
            }
        }
        // self alias
        if (PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase) ||
            PinName.Equals(TEXT("target"), ESearchCase::IgnoreCase))
        {
            for (UEdGraphPin* P : Node->Pins)
            {
                if (P && P->Direction == Dir && P->PinName == UEdGraphSchema_K2::PN_Self)
                {
                    return P;
                }
            }
        }
        return nullptr;
    }

    /**
     * Apply one JSON pin-default and VERIFY it stuck (read-back). Returns
     * empty string on success; a precise diagnostic otherwise (pin name +
     * category + attempted value + what the pin actually shows). This makes
     * silent schema rejections — int pin given "4.000000", wildcard not yet
     * resolved, bad asset path — instantly visible to a self-correcting
     * caller with no dump / no PIE needed.
     */
    FString ApplyPinDefault(UEdGraphPin* Pin, const TSharedPtr<FJsonValue>& Val)
    {
        if (!Pin || !Val.IsValid())
        {
            return TEXT("invalid pin/value");
        }
        const UEdGraphSchema* Schema = Pin->GetSchema();
        if (!Schema)
        {
            return TEXT("pin has no schema");
        }
        const FString PinName = Pin->PinName.ToString();
        const FName Cat = Pin->PinType.PinCategory;

        // -- object/class/soft path -> DefaultObject --
        if (Val->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Obj = Val->AsObject();
            FString PathStr;
            if (Obj->TryGetStringField(TEXT("object"), PathStr) ||
                Obj->TryGetStringField(TEXT("soft_object"), PathStr) ||
                Obj->TryGetStringField(TEXT("class"), PathStr))
            {
                UObject* Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *PathStr);
                if (!Loaded && !PathStr.EndsWith(TEXT("_C")))
                {
                    if (UBlueprint* BpAsset = LoadObject<UBlueprint>(nullptr, *PathStr))
                    {
                        Loaded = BpAsset->GeneratedClass;
                    }
                }
                if (!Loaded)
                {
                    return FString::Printf(TEXT("pin '%s': could not load object '%s'"), *PinName, *PathStr);
                }
                Schema->TrySetDefaultObject(*Pin, Loaded);
                if (Pin->DefaultObject != Loaded)
                {
                    return FString::Printf(
                        TEXT("pin '%s' (category=%s) rejected object '%s' — pin is not object-typed "
                             "or the asset class is incompatible (pin still shows %s)"),
                        *PinName, *Cat.ToString(), *PathStr,
                        Pin->DefaultObject ? *Pin->DefaultObject->GetName() : TEXT("null"));
                }
                return FString();
            }
            // struct literals
            const TArray<TSharedPtr<FJsonValue>>* Vec = nullptr;
            FString StructStr;
            if (Obj->TryGetArrayField(TEXT("vector"), Vec) && Vec->Num() == 3)
            {
                StructStr = FString::Printf(TEXT("%f,%f,%f"),
                    (*Vec)[0]->AsNumber(), (*Vec)[1]->AsNumber(), (*Vec)[2]->AsNumber());
            }
            else if (Obj->TryGetArrayField(TEXT("rotator"), Vec) && Vec->Num() == 3)
            {
                StructStr = FString::Printf(TEXT("%f,%f,%f"),
                    (*Vec)[0]->AsNumber(), (*Vec)[1]->AsNumber(), (*Vec)[2]->AsNumber());
            }
            else if (Obj->TryGetStringField(TEXT("raw"), StructStr))
            {
                // raw escape hatch — caller owns correctness
            }
            else
            {
                return FString::Printf(TEXT("pin '%s': unrecognized pin_default object shape"), *PinName);
            }
            Schema->TrySetDefaultValue(*Pin, StructStr);
            if (Pin->DefaultValue.IsEmpty())
            {
                return FString::Printf(
                    TEXT("pin '%s' (category=%s) rejected struct default '%s' — likely wrong struct "
                         "type or unresolved wildcard"),
                    *PinName, *Cat.ToString(), *StructStr);
            }
            return FString();
        }

        // -- scalar value pins --
        FString Attempt;
        switch (Val->Type)
        {
        case EJson::String:
            Attempt = Val->AsString();
            break;
        case EJson::Boolean:
            Attempt = Val->AsBool() ? TEXT("true") : TEXT("false");
            break;
        case EJson::Number:
        {
            const double Num = Val->AsNumber();
            // int/byte pins REJECT "4.000000"; they need "4". Only real
            // pins take the float-formatted form.
            if (Cat == UEdGraphSchema_K2::PC_Int ||
                Cat == UEdGraphSchema_K2::PC_Int64 ||
                Cat == UEdGraphSchema_K2::PC_Byte)
            {
                Attempt = FString::Printf(TEXT("%lld"), static_cast<int64>(FMath::RoundToDouble(Num)));
            }
            else
            {
                Attempt = FString::SanitizeFloat(Num);
            }
            break;
        }
        default:
            return FString::Printf(TEXT("pin '%s': unsupported pin_default JSON type"), *PinName);
        }

        Schema->TrySetDefaultValue(*Pin, Attempt);
        // Read-back: a rejected value leaves DefaultValue empty (or unchanged
        // to autogen). Empty after a non-empty attempt = silent rejection.
        if (Pin->DefaultValue.IsEmpty() && !Attempt.IsEmpty())
        {
            return FString::Printf(
                TEXT("pin '%s' (category=%s) rejected default '%s' — type mismatch "
                     "(e.g. float-formatted value on an int pin, or unresolved wildcard)"),
                *PinName, *Cat.ToString(), *Attempt);
        }
        return FString();
    }

    /** Resolve an event UFunction by name across parent chain + interfaces. */
    UFunction* ResolveEventFunction(UBlueprint* Blueprint, const FString& EventName)
    {
        if (!Blueprint)
        {
            return nullptr;
        }
        for (UClass* C = Blueprint->ParentClass; C; C = C->GetSuperClass())
        {
            if (UFunction* Fn = C->FindFunctionByName(*EventName))
            {
                return Fn;
            }
        }
        for (const FBPInterfaceDescription& IfaceDesc : Blueprint->ImplementedInterfaces)
        {
            if (IfaceDesc.Interface)
            {
                if (UFunction* Fn = IfaceDesc.Interface->FindFunctionByName(*EventName))
                {
                    return Fn;
                }
            }
        }
        return nullptr;
    }

    /** Resolve a function by "Owner" + "Func" with K2_ 4-tier fallback. */
    UFunction* ResolveFn(const FString& OwnerSpec, const FString& FuncName, FString& OutErr)
    {
        UClass* Cls = AgenticMCPBlueprintHelpers::ResolveClassByName(OwnerSpec);
        if (!Cls)
        {
            OutErr = FString::Printf(TEXT("class '%s' not found"), *OwnerSpec);
            return nullptr;
        }
        if (UFunction* Fn = Cls->FindFunctionByName(*FuncName))
        {
            return Fn;
        }
        if (!FuncName.StartsWith(TEXT("K2_")))
        {
            if (UFunction* Fn = Cls->FindFunctionByName(*FString::Printf(TEXT("K2_%s"), *FuncName)))
            {
                return Fn;
            }
        }
        if (FuncName.StartsWith(TEXT("K2_")))
        {
            if (UFunction* Fn = Cls->FindFunctionByName(*FuncName.Mid(3)))
            {
                return Fn;
            }
        }
        FString BestMatch;
        for (TFieldIterator<UFunction> It(Cls); It; ++It)
        {
            if (It->GetName().Equals(FuncName, ESearchCase::IgnoreCase))
            {
                return *It;
            }
            if (BestMatch.IsEmpty() && It->GetName().Contains(FuncName, ESearchCase::IgnoreCase))
            {
                BestMatch = It->GetName();
            }
        }
        OutErr = FString::Printf(TEXT("function '%s' not found on '%s'"), *FuncName, *OwnerSpec);
        if (!BestMatch.IsEmpty())
        {
            OutErr += FString::Printf(TEXT(" (did you mean '%s'?)"), *BestMatch);
        }
        return nullptr;
    }

    /** Find the StandardMacros library graph for a macro short name. */
    UEdGraph* FindStandardMacroGraph(const FString& MacroName)
    {
        UBlueprint* MacroLib = LoadObject<UBlueprint>(
            nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
        if (!MacroLib)
        {
            return nullptr;
        }
        for (UEdGraph* G : MacroLib->MacroGraphs)
        {
            if (G && G->GetName().Equals(MacroName, ESearchCase::IgnoreCase))
            {
                return G;
            }
        }
        return nullptr;
    }

    /** flow.<verb> -> StandardMacros graph name (inverse of ResolveMacroHandle). */
    FString FlowVerbToMacroName(const FString& Verb)
    {
        static const TMap<FString, FString> M = {
            { TEXT("foreach"),          TEXT("ForEachLoop") },
            { TEXT("foreach.break"),    TEXT("ForEachLoopWithBreak") },
            { TEXT("reverse_foreach"),  TEXT("ReverseForEachLoop") },
            { TEXT("for"),              TEXT("ForLoop") },
            { TEXT("for.break"),        TEXT("ForLoopWithBreak") },
            { TEXT("while"),            TEXT("WhileLoop") },
            { TEXT("valid"),            TEXT("IsValid") },
            { TEXT("gate"),             TEXT("Gate") },
            { TEXT("once"),             TEXT("DoOnce") },
            { TEXT("don"),              TEXT("Do N") },
            { TEXT("flipflop"),         TEXT("FlipFlop") },
        };
        if (const FString* Found = M.Find(Verb))
        {
            return *Found;
        }
        return FString();
    }

    /**
     * Seeded auto-repair table: known-wrong handle -> canonical handle.
     * Only fully-qualified fn./ev./op. handles belong here (never var./
     * custom./cast./spawn./node. — those carry caller-chosen suffixes).
     * The K2_ 4-tier fallback in ResolveFn already covers K2_-prefix cases
     * (fn.Actor.GetActorLocation etc.), so they are NOT duplicated here.
     */
    const TMap<FString, FString>& GetAutoRepairTable()
    {
        static const TMap<FString, FString> T = {
            // Timer family — live on KismetSystemLibrary as K2_*
            { TEXT("fn.GameplayStatics.SetTimer"),                   TEXT("fn.KismetSystemLibrary.K2_SetTimer") },
            { TEXT("fn.GameplayStatics.SetTimerByEvent"),            TEXT("fn.KismetSystemLibrary.K2_SetTimerDelegate") },
            { TEXT("fn.KismetSystemLibrary.SetTimer"),               TEXT("fn.KismetSystemLibrary.K2_SetTimer") },
            { TEXT("fn.KismetSystemLibrary.SetTimerByEvent"),        TEXT("fn.KismetSystemLibrary.K2_SetTimerDelegate") },
            { TEXT("fn.KismetSystemLibrary.SetTimerByFunctionName"), TEXT("fn.KismetSystemLibrary.K2_SetTimer") },
            // String conversions — KismetStringLibrary, not System
            { TEXT("fn.KismetSystemLibrary.Conv_IntToString"),       TEXT("fn.KismetStringLibrary.Conv_IntToString") },
            { TEXT("fn.KismetSystemLibrary.Conv_FloatToString"),     TEXT("fn.KismetStringLibrary.Conv_DoubleToString") },
            { TEXT("fn.KismetSystemLibrary.Conv_DoubleToString"),    TEXT("fn.KismetStringLibrary.Conv_DoubleToString") },
            { TEXT("fn.KismetSystemLibrary.Conv_BoolToString"),      TEXT("fn.KismetStringLibrary.Conv_BoolToString") },
            { TEXT("fn.KismetSystemLibrary.Conv_VectorToString"),    TEXT("fn.KismetStringLibrary.Conv_VectorToString") },
            // MakeLiteral family — KismetSystemLibrary, not Math
            { TEXT("fn.KismetMathLibrary.MakeLiteralInt"),           TEXT("fn.KismetSystemLibrary.MakeLiteralInt") },
            { TEXT("fn.KismetMathLibrary.MakeLiteralBool"),          TEXT("fn.KismetSystemLibrary.MakeLiteralBool") },
            { TEXT("fn.KismetMathLibrary.MakeLiteralFloat"),         TEXT("fn.KismetSystemLibrary.MakeLiteralDouble") },
            { TEXT("fn.KismetMathLibrary.MakeLiteralString"),        TEXT("fn.KismetSystemLibrary.MakeLiteralString") },
            { TEXT("fn.KismetMathLibrary.MakeLiteralName"),          TEXT("fn.KismetSystemLibrary.MakeLiteralName") },
            // Object equality — KismetMathLibrary, not System
            { TEXT("fn.KismetSystemLibrary.EqualEqual_ObjectObject"), TEXT("fn.KismetMathLibrary.EqualEqual_ObjectObject") },
            { TEXT("fn.KismetSystemLibrary.NotEqual_ObjectObject"),   TEXT("fn.KismetMathLibrary.NotEqual_ObjectObject") },
            // Boolean NOT
            { TEXT("fn.KismetMathLibrary.BooleanNOT"),               TEXT("fn.KismetMathLibrary.Not_PreBool") },
            // Engine event short-name aliases -> the real ReceiveX UFUNCTION
            { TEXT("ev.BeginPlay"),          TEXT("ev.ReceiveBeginPlay") },
            { TEXT("ev.Tick"),               TEXT("ev.ReceiveTick") },
            { TEXT("ev.ActorBeginOverlap"),  TEXT("ev.ReceiveActorBeginOverlap") },
            { TEXT("ev.ActorEndOverlap"),    TEXT("ev.ReceiveActorEndOverlap") },
            { TEXT("ev.Hit"),                TEXT("ev.ReceiveHit") },
            { TEXT("ev.AnyDamage"),          TEXT("ev.ReceiveAnyDamage") },
            { TEXT("ev.PointDamage"),        TEXT("ev.ReceivePointDamage") },
            { TEXT("ev.RadialDamage"),       TEXT("ev.ReceiveRadialDamage") },
            { TEXT("ev.Destroyed"),          TEXT("ev.ReceiveDestroyed") },
            { TEXT("ev.EndPlay"),            TEXT("ev.ReceiveEndPlay") },
            { TEXT("ev.Possessed"),          TEXT("ev.ReceivePossessed") },
            // Bare loop words -> flow namespace
            { TEXT("forEach"),               TEXT("flow.foreach") },
            { TEXT("foreach"),               TEXT("flow.foreach") },
            { TEXT("ForEachLoop"),           TEXT("flow.foreach") },
            { TEXT("forLoop"),               TEXT("flow.for") },
            { TEXT("disp"),                  TEXT("dispatch") },
        };
        return T;
    }

    /**
     * Apply auto-repair to a handle. Returns the (possibly) repaired handle;
     * OutReason is set when a repair fired (empty otherwise).
     * Order: exact-table -> Float/Float -> Double/Double pattern.
     */
    FString ApplyAutoRepair(const FString& Handle, FString& OutReason)
    {
        if (const FString* Exact = GetAutoRepairTable().Find(Handle))
        {
            OutReason = FString::Printf(
                TEXT("'%s' is not a valid handle — auto-corrected to '%s'. Use the corrected form next time."),
                *Handle, **Exact);
            return *Exact;
        }
        // UE 5.5 renamed math operands: *_FloatFloat -> *_DoubleDouble
        if (Handle.StartsWith(TEXT("fn.KismetMathLibrary.")) && Handle.EndsWith(TEXT("_FloatFloat")))
        {
            FString Fixed = Handle.LeftChop(11) + TEXT("_DoubleDouble");
            OutReason = FString::Printf(
                TEXT("UE 5.5 renamed float math operands — auto-corrected '%s' to '%s'."),
                *Handle, *Fixed);
            return Fixed;
        }
        return Handle;
    }

    /**
     * Crash-safe node spawn via the canonical UBlueprintNodeSpawner::Invoke
     * path — the exact sequence UE's own "Add Node" menu uses. Hand-rolling
     * NewObject+PostPlacedNewNode+AllocateDefaultPins crashes for class-bound
     * nodes (K2Node_SpawnActorFromClass walks exposed-on-spawn properties in
     * AllocateDefaultPins and dereferences spawner-set state that only the
     * Invoke path establishes). The optional Customize runs AFTER the node is
     * spawned but BEFORE its pins are allocated (FCustomizeNodeDelegate
     * contract) — the only safe place to set TargetType / CustomFunctionName /
     * MacroGraph so the pin layout is correct on first allocation.
     */
    UEdGraphNode* SpawnViaSpawner(UClass* NodeClass, UEdGraph* Graph, int32 X, int32 Y,
                                  TFunction<void(UEdGraphNode*)> Customize = nullptr)
    {
        if (!NodeClass || !Graph)
        {
            return nullptr;
        }
        UBlueprintNodeSpawner::FCustomizeNodeDelegate Del;
        if (Customize)
        {
            Del = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda(
                [Customize](UEdGraphNode* NewNode, bool bIsTemplateNode)
                {
                    if (!bIsTemplateNode && NewNode)
                    {
                        Customize(NewNode);
                    }
                });
        }
        UBlueprintNodeSpawner* Sp = UBlueprintNodeSpawner::Create(NodeClass, GetTransientPackage(), Del);
        if (!Sp)
        {
            return nullptr;
        }
        return Sp->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(X, Y));
    }
}

TSharedPtr<FJsonObject> FAgenticMCPBpAuthorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("bp_actions_search"))
    {
        return HandleActionsSearch(Params);
    }
    if (CommandType == TEXT("bp_author_graph"))
    {
        return HandleAuthorGraph(Params);
    }

    return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown bp-author command: %s"), *CommandType));
}

// ============================================================================
//  Phase A — Discovery (FBlueprintActionDatabase walk)
// ============================================================================

TSharedPtr<FJsonObject> FAgenticMCPBpAuthorCommands::HandleActionsSearch(const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonObject> SafeParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();

    // Accept either a single `query` (legacy shape) or a `queries` array
    // (batch). Batch walks the registry ONCE and scores every spawner
    // against all queries in that single pass — the multi-second cold walk
    // is paid once for N queries instead of N times, and the per-result
    // template spawn is de-duplicated across queries.
    TArray<FString> Queries;
    bool bBatch = false;
    {
        const TArray<TSharedPtr<FJsonValue>>* QArr = nullptr;
        if (SafeParams->TryGetArrayField(TEXT("queries"), QArr) && QArr)
        {
            bBatch = true;
            for (const TSharedPtr<FJsonValue>& QV : *QArr)
            {
                FString Q;
                if (QV.IsValid() && QV->TryGetString(Q))
                {
                    Q = Q.TrimStartAndEnd();
                    if (!Q.IsEmpty())
                    {
                        Queries.AddUnique(Q);
                    }
                }
            }
        }
    }
    if (!bBatch)
    {
        FString Query;
        SafeParams->TryGetStringField(TEXT("query"), Query);
        Queries.Add(Query.TrimStartAndEnd());
    }
    if (Queries.Num() == 0)
    {
        Queries.Add(FString());
    }

    int32 MaxResults = 20;
    {
        double NumOut = 0.0;
        if (SafeParams->TryGetNumberField(TEXT("max_results"), NumOut) && NumOut > 0.0)
        {
            MaxResults = FMath::Clamp(static_cast<int32>(NumOut), 1, 200);
        }
    }

    // Optional target Blueprint context — loading it ensures its own
    // variable/function spawners are present in the registry, and gives
    // template nodes a compatible graph outer for accurate pin layout.
    FString TargetBpPath;
    SafeParams->TryGetStringField(TEXT("target_blueprint"), TargetBpPath);
    FString TargetGraphName;
    SafeParams->TryGetStringField(TEXT("target_graph"), TargetGraphName);

    UEdGraph* TargetGraph = nullptr;
    if (!TargetBpPath.IsEmpty())
    {
        TSharedPtr<FJsonObject> LoadErr;
        UBlueprint* TargetBp = AgenticMCPBlueprintHelpers::LoadBlueprintOrError(TargetBpPath, LoadErr);
        if (TargetBp)
        {
            const FString GraphToFind = TargetGraphName.IsEmpty() ? TEXT("EventGraph") : TargetGraphName;
            TargetGraph = AgenticMCPBlueprintHelpers::FindGraphByName(TargetBp, GraphToFind);

            // The BP's own variable/function get/set spawners are registered
            // under the *Blueprint asset* key (via the SkeletonGeneratedClass),
            // populated synchronously by RefreshAssetActions — NOT under the
            // GeneratedClass key. RefreshClassActions(GeneratedClass) misses
            // them entirely (BlueprintActionDatabase.cpp:1631,1654-1657).
            FBlueprintActionDatabase::Get().RefreshAssetActions(TargetBp);
        }
        // A bad target_blueprint is non-fatal — fall back to a context-free
        // registry walk rather than erroring the whole search.
    }

    FBlueprintActionDatabase& Db = FBlueprintActionDatabase::Get();
    const FBlueprintActionDatabase::FActionRegistry& Registry = Db.GetAllActions();

    // Single registry walk — collect every BP-relevant spawner ONCE. The
    // walk (the documented multi-second cold cost) is paid one time no
    // matter how many queries; only the cheap per-query string scoring is
    // repeated below.
    TArray<UBlueprintNodeSpawner*> AllSpawners;
    int32 TotalScanned = 0;
    for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Entry : Registry)
    {
        for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : Entry.Value)
        {
            UBlueprintNodeSpawner* Spawner = SpawnerPtr.Get();
            if (!Spawner || !Spawner->NodeClass)
            {
                continue;
            }
            // BP-relevance filter: only nodes spawnable in a Blueprint
            // graph. Drops RigVMEdGraphNode / Niagara / other graph-system
            // nodes that pollute results (e.g. Control Rig's "Branch").
            if (!Spawner->NodeClass->IsChildOf(UK2Node::StaticClass()))
            {
                continue;
            }
            ++TotalScanned;
            AllSpawners.Add(Spawner);
        }
    }

    auto ScoreOne = [](UBlueprintNodeSpawner* Spawner, const FString& Q) -> int32
    {
        int32 Score = ScoreSpawner(Spawner->DefaultMenuSignature, Q);
        if (!Q.IsEmpty())
        {
            // Cheap derived-name scoring: the bound field's name. Surfaces
            // freshly-loaded BP variables/functions before the editor's
            // priming tick fills MenuName.
            Score = FMath::Max(Score, ScoreName(GetCheapSpawnerName(Spawner), Q));

            // Intent synonyms keyed by NodeClass name. Exact token match
            // beats generic function contains-matches so canonical flow
            // primitives ("branch" -> flow.branch) rank above incidental
            // "...Branch..." function names.
            if (const FString* Synonyms = GetFlowIntentAliases().Find(Spawner->NodeClass->GetName()))
            {
                if (HasExactToken(*Synonyms, Q))
                {
                    Score = FMath::Max(Score, 900);
                }
                else if (ContainsCI(*Synonyms, Q))
                {
                    Score = FMath::Max(Score, 250);
                }
            }
        }
        return Score;
    };

    // The per-result JSON is the expensive part (PrimeDefaultUiSpec +
    // GetTemplateNode + AllocateDefaultPins). Build it at most once per
    // spawner even when a spawner ranks for several queries; clone + stamp
    // the per-query score onto the returned copy.
    TMap<UBlueprintNodeSpawner*, TSharedPtr<FJsonObject>> MatchCache;
    auto BuildMatch = [&MatchCache, TargetGraph](UBlueprintNodeSpawner* Spawner, int32 ScoreVal) -> TSharedPtr<FJsonValue>
    {
        TSharedPtr<FJsonObject> Base;
        if (TSharedPtr<FJsonObject>* Cached = MatchCache.Find(Spawner))
        {
            Base = *Cached;
        }
        else
        {
            const FBlueprintActionUiSpec& Ui = Spawner->PrimeDefaultUiSpec(TargetGraph);
            // GetTemplateNode returns a CACHED node whose pins are NOT
            // allocated until primed — Prime() itself calls
            // AllocateDefaultPins (BlueprintNodeSpawner.cpp:100-104). Mirror
            // that so unprimed spawners still report a full signature.
            UEdGraphNode* Template = Spawner->GetTemplateNode(TargetGraph);
            if (Template && Template->Pins.Num() == 0)
            {
                Template->AllocateDefaultPins();
            }
            Base = MakeShared<FJsonObject>();
            Base->SetStringField(TEXT("handle"), SynthesizeHandle(Spawner, Template));
            Base->SetStringField(TEXT("display_name"), Ui.MenuName.ToString());
            Base->SetStringField(TEXT("category"), Ui.Category.ToString());
            Base->SetStringField(TEXT("tooltip"), Ui.Tooltip.ToString());
            if (!Ui.Keywords.IsEmpty())
            {
                Base->SetStringField(TEXT("keywords"), Ui.Keywords.ToString());
            }
            Base->SetStringField(TEXT("node_class"),
                Spawner->NodeClass ? Spawner->NodeClass->GetName() : TEXT("Unknown"));
            TArray<TSharedPtr<FJsonValue>> PinArr;
            if (Template)
            {
                for (UEdGraphPin* Pin : Template->Pins)
                {
                    if (Pin && !Pin->bHidden)
                    {
                        PinArr.Add(MakeShared<FJsonValueObject>(PinToActionJson(Pin)));
                    }
                }
            }
            Base->SetArrayField(TEXT("pins"), PinArr);
            MatchCache.Add(Spawner, Base);
        }
        // Copy so the per-query score never bleeds across queries (the
        // cached Base is score-less; child pin/string values are shared
        // immutably).
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(*Base);
        Out->SetNumberField(TEXT("score"), ScoreVal);
        return MakeShared<FJsonValueObject>(Out);
    };

    struct FScored
    {
        UBlueprintNodeSpawner* Spawner = nullptr;
        int32 Score = 0;
    };

    auto RunQuery = [&](const FString& Q) -> TSharedPtr<FJsonObject>
    {
        TArray<FScored> Scored;
        Scored.Reserve(AllSpawners.Num() / 4);
        for (UBlueprintNodeSpawner* Spawner : AllSpawners)
        {
            const int32 Score = ScoreOne(Spawner, Q);
            if (Score > 0)
            {
                Scored.Add({ Spawner, Score });
            }
        }
        Scored.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });

        const int32 ResultCount = FMath::Min(Scored.Num(), MaxResults);
        TArray<TSharedPtr<FJsonValue>> Matches;
        Matches.Reserve(ResultCount);
        for (int32 i = 0; i < ResultCount; ++i)
        {
            Matches.Add(BuildMatch(Scored[i].Spawner, Scored[i].Score));
        }

        TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
        One->SetStringField(TEXT("query"), Q);
        One->SetArrayField(TEXT("matches"), Matches);
        One->SetNumberField(TEXT("match_count"), Matches.Num());
        One->SetNumberField(TEXT("total_matched"), Scored.Num());
        return One;
    };

    if (!bBatch)
    {
        // Legacy single-query shape — unchanged contract.
        TSharedPtr<FJsonObject> Data = RunQuery(Queries[0]);
        Data->SetNumberField(TEXT("total_scanned"), TotalScanned);
        return FAgenticMCPCommonUtils::CreateSuccessResponse(Data);
    }

    // Batch shape: one result block per query, single shared scanned count.
    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(Queries.Num());
    for (const FString& Q : Queries)
    {
        Results.Add(MakeShared<FJsonValueObject>(RunQuery(Q)));
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("results"), Results);
    Data->SetNumberField(TEXT("query_count"), Queries.Num());
    Data->SetNumberField(TEXT("total_scanned"), TotalScanned);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Data);
}

// ============================================================================
//  Phase B — Atomic graph authoring
// ============================================================================

TSharedPtr<FJsonObject> FAgenticMCPBpAuthorCommands::HandleAuthorGraph(const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonObject> P = Params.IsValid() ? Params : MakeShared<FJsonObject>();

    FString Path, GraphName;
    if (!P->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("bp_author_graph: 'path' is required"));
    }
    if (!P->TryGetStringField(TEXT("graph"), GraphName) || GraphName.IsEmpty())
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("bp_author_graph: 'graph' is required"));
    }

    const TSharedPtr<FJsonObject>* SpecPtr = nullptr;
    if (!P->TryGetObjectField(TEXT("spec"), SpecPtr) || !SpecPtr || !SpecPtr->IsValid())
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("bp_author_graph: 'spec' object is required"));
    }
    const TSharedPtr<FJsonObject> Spec = *SpecPtr;

    bool bClearFirst = false;
    bool bCompileAfter = true;
    bool bSaveAfter = false;
    Spec->TryGetBoolField(TEXT("clear_first"), bClearFirst);
    Spec->TryGetBoolField(TEXT("compile_after"), bCompileAfter);
    Spec->TryGetBoolField(TEXT("save_after"), bSaveAfter);

    const TArray<TSharedPtr<FJsonValue>>* NodesArr = nullptr;
    if (!Spec->TryGetArrayField(TEXT("nodes"), NodesArr))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            TEXT("bp_author_graph: spec.nodes must be a JSON array"));
    }
    const TArray<TSharedPtr<FJsonValue>>* LinksArr = nullptr;
    Spec->TryGetArrayField(TEXT("links"), LinksArr);

    if (NodesArr->Num() == 0 && (!LinksArr || LinksArr->Num() == 0) && !bClearFirst)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            TEXT("bp_author_graph: empty spec (no nodes, no links, clear_first=false) is a no-op. "
                 "Pass clear_first=true to wipe the graph, or add nodes/links."));
    }

    TSharedPtr<FJsonObject> LoadErr;
    UBlueprint* Blueprint = AgenticMCPBlueprintHelpers::LoadBlueprintOrError(Path, LoadErr);
    if (!Blueprint)
    {
        return LoadErr.IsValid() ? LoadErr
            : FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Blueprint not found: %s"), *Path));
    }
    UEdGraph* Graph = AgenticMCPBlueprintHelpers::FindGraphByName(Blueprint, GraphName);
    if (!Graph)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Graph '%s' not found on %s"), *GraphName, *Path));
    }
    const UEdGraphSchema* Schema = Graph->GetSchema();
    if (!Schema)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Graph has no schema"));
    }

    // Keep this BP's variable/function spawners fresh (var.X / set.X resolve
    // against the SkeletonGeneratedClass property below).
    FBlueprintActionDatabase::Get().RefreshAssetActions(Blueprint);
    UClass* SelfClass = Blueprint->SkeletonGeneratedClass
        ? Blueprint->SkeletonGeneratedClass : Blueprint->GeneratedClass;

    // ---------------------------------------------------------------------
    //  Pre-flight + auto-repair — runs BEFORE the transaction. Hard errors
    //  abort with zero mutation; soft issues warn but the build proceeds.
    //  Repaired handles are written back into the node JSON so the spawn
    //  loop below transparently sees the corrected form.
    // ---------------------------------------------------------------------
    TArray<TSharedPtr<FJsonValue>> RepairsMade;
    TArray<TSharedPtr<FJsonValue>> PreFlightIssues;
    TArray<FString> HardErrors;
    TSet<FString> DeclaredIds;

    // Only prefixes that have a matching spawn branch below.
    // Prefixes listed here but unimplemented would pass pre-flight and then
    // fail at spawn with a generic error — omit them so the validator rejects
    // them early with the bp_actions_search hint.
    // NOTE: NOT static const — Live Coding never re-inits function-local statics.
    const TArray<FString> KnownPrefixes = {
        TEXT("fn."), TEXT("op."), TEXT("ev."), TEXT("custom."),
        TEXT("var."), TEXT("set."), TEXT("prop."),
        TEXT("flow."), TEXT("cast."), TEXT("spawn."), TEXT("dispatch."), TEXT("node.")
    };

    for (const TSharedPtr<FJsonValue>& NV : *NodesArr)
    {
        const TSharedPtr<FJsonObject> NObj = NV.IsValid() ? NV->AsObject() : nullptr;
        if (!NObj.IsValid())
        {
            HardErrors.Add(TEXT("spec.nodes contains a non-object entry"));
            continue;
        }
        FString Id, Handle;
        NObj->TryGetStringField(TEXT("id"), Id);
        NObj->TryGetStringField(TEXT("handle"), Handle);

        if (Id.IsEmpty() || Handle.IsEmpty())
        {
            HardErrors.Add(FString::Printf(
                TEXT("node (id='%s') requires non-empty 'id' and 'handle'"), *Id));
            continue;
        }
        if (Id.Equals(TEXT("entry"), ESearchCase::IgnoreCase) ||
            Id.Equals(TEXT("return"), ESearchCase::IgnoreCase))
        {
            HardErrors.Add(FString::Printf(
                TEXT("'%s' is a reserved sentinel — reference it in links only, never as a node id"), *Id));
            continue;
        }
        if (DeclaredIds.Contains(Id))
        {
            HardErrors.Add(FString::Printf(TEXT("duplicate node id '%s'"), *Id));
            continue;
        }
        DeclaredIds.Add(Id);

        // auto-repair (rewrite handle in place so the spawn loop sees it)
        FString Reason;
        const FString Repaired = ApplyAutoRepair(Handle, Reason);
        if (!Reason.IsEmpty() && !Repaired.Equals(Handle, ESearchCase::CaseSensitive))
        {
            NObj->SetStringField(TEXT("handle"), Repaired);
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetStringField(TEXT("node_id"), Id);
            R->SetStringField(TEXT("from"), Handle);
            R->SetStringField(TEXT("to"), Repaired);
            R->SetStringField(TEXT("reason"), Reason);
            RepairsMade.Add(MakeShared<FJsonValueObject>(R));
            Handle = Repaired;
        }

        // recognized prefix?
        bool bKnownPrefix = false;
        for (const FString& Pfx : KnownPrefixes)
        {
            if (Handle.StartsWith(Pfx)) { bKnownPrefix = true; break; }
        }
        if (!bKnownPrefix &&
            !Handle.Equals(TEXT("self"), ESearchCase::IgnoreCase) &&
            !Handle.Equals(TEXT("knot"), ESearchCase::IgnoreCase))
        {
            HardErrors.Add(FString::Printf(
                TEXT("node '%s': unrecognized handle '%s' (no known prefix; run agentic_bp_actions_search to discover handles)"),
                *Id, *Handle));
            continue;
        }

        // soft: var./set. variable must exist on the BP
        if ((Handle.StartsWith(TEXT("var.")) || Handle.StartsWith(TEXT("set."))) && SelfClass)
        {
            const FString VarName = Handle.Mid(4);
            if (!FindFProperty<FProperty>(SelfClass, *VarName))
            {
                TSharedPtr<FJsonObject> Iss = MakeShared<FJsonObject>();
                Iss->SetStringField(TEXT("type"), TEXT("variable_not_found"));
                Iss->SetStringField(TEXT("node_id"), Id);
                Iss->SetStringField(TEXT("variable"), VarName);
                Iss->SetStringField(TEXT("hint"), FString::Printf(
                    TEXT("variable '%s' is not on %s — call agentic_bp_add_variables + agentic_bp_compile "
                         "before this build, or fix the name. This node will hard-fail at spawn."),
                    *VarName, *Blueprint->GetName()));
                PreFlightIssues.Add(MakeShared<FJsonValueObject>(Iss));
            }
        }
    }

    // links must reference a declared id or the entry/return sentinels
    if (LinksArr)
    {
        auto NodeRefKnown = [&DeclaredIds](const FString& Ref) -> bool
        {
            return Ref.Equals(TEXT("entry"), ESearchCase::IgnoreCase) ||
                   Ref.Equals(TEXT("return"), ESearchCase::IgnoreCase) ||
                   DeclaredIds.Contains(Ref);
        };
        int32 LIdx = -1;
        for (const TSharedPtr<FJsonValue>& LV : *LinksArr)
        {
            ++LIdx;
            const TSharedPtr<FJsonObject> LObj = LV.IsValid() ? LV->AsObject() : nullptr;
            if (!LObj.IsValid())
            {
                continue;
            }
            FString FN, TN, S, D;
            if (LObj->TryGetStringField(TEXT("from"), S) && S.Contains(TEXT(".")))
            {
                int32 Dot; S.FindChar(TEXT('.'), Dot); FN = S.Left(Dot);
            }
            if (LObj->TryGetStringField(TEXT("to"), D) && D.Contains(TEXT(".")))
            {
                int32 Dot; D.FindChar(TEXT('.'), Dot); TN = D.Left(Dot);
            }
            LObj->TryGetStringField(TEXT("from_node"), FN);
            LObj->TryGetStringField(TEXT("to_node"), TN);
            if (!FN.IsEmpty() && !NodeRefKnown(FN))
            {
                HardErrors.Add(FString::Printf(
                    TEXT("link[%d] references unknown from-node '%s'"), LIdx, *FN));
            }
            if (!TN.IsEmpty() && !NodeRefKnown(TN))
            {
                HardErrors.Add(FString::Printf(
                    TEXT("link[%d] references unknown to-node '%s'"), LIdx, *TN));
            }
        }
    }

    if (HardErrors.Num() > 0)
    {
        TSharedPtr<FJsonObject> EData = MakeShared<FJsonObject>();
        EData->SetStringField(TEXT("graph"), GraphName);
        TArray<TSharedPtr<FJsonValue>> HEArr;
        for (const FString& HE : HardErrors)
        {
            HEArr.Add(MakeShared<FJsonValueString>(HE));
        }
        EData->SetArrayField(TEXT("hard_errors"), HEArr);
        EData->SetArrayField(TEXT("repairs_made"), RepairsMade);
        EData->SetArrayField(TEXT("pre_flight_issues"), PreFlightIssues);
        TSharedPtr<FJsonObject> Resp = FAgenticMCPCommonUtils::CreateSuccessResponse(EData);
        Resp->SetBoolField(TEXT("success"), false);
        Resp->SetStringField(TEXT("error"), FString::Printf(
            TEXT("pre-flight rejected the spec: %d hard error(s), zero mutation applied — see data.hard_errors"),
            HardErrors.Num()));
        return Resp;
    }

    const FScopedTransaction Tx(FText::FromString(TEXT("Author Blueprint Graph")));
    Blueprint->Modify();
    Graph->Modify();

    // --- clear_first ---
    int32 ClearedCount = 0;
    if (bClearFirst)
    {
        TArray<UEdGraphNode*> ToRemove;
        for (UEdGraphNode* N : Graph->Nodes)
        {
            if (N && !IsPreservedOnClear(N) && N->CanUserDeleteNode())
            {
                ToRemove.Add(N);
            }
        }
        for (UEdGraphNode* N : ToRemove)
        {
            N->Modify();
            Graph->RemoveNode(N);
            ++ClearedCount;
        }
    }

    // --- spawn nodes ---
    TMap<FString, UEdGraphNode*> IdToNode;
    TArray<TSharedPtr<FJsonValue>> NodesAdded;
    TArray<TSharedPtr<FJsonValue>> NodeFailures;
    // Pin defaults are applied AFTER the link pass: a default set on a
    // wildcard/container pin (e.g. Array_Add.NewItem) is wiped when the
    // wildcard resolves during linking, so setting it pre-link is lost.
    // Connections are type-checked by pin type (never by defaults), so
    // deferring defaults to post-link is strictly correct.
    struct FDeferredDefaults { FString Id; UEdGraphNode* Node; TSharedPtr<FJsonObject> Defs; };
    TArray<FDeferredDefaults> DeferredPinDefaults;
    TArray<TSharedPtr<FJsonValue>> PinDefaultsApplied;
    TArray<TSharedPtr<FJsonValue>> PinDefaultFailures;

    auto FailNode = [&NodeFailures](const FString& Id, const FString& Handle, const FString& Err)
    {
        TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
        F->SetStringField(TEXT("id"), Id);
        F->SetStringField(TEXT("handle"), Handle);
        F->SetStringField(TEXT("error"), Err);
        NodeFailures.Add(MakeShared<FJsonValueObject>(F));
    };

    int32 AutoY = 0;
    for (const TSharedPtr<FJsonValue>& NV : *NodesArr)
    {
        const TSharedPtr<FJsonObject> NObj = NV->AsObject();
        if (!NObj.IsValid())
        {
            continue;
        }
        FString Id, Handle;
        NObj->TryGetStringField(TEXT("id"), Id);
        NObj->TryGetStringField(TEXT("handle"), Handle);
        if (Id.IsEmpty() || Handle.IsEmpty())
        {
            FailNode(Id, Handle, TEXT("node requires non-empty 'id' and 'handle'"));
            continue;
        }
        if (Id.Equals(TEXT("entry"), ESearchCase::IgnoreCase) ||
            Id.Equals(TEXT("return"), ESearchCase::IgnoreCase))
        {
            FailNode(Id, Handle, TEXT("'entry'/'return' are reserved sentinels — reference in links only"));
            continue;
        }

        int32 X = 0, Y = AutoY;
        {
            double D = 0.0;
            if (NObj->TryGetNumberField(TEXT("x"), D)) { X = static_cast<int32>(D); }
            if (NObj->TryGetNumberField(TEXT("y"), D)) { Y = static_cast<int32>(D); }
        }
        AutoY += 180;

        UEdGraphNode* Spawned = nullptr;
        FString SpawnErr;

        // -- fn.<Owner>.<Func>  /  op.<verb> --
        if (Handle.StartsWith(TEXT("fn.")) || Handle.StartsWith(TEXT("op.")))
        {
            FString Owner, Func;
            if (Handle.StartsWith(TEXT("op.")))
            {
                static const TMap<FString, FString> Ops = {
                    { TEXT("add"), TEXT("Add_DoubleDouble") }, { TEXT("sub"), TEXT("Subtract_DoubleDouble") },
                    { TEXT("mul"), TEXT("Multiply_DoubleDouble") }, { TEXT("div"), TEXT("Divide_DoubleDouble") },
                    { TEXT("mod"), TEXT("Percent_DoubleDouble") },
                    { TEXT("eq"), TEXT("EqualEqual_DoubleDouble") }, { TEXT("neq"), TEXT("NotEqual_DoubleDouble") },
                    { TEXT("lt"), TEXT("Less_DoubleDouble") }, { TEXT("lte"), TEXT("LessEqual_DoubleDouble") },
                    { TEXT("gt"), TEXT("Greater_DoubleDouble") }, { TEXT("gte"), TEXT("GreaterEqual_DoubleDouble") },
                    { TEXT("and"), TEXT("BooleanAND") }, { TEXT("or"), TEXT("BooleanOR") },
                    { TEXT("not"), TEXT("Not_PreBool") },
                };
                const FString Verb = Handle.Mid(3);
                const FString* Mapped = Ops.Find(Verb);
                if (!Mapped) { FailNode(Id, Handle, FString::Printf(TEXT("unknown op verb '%s'"), *Verb)); continue; }
                Owner = TEXT("KismetMathLibrary");
                Func = *Mapped;
            }
            else
            {
                FString Rest = Handle.Mid(3);
                int32 Dot;
                if (!Rest.FindLastChar(TEXT('.'), Dot))
                {
                    FailNode(Id, Handle, TEXT("fn.* handle must be fn.<Owner>.<Func>"));
                    continue;
                }
                Owner = Rest.Left(Dot);
                Func = Rest.Mid(Dot + 1);
            }
            UFunction* Fn = ResolveFn(Owner, Func, SpawnErr);
            if (!Fn) { FailNode(Id, Handle, SpawnErr); continue; }
            UBlueprintFunctionNodeSpawner* Sp = UBlueprintFunctionNodeSpawner::Create(Fn);
            if (Sp) { Spawned = Sp->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(X, Y)); }
            if (!Spawned) { FailNode(Id, Handle, TEXT("function spawner Invoke failed")); continue; }
        }
        // -- ev.<EventName> --
        else if (Handle.StartsWith(TEXT("ev.")))
        {
            const FString EvName = Handle.Mid(3);
            UFunction* EvFn = ResolveEventFunction(Blueprint, EvName);
            if (!EvFn)
            {
                FailNode(Id, Handle, FString::Printf(
                    TEXT("event '%s' not found on parent class or implemented interfaces (did you mean custom.%s?)"),
                    *EvName, *EvName));
                continue;
            }
            UBlueprintEventNodeSpawner* Sp = UBlueprintEventNodeSpawner::Create(EvFn);
            if (Sp) { Spawned = Sp->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(X, Y)); }
            if (!Spawned) { FailNode(Id, Handle, TEXT("event spawner Invoke failed")); continue; }
        }
        // -- custom.<Name> --
        else if (Handle.StartsWith(TEXT("custom.")))
        {
            const FString EvName = Handle.Mid(7);
            Spawned = SpawnViaSpawner(UK2Node_CustomEvent::StaticClass(), Graph, X, Y,
                [EvName](UEdGraphNode* N)
                {
                    if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(N))
                    {
                        CE->CustomFunctionName = *EvName;
                        CE->bIsEditable = true;
                    }
                });
            if (!Spawned) { FailNode(Id, Handle, TEXT("custom-event spawn failed")); continue; }
        }
        // -- var.<Name> / set.<Name> --
        else if (Handle.StartsWith(TEXT("var.")) || Handle.StartsWith(TEXT("set.")))
        {
            const bool bSetter = Handle.StartsWith(TEXT("set."));
            const FString VarName = Handle.Mid(4);
            FProperty* Prop = SelfClass ? FindFProperty<FProperty>(SelfClass, *VarName) : nullptr;
            if (!Prop)
            {
                FailNode(Id, Handle, FString::Printf(
                    TEXT("variable '%s' not found on %s (add it via bp_add_variables + compile first)"),
                    *VarName, *Blueprint->GetName()));
                continue;
            }
            UBlueprintVariableNodeSpawner* Sp = UBlueprintVariableNodeSpawner::CreateFromMemberOrParam(
                bSetter ? UK2Node_VariableSet::StaticClass() : UK2Node_VariableGet::StaticClass(),
                Prop, nullptr, SelfClass);
            if (Sp) { Spawned = Sp->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(X, Y)); }
            if (!Spawned) { FailNode(Id, Handle, TEXT("variable spawner Invoke failed")); continue; }
        }
        // -- prop.<Class>.<Prop>  /  prop.set.<Class>.<Prop> --
        // External-object property accessor: a VariableGet/Set on a property
        // of some OTHER class (e.g. AStaticMeshActor::StaticMeshComponent),
        // with a typed target/self input pin instead of self-context.
        else if (Handle.StartsWith(TEXT("prop.")))
        {
            FString Body = Handle.Mid(5);
            const bool bPropSetter = Body.StartsWith(TEXT("set."));
            if (bPropSetter)
            {
                Body = Body.Mid(4);
            }
            int32 LastDot;
            if (!Body.FindLastChar(TEXT('.'), LastDot) || LastDot == 0 || LastDot == Body.Len() - 1)
            {
                FailNode(Id, Handle, TEXT("prop.* handle must be prop.<Class>.<Prop> or prop.set.<Class>.<Prop>"));
                continue;
            }
            const FString OwnerSpec = Body.Left(LastDot);
            const FString PropName = Body.Mid(LastDot + 1);
            UClass* OwnerCls = AgenticMCPBlueprintHelpers::ResolveClassByName(OwnerSpec);
            if (!OwnerCls)
            {
                FailNode(Id, Handle, FString::Printf(TEXT("prop owner class '%s' not found"), *OwnerSpec));
                continue;
            }
            FProperty* ExtProp = FindFProperty<FProperty>(OwnerCls, *PropName);
            if (!ExtProp)
            {
                FailNode(Id, Handle, FString::Printf(
                    TEXT("property '%s' not found on class '%s'"), *PropName, *OwnerSpec));
                continue;
            }
            UBlueprintVariableNodeSpawner* PSp = UBlueprintVariableNodeSpawner::CreateFromMemberOrParam(
                bPropSetter ? UK2Node_VariableSet::StaticClass() : UK2Node_VariableGet::StaticClass(),
                ExtProp, nullptr, OwnerCls);
            if (PSp) { Spawned = PSp->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(X, Y)); }
            if (!Spawned) { FailNode(Id, Handle, TEXT("external-property spawner Invoke failed")); continue; }
        }
        // -- flow.branch / flow.sequence / flow.<macro> --
        else if (Handle.StartsWith(TEXT("flow.")))
        {
            const FString Verb = Handle.Mid(5);
            if (Verb.Equals(TEXT("branch"), ESearchCase::IgnoreCase))
            {
                Spawned = SpawnViaSpawner(UK2Node_IfThenElse::StaticClass(), Graph, X, Y);
            }
            else if (Verb.Equals(TEXT("sequence"), ESearchCase::IgnoreCase))
            {
                Spawned = SpawnViaSpawner(UK2Node_ExecutionSequence::StaticClass(), Graph, X, Y);
            }
            else
            {
                const FString MacroName = FlowVerbToMacroName(Verb);
                UEdGraph* MacroGraph = MacroName.IsEmpty() ? nullptr : FindStandardMacroGraph(MacroName);
                if (!MacroGraph)
                {
                    FailNode(Id, Handle, FString::Printf(
                        TEXT("flow verb '%s' is not a known StandardMacro"), *Verb));
                    continue;
                }
                Spawned = SpawnViaSpawner(UK2Node_MacroInstance::StaticClass(), Graph, X, Y,
                    [MacroGraph](UEdGraphNode* N)
                    {
                        if (UK2Node_MacroInstance* MI = Cast<UK2Node_MacroInstance>(N))
                        {
                            MI->SetMacroGraph(MacroGraph);
                        }
                    });
            }
            if (!Spawned) { FailNode(Id, Handle, TEXT("flow node spawn failed")); continue; }
        }
        // -- cast.<Class> --
        else if (Handle.StartsWith(TEXT("cast.")))
        {
            UClass* Target = AgenticMCPBlueprintHelpers::ResolveClassByName(Handle.Mid(5));
            if (!Target)
            {
                FailNode(Id, Handle, FString::Printf(TEXT("cast target class '%s' not found"), *Handle.Mid(5)));
                continue;
            }
            Spawned = SpawnViaSpawner(UK2Node_DynamicCast::StaticClass(), Graph, X, Y,
                [Target](UEdGraphNode* N)
                {
                    if (UK2Node_DynamicCast* C = Cast<UK2Node_DynamicCast>(N))
                    {
                        C->TargetType = Target;
                    }
                });
            if (!Spawned) { FailNode(Id, Handle, TEXT("cast node spawn failed")); continue; }
        }
        // -- spawn.<ActorClass> --
        else if (Handle.StartsWith(TEXT("spawn.")))
        {
            UClass* ActorCls = AgenticMCPBlueprintHelpers::ResolveClassByName(Handle.Mid(6));
            if (!ActorCls)
            {
                FailNode(Id, Handle, FString::Printf(TEXT("spawn class '%s' not found"), *Handle.Mid(6)));
                continue;
            }
            Spawned = SpawnViaSpawner(UK2Node_SpawnActorFromClass::StaticClass(), Graph, X, Y);
            if (!Spawned) { FailNode(Id, Handle, TEXT("spawn-actor node spawn failed")); continue; }
            // Class pin only exists after Invoke's AllocateDefaultPins; setting
            // it triggers OnClassPinChanged -> exposed-pin reconstruction (the
            // node's designed flow).
            if (UK2Node_SpawnActorFromClass* SP = Cast<UK2Node_SpawnActorFromClass>(Spawned))
            {
                if (UEdGraphPin* ClassPin = SP->GetClassPin())
                {
                    Schema->TrySetDefaultObject(*ClassPin, ActorCls);
                    SP->PinDefaultValueChanged(ClassPin);
                }
            }
        }
        // -- dispatch.<DispatcherName> --
        // Broadcasts the named event dispatcher on self (K2Node_CallDelegate).
        // Requires the dispatcher to be added via agentic_bp_add_event_dispatcher
        // and the BP compiled before this author call (skeleton resolution — P6).
        else if (Handle.StartsWith(TEXT("dispatch.")))
        {
            const FString DispName = Handle.Mid(9);
            FMulticastDelegateProperty* DelegateProp =
                FindFProperty<FMulticastDelegateProperty>(Blueprint->GeneratedClass, *DispName);
            if (!DelegateProp)
            {
                FailNode(Id, Handle, FString::Printf(
                    TEXT("event dispatcher '%s' not found on '%s' — call agentic_bp_add_event_dispatcher + agentic_bp_compile before this author call"),
                    *DispName, *Blueprint->GetName()));
                continue;
            }
            UBlueprintDelegateNodeSpawner* DelSpawner = UBlueprintDelegateNodeSpawner::Create(UK2Node_CallDelegate::StaticClass(), DelegateProp);
            if (!DelSpawner)
            {
                FailNode(Id, Handle, TEXT("dispatcher spawner creation failed"));
                continue;
            }
            Spawned = DelSpawner->Invoke(Graph, IBlueprintNodeBinder::FBindingSet(), FVector2D(X, Y));
            if (!Spawned) { FailNode(Id, Handle, TEXT("dispatcher call node spawn failed")); continue; }
        }
        // -- self / knot --
        else if (Handle.Equals(TEXT("self"), ESearchCase::IgnoreCase))
        {
            Spawned = SpawnViaSpawner(UK2Node_Self::StaticClass(), Graph, X, Y);
            if (!Spawned) { FailNode(Id, Handle, TEXT("self node spawn failed")); continue; }
        }
        else if (Handle.Equals(TEXT("knot"), ESearchCase::IgnoreCase))
        {
            Spawned = SpawnViaSpawner(UK2Node_Knot::StaticClass(), Graph, X, Y);
            if (!Spawned) { FailNode(Id, Handle, TEXT("knot node spawn failed")); continue; }
        }
        // -- node.<UClassShortName> --
        else if (Handle.StartsWith(TEXT("node.")))
        {
            UClass* NodeCls = AgenticMCPBlueprintHelpers::ResolveClassByName(Handle.Mid(5));
            if (!NodeCls || !NodeCls->IsChildOf(UEdGraphNode::StaticClass()))
            {
                FailNode(Id, Handle, FString::Printf(TEXT("node class '%s' not found / not a UEdGraphNode"), *Handle.Mid(5)));
                continue;
            }
            Spawned = SpawnViaSpawner(NodeCls, Graph, X, Y);
            if (!Spawned) { FailNode(Id, Handle, TEXT("generic node spawn failed")); continue; }
        }
        else
        {
            FailNode(Id, Handle, FString::Printf(
                TEXT("unrecognized handle prefix '%s' (use bp_actions_search to discover handles)"), *Handle));
            continue;
        }

        if (!Spawned)
        {
            FailNode(Id, Handle, TEXT("spawn produced null node"));
            continue;
        }

        IdToNode.Add(Id, Spawned);

        // Defer pin defaults until after the link pass (see DeferredPinDefaults).
        const TSharedPtr<FJsonObject>* PinDefaults = nullptr;
        if (NObj->TryGetObjectField(TEXT("pin_defaults"), PinDefaults) && PinDefaults && PinDefaults->IsValid())
        {
            DeferredPinDefaults.Add(FDeferredDefaults{ Id, Spawned, *PinDefaults });
        }

        TSharedPtr<FJsonObject> Added = MakeShared<FJsonObject>();
        Added->SetStringField(TEXT("id"), Id);
        Added->SetStringField(TEXT("ue_name"), Spawned->GetName());
        Added->SetStringField(TEXT("handle"), Handle);
        Added->SetNumberField(TEXT("x"), Spawned->NodePosX);
        Added->SetNumberField(TEXT("y"), Spawned->NodePosY);
        NodesAdded.Add(MakeShared<FJsonValueObject>(Added));
    }

    // --- links ---
    int32 LinksMade = 0;
    TArray<TSharedPtr<FJsonValue>> LinkFailures;

    auto ResolveEndpoint = [&IdToNode, Graph](const FString& NodeRef, const FString& PinRef,
                                               EEdGraphPinDirection Dir) -> UEdGraphPin*
    {
        UEdGraphNode* Node = nullptr;
        if (NodeRef.Equals(TEXT("entry"), ESearchCase::IgnoreCase))
        {
            for (UEdGraphNode* N : Graph->Nodes) { if (N && N->IsA<UK2Node_FunctionEntry>()) { Node = N; break; } }
        }
        else if (NodeRef.Equals(TEXT("return"), ESearchCase::IgnoreCase))
        {
            for (UEdGraphNode* N : Graph->Nodes) { if (N && N->IsA<UK2Node_FunctionResult>()) { Node = N; break; } }
        }
        else if (UEdGraphNode** Found = IdToNode.Find(NodeRef))
        {
            Node = *Found;
        }
        if (!Node)
        {
            return nullptr;
        }
        UEdGraphPin* Pin = FindPinFlexible(Node, PinRef, Dir);
        if (!Pin)
        {
            Pin = AgenticMCPBlueprintHelpers::FindPinByName(Node, PinRef, Dir);
        }
        return Pin;
    };

    if (LinksArr)
    {
        int32 LinkIdx = -1;
        for (const TSharedPtr<FJsonValue>& LV : *LinksArr)
        {
            ++LinkIdx;
            const TSharedPtr<FJsonObject> LObj = LV->AsObject();
            if (!LObj.IsValid())
            {
                continue;
            }
            FString FromNode, FromPin, ToNode, ToPin;
            FString F, T;
            if (LObj->TryGetStringField(TEXT("from"), F) && F.Contains(TEXT(".")))
            {
                int32 D; F.FindChar(TEXT('.'), D);
                FromNode = F.Left(D); FromPin = F.Mid(D + 1);
            }
            if (LObj->TryGetStringField(TEXT("to"), T) && T.Contains(TEXT(".")))
            {
                int32 D; T.FindChar(TEXT('.'), D);
                ToNode = T.Left(D); ToPin = T.Mid(D + 1);
            }
            LObj->TryGetStringField(TEXT("from_node"), FromNode);
            LObj->TryGetStringField(TEXT("from_pin"), FromPin);
            LObj->TryGetStringField(TEXT("to_node"), ToNode);
            LObj->TryGetStringField(TEXT("to_pin"), ToPin);

            auto FailLink = [&LinkFailures, LinkIdx](const FString& Err, const FString& Fr, const FString& To)
            {
                TSharedPtr<FJsonObject> LF = MakeShared<FJsonObject>();
                LF->SetNumberField(TEXT("index"), LinkIdx);
                LF->SetStringField(TEXT("from"), Fr);
                LF->SetStringField(TEXT("to"), To);
                LF->SetStringField(TEXT("error"), Err);
                LinkFailures.Add(MakeShared<FJsonValueObject>(LF));
            };
            const FString FrLabel = FromNode + TEXT(".") + FromPin;
            const FString ToLabel = ToNode + TEXT(".") + ToPin;

            if (FromNode.IsEmpty() || FromPin.IsEmpty() || ToNode.IsEmpty() || ToPin.IsEmpty())
            {
                FailLink(TEXT("missing from/from_pin/to/to_pin"), FrLabel, ToLabel);
                continue;
            }

            UEdGraphPin* PinFrom = ResolveEndpoint(FromNode, FromPin, EGPD_Output);
            UEdGraphPin* PinTo = ResolveEndpoint(ToNode, ToPin, EGPD_Input);

            // direction auto-swap
            if (!PinFrom || !PinTo)
            {
                UEdGraphPin* AltFrom = ResolveEndpoint(FromNode, FromPin, EGPD_Input);
                UEdGraphPin* AltTo = ResolveEndpoint(ToNode, ToPin, EGPD_Output);
                if (AltFrom && AltTo)
                {
                    PinFrom = AltTo;
                    PinTo = AltFrom;
                }
            }
            if (!PinFrom)
            {
                FailLink(FString::Printf(TEXT("output pin '%s' not found on '%s'"), *FromPin, *FromNode), FrLabel, ToLabel);
                continue;
            }
            if (!PinTo)
            {
                FailLink(FString::Printf(TEXT("input pin '%s' not found on '%s'"), *ToPin, *ToNode), FrLabel, ToLabel);
                continue;
            }
            if (Schema->TryCreateConnection(PinFrom, PinTo))
            {
                ++LinksMade;
            }
            else
            {
                FailLink(TEXT("schema rejected connection (type mismatch / direction)"), FrLabel, ToLabel);
            }
        }
    }

    // --- pin defaults (post-link: wildcards have resolved their type) ---
    // Every outcome is echoed in the response (pin_defaults_applied /
    // pin_default_failures) so a self-correcting caller sees exactly which
    // default landed and which was silently rejected — no dump, no PIE.
    for (const FDeferredDefaults& Entry : DeferredPinDefaults)
    {
        UEdGraphNode* Node = Entry.Node;
        if (!Node || !Entry.Defs.IsValid())
        {
            continue;
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Entry.Defs->Values)
        {
            UEdGraphPin* Pin = FindPinFlexible(Node, KV.Key, EGPD_Input);
            if (!Pin)
            {
                Pin = AgenticMCPBlueprintHelpers::FindPinByName(Node, KV.Key, EGPD_Input);
            }
            if (!Pin)
            {
                TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
                F->SetStringField(TEXT("node_id"), Entry.Id);
                F->SetStringField(TEXT("pin"), KV.Key);
                F->SetStringField(TEXT("error"), FString::Printf(
                    TEXT("input pin '%s' not found on node '%s'"), *KV.Key, *Entry.Id));
                PinDefaultFailures.Add(MakeShared<FJsonValueObject>(F));
                continue;
            }
            if (Pin->LinkedTo.Num() > 0)
            {
                // A wire overrides any literal default — record as skipped,
                // not failed (this is correct behaviour, surfaced for clarity).
                TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
                S->SetStringField(TEXT("node_id"), Entry.Id);
                S->SetStringField(TEXT("pin"), KV.Key);
                S->SetStringField(TEXT("status"), TEXT("skipped_connected"));
                PinDefaultsApplied.Add(MakeShared<FJsonValueObject>(S));
                continue;
            }
            const FString Err = ApplyPinDefault(Pin, KV.Value);
            if (Err.IsEmpty())
            {
                TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
                A->SetStringField(TEXT("node_id"), Entry.Id);
                A->SetStringField(TEXT("pin"), KV.Key);
                A->SetStringField(TEXT("status"), TEXT("applied"));
                A->SetStringField(TEXT("value"),
                    Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : Pin->DefaultValue);
                PinDefaultsApplied.Add(MakeShared<FJsonValueObject>(A));
            }
            else
            {
                TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
                F->SetStringField(TEXT("node_id"), Entry.Id);
                F->SetStringField(TEXT("pin"), KV.Key);
                F->SetStringField(TEXT("error"), Err);
                PinDefaultFailures.Add(MakeShared<FJsonValueObject>(F));
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    FString CompileStatus;
    if (bCompileAfter)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave);
        switch (Blueprint->Status)
        {
        case BS_UpToDate:             CompileStatus = TEXT("UpToDate"); break;
        case BS_UpToDateWithWarnings: CompileStatus = TEXT("UpToDateWithWarnings"); break;
        case BS_Error:                CompileStatus = TEXT("Error"); break;
        case BS_Dirty:                CompileStatus = TEXT("Dirty"); break;
        default:                      CompileStatus = TEXT("Unknown"); break;
        }
    }
    if (bSaveAfter && Blueprint->Status != BS_Error)
    {
        UEditorAssetLibrary::SaveLoadedAsset(Blueprint, /*bOnlyIfDirty*/false);
    }

    const int32 NodeFailCount = NodeFailures.Num();
    const int32 LinkFailCount = LinkFailures.Num();
    const int32 PinDefFailCount = PinDefaultFailures.Num();

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("graph"), GraphName);
    Data->SetNumberField(TEXT("cleared"), ClearedCount);
    Data->SetArrayField(TEXT("nodes_added"), NodesAdded);
    Data->SetNumberField(TEXT("nodes_added_count"), NodesAdded.Num());
    Data->SetNumberField(TEXT("links_made"), LinksMade);
    Data->SetArrayField(TEXT("node_failures"), NodeFailures);
    Data->SetArrayField(TEXT("link_failures"), LinkFailures);
    Data->SetArrayField(TEXT("pin_defaults_applied"), PinDefaultsApplied);
    Data->SetArrayField(TEXT("pin_default_failures"), PinDefaultFailures);
    Data->SetArrayField(TEXT("repairs_made"), RepairsMade);
    Data->SetArrayField(TEXT("pre_flight_issues"), PreFlightIssues);
    if (bCompileAfter)
    {
        Data->SetStringField(TEXT("compile_status"), CompileStatus);
    }

    if (NodeFailCount > 0 || LinkFailCount > 0 || PinDefFailCount > 0 || CompileStatus == TEXT("Error"))
    {
        // HARD CONTRACT (see file header §5): a partial success is still a
        // failure. Flip success:false but keep the FULL data envelope —
        // node/link/pin-default failure lists, repairs_made, pre_flight
        // issues — so the caller sees exactly what to fix and never has to
        // re-run blind to find out. Never early-return a bare error here.
        TSharedPtr<FJsonObject> Resp = FAgenticMCPCommonUtils::CreateSuccessResponse(Data);
        Resp->SetBoolField(TEXT("success"), false);
        Resp->SetStringField(TEXT("error"), FString::Printf(
            TEXT("%d node, %d link, %d pin-default failure(s)%s — see data.node_failures / "
                 "data.link_failures / data.pin_default_failures"),
            NodeFailCount, LinkFailCount, PinDefFailCount,
            CompileStatus == TEXT("Error") ? TEXT(", compile FAILED") : TEXT("")));
        return Resp;
    }
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Data);
}
