// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Commands/AgenticMCPBlueprintCommands.h"
#include "Commands/AgenticMCPCommonUtils.h"
#include "Commands/AgenticMCPBlueprintHelpers.h"
#include "AgenticMCPModule.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/Engine.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Composite.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Knot.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_Self.h"
#include "K2Node_EditablePinBase.h"
#include "BlueprintEditorLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Algo/Reverse.h"
#include "EngineUtils.h"  // TActorIterator
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/TokenizedMessage.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "WidgetBlueprint.h"
#include "Animation/AnimBlueprint.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_LinkedInputPose.h"

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("set_physics_properties"))
        return HandleSetPhysicsProperties(Params);
    if (CommandType == TEXT("set_static_mesh_properties"))
        return HandleSetStaticMeshProperties(Params);
    if (CommandType == TEXT("set_mesh_material_color"))
        return HandleSetMeshMaterialColor(Params);

    // BP-Expert Phase 1: Read-only introspection
    if (CommandType == TEXT("bp_inspect"))
        return HandleBpInspect(Params);
    if (CommandType == TEXT("bp_summary"))
        return HandleBpSummary(Params);
    if (CommandType == TEXT("bp_get_function_flow"))
        return HandleBpGetFunctionFlow(Params);
    if (CommandType == TEXT("bp_list_overridable_functions"))
        return HandleBpListOverridableFunctions(Params);
    if (CommandType == TEXT("bp_get_components"))
        return HandleBpGetComponents(Params);
    if (CommandType == TEXT("bp_get_interfaces"))
        return HandleBpGetInterfaces(Params);

    // BP-Expert Phase 2: Bulk graph editing
    if (CommandType == TEXT("bp_add_nodes"))
        return HandleBpAddNodes(Params);
    if (CommandType == TEXT("bp_configure_node"))
        return HandleBpConfigureNode(Params);
    if (CommandType == TEXT("bp_link_nodes"))
        return HandleBpLinkNodes(Params);
    if (CommandType == TEXT("bp_remove_nodes"))
        return HandleBpRemoveNodes(Params);
    if (CommandType == TEXT("bp_clear_graph"))
        return HandleBpClearGraph(Params);
    if (CommandType == TEXT("bp_build_graph"))
        return HandleBpBuildGraph(Params);

    // BP-Expert Phase 3: Variables, functions, dispatchers, RPCs
    if (CommandType == TEXT("bp_add_variables"))
        return HandleBpAddVariables(Params);
    if (CommandType == TEXT("bp_rename_variable"))
        return HandleBpRenameVariable(Params);
    if (CommandType == TEXT("bp_set_variable_default"))
        return HandleBpSetVariableDefault(Params);
    if (CommandType == TEXT("bp_set_variable_flags"))
        return HandleBpSetVariableFlags(Params);
    if (CommandType == TEXT("bp_add_function"))
        return HandleBpAddFunction(Params);
    if (CommandType == TEXT("bp_set_function_access"))
        return HandleBpSetFunctionAccess(Params);
    if (CommandType == TEXT("bp_set_function_pure"))
        return HandleBpSetFunctionPure(Params);
    if (CommandType == TEXT("bp_set_function_replication"))
        return HandleBpSetFunctionReplication(Params);
    if (CommandType == TEXT("bp_implement_interface"))
        return HandleBpImplementInterface(Params);
    if (CommandType == TEXT("bp_remove_interface"))
        return HandleBpRemoveInterface(Params);

    // BP-Expert Phase 4: SCS / Components
    if (CommandType == TEXT("bp_add_components"))
        return HandleBpAddComponents(Params);
    if (CommandType == TEXT("bp_remove_components"))
        return HandleBpRemoveComponents(Params);
    if (CommandType == TEXT("bp_attach_component"))
        return HandleBpAttachComponent(Params);
    if (CommandType == TEXT("bp_rename_component"))
        return HandleBpRenameComponent(Params);
    if (CommandType == TEXT("bp_get_component_details"))
        return HandleBpGetComponentDetails(Params);
    if (CommandType == TEXT("bp_set_component_property"))
        return HandleBpSetComponentProperty(Params);
    if (CommandType == TEXT("bp_set_component_collision"))
        return HandleBpSetComponentCollision(Params);
    if (CommandType == TEXT("bp_set_component_mobility"))
        return HandleBpSetComponentMobility(Params);
    if (CommandType == TEXT("bp_set_component_transform"))
        return HandleBpSetComponentTransform(Params);

    // BP-Expert Phase 6: Compilation, validation, analysis
    if (CommandType == TEXT("bp_diff"))
        return HandleBpDiff(Params);
    if (CommandType == TEXT("bp_replace_references"))
        return HandleBpReplaceReferences(Params);
    if (CommandType == TEXT("bp_estimate_complexity"))
        return HandleBpEstimateComplexity(Params);
    if (CommandType == TEXT("bp_search"))
        return HandleBpSearch(Params);

    return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FAgenticMCPCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        float Mass = Params->GetNumberField(TEXT("mass"));
        // In UE5.5, use proper overrideMass instead of just scaling
        PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
        UE_LOG(LogAgenticMCP, Display, TEXT("Set mass for component %s to %f kg"), *ComponentName, Mass);
    }

    if (Params->HasField(TEXT("linear_damping")))
    {
        PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
    }

    if (Params->HasField(TEXT("angular_damping")))
    {
        PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FAgenticMCPCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    if (Params->HasField(TEXT("static_mesh")))
    {
        FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (Mesh)
        {
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        FString MaterialPath = Params->GetStringField(TEXT("material"));
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            MeshComponent->SetMaterial(0, Material);
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    UBlueprint* Blueprint = FAgenticMCPCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    const TArray<TSharedPtr<FJsonValue>>* ColorJsonArray;
    if (!Params->TryGetArrayField(TEXT("color"), ColorJsonArray) || ColorJsonArray->Num() != 4)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("'color' must be an array of 4 float values [R, G, B, A]"));
    }

    TArray<float> ColorArray;
    for (const TSharedPtr<FJsonValue>& Value : *ColorJsonArray)
    {
        ColorArray.Add(FMath::Clamp(Value->AsNumber(), 0.0f, 1.0f));
    }

    FLinearColor Color(ColorArray[0], ColorArray[1], ColorArray[2], ColorArray[3]);

    int32 MaterialSlot = 0;
    if (Params->HasField(TEXT("material_slot")))
    {
        MaterialSlot = Params->GetIntegerField(TEXT("material_slot"));
    }

    FString ParameterName = TEXT("BaseColor");
    Params->TryGetStringField(TEXT("parameter_name"), ParameterName);

    UMaterialInterface* Material = nullptr;
    FString MaterialPath;
    if (Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (!Material)
        {
            return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
        }
    }
    else
    {
        Material = PrimComponent->GetMaterial(MaterialSlot);
        if (!Material)
        {
            Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
            if (!Material)
            {
                return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("No material found on component and failed to load default material"));
            }
        }
    }

    UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(Material, PrimComponent);
    if (!DynMaterial)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance"));
    }

    DynMaterial->SetVectorParameterValue(*ParameterName, Color);
    PrimComponent->SetMaterial(MaterialSlot, DynMaterial);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    ResultObj->SetNumberField(TEXT("material_slot"), MaterialSlot);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    
    TArray<TSharedPtr<FJsonValue>> ColorResultArray;
    ColorResultArray.Reserve(4);
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.R));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.G));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.B));
    ColorResultArray.Add(MakeShared<FJsonValueNumber>(Color.A));
    ResultObj->SetArrayField(TEXT("color"), ColorResultArray);
    
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =============================================================================
// BP-Expert Phase 1 — Read-only introspection
// =============================================================================

namespace AgenticBpInspect
{
    // LoadBlueprintOrError + PinTypeToJson live in AgenticMCPBlueprintHelpers
    // so future sibling command classes can reuse them. Re-exposed here so
    // `using namespace AgenticBpInspect;` at the call site keeps working
    // unchanged across this file.
    using AgenticMCPBlueprintHelpers::LoadBlueprintOrError;
    using AgenticMCPBlueprintHelpers::PinTypeToJson;

    /** Map FBPVariableDescription to a JSON variable entry with full metadata. */
    static TSharedPtr<FJsonObject> VariableToJson(const FBPVariableDescription& Var)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), Var.VarName.ToString());
        Obj->SetStringField(TEXT("guid"), Var.VarGuid.ToString());
        Obj->SetObjectField(TEXT("type"), PinTypeToJson(Var.VarType));
        Obj->SetStringField(TEXT("default_value"), Var.DefaultValue);
        Obj->SetStringField(TEXT("category"), Var.Category.ToString());
        Obj->SetStringField(TEXT("friendly_name"), Var.FriendlyName);

        const uint64 Flags = Var.PropertyFlags;
        Obj->SetBoolField(TEXT("instance_editable"),  (Flags & CPF_Edit) != 0);
        Obj->SetBoolField(TEXT("expose_on_spawn"),    (Flags & CPF_ExposeOnSpawn) != 0);
        Obj->SetBoolField(TEXT("blueprint_read_only"),(Flags & CPF_BlueprintReadOnly) != 0);
        Obj->SetBoolField(TEXT("private"),            (Flags & CPF_DisableEditOnInstance) != 0);
        Obj->SetBoolField(TEXT("transient"),          (Flags & CPF_Transient) != 0);
        Obj->SetBoolField(TEXT("save_game"),          (Flags & CPF_SaveGame) != 0);
        Obj->SetBoolField(TEXT("config"),             (Flags & CPF_Config) != 0);
        Obj->SetBoolField(TEXT("replicated"),         (Flags & CPF_Net) != 0);
        Obj->SetBoolField(TEXT("rep_notify"),         (Flags & CPF_RepNotify) != 0);
        if ((Flags & CPF_RepNotify) != 0)
        {
            Obj->SetStringField(TEXT("rep_notify_func"), Var.RepNotifyFunc.ToString());
        }

        const TCHAR* CondStr = TEXT("None");
        switch (Var.ReplicationCondition)
        {
            case COND_InitialOnly:        CondStr = TEXT("InitialOnly"); break;
            case COND_OwnerOnly:          CondStr = TEXT("OwnerOnly"); break;
            case COND_SkipOwner:          CondStr = TEXT("SkipOwner"); break;
            case COND_SimulatedOnly:      CondStr = TEXT("SimulatedOnly"); break;
            case COND_AutonomousOnly:     CondStr = TEXT("AutonomousOnly"); break;
            case COND_SimulatedOrPhysics: CondStr = TEXT("SimulatedOrPhysics"); break;
            case COND_InitialOrOwner:     CondStr = TEXT("InitialOrOwner"); break;
            case COND_Custom:             CondStr = TEXT("Custom"); break;
            case COND_ReplayOrOwner:      CondStr = TEXT("ReplayOrOwner"); break;
            case COND_ReplayOnly:         CondStr = TEXT("ReplayOnly"); break;
            case COND_SimulatedOnlyNoReplay: CondStr = TEXT("SimulatedOnlyNoReplay"); break;
            case COND_SimulatedOrPhysicsNoReplay: CondStr = TEXT("SimulatedOrPhysicsNoReplay"); break;
            case COND_SkipReplay:         CondStr = TEXT("SkipReplay"); break;
            case COND_Never:              CondStr = TEXT("Never"); break;
            default: break;
        }
        Obj->SetStringField(TEXT("replication_condition"), CondStr);

        TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
        for (const FBPVariableMetaDataEntry& Entry : Var.MetaDataArray)
        {
            Meta->SetStringField(Entry.DataKey.ToString(), Entry.DataValue);
        }
        Obj->SetObjectField(TEXT("metadata"), Meta);
        return Obj;
    }

    /**
     * Write Blueprint-graph function flags + access specifier into an existing JSON object.
     * Shared between the function-list block in HandleBpInspect and FunctionSignatureToJson.
     * Takes EFunctionFlags directly so it works from both UK2Node_FunctionEntry and UFunction.
     */
    static void WriteFunctionFlagsToJson(TSharedPtr<FJsonObject>& InOutObj, const EFunctionFlags Flags)
    {
        InOutObj->SetBoolField(TEXT("is_pure"),         (Flags & FUNC_BlueprintPure) != 0);
        InOutObj->SetBoolField(TEXT("is_const"),        (Flags & FUNC_Const) != 0);
        InOutObj->SetBoolField(TEXT("is_net_server"),   (Flags & FUNC_NetServer) != 0);
        InOutObj->SetBoolField(TEXT("is_net_client"),   (Flags & FUNC_NetClient) != 0);
        InOutObj->SetBoolField(TEXT("is_net_multicast"),(Flags & FUNC_NetMulticast) != 0);
        InOutObj->SetBoolField(TEXT("is_reliable"),     (Flags & FUNC_NetReliable) != 0);

        const TCHAR* Access = TEXT("Public");
        if (Flags & FUNC_Protected)    Access = TEXT("Protected");
        else if (Flags & FUNC_Private) Access = TEXT("Private");
        InOutObj->SetStringField(TEXT("access"), Access);
    }

    /** Map a UFunction signature to JSON (for overridable function listings + interface impls). */
    static TSharedPtr<FJsonObject> FunctionSignatureToJson(UFunction* Func)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), Func->GetName());
        Obj->SetBoolField(TEXT("is_event"), Func->HasAnyFunctionFlags(FUNC_BlueprintEvent));
        WriteFunctionFlagsToJson(Obj, Func->FunctionFlags);

        TArray<TSharedPtr<FJsonValue>> Inputs, Outputs;
        for (TFieldIterator<FProperty> It(Func); It; ++It)
        {
            FProperty* Prop = *It;
            if (!Prop->HasAnyPropertyFlags(CPF_Parm)) continue;
            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), Prop->GetName());
            ParamObj->SetStringField(TEXT("cpp_type"), Prop->GetCPPType());
            const bool bIsReference = Prop->HasAnyPropertyFlags(CPF_ReferenceParm);
            const bool bIsConst     = Prop->HasAnyPropertyFlags(CPF_ConstParm);
            const bool bIsReturn    = Prop->HasAnyPropertyFlags(CPF_ReturnParm);
            const bool bIsOut       = Prop->HasAnyPropertyFlags(CPF_OutParm);
            ParamObj->SetBoolField(TEXT("is_reference"), bIsReference);
            ParamObj->SetBoolField(TEXT("is_const"),     bIsConst);

            // BP "out" parameters carry CPF_OutParm; many of those ALSO carry
            // CPF_ReferenceParm (UPARAM(ref) ref-outs — written back to caller).
            // Const-ref inputs (`const T&`) also have CPF_ReferenceParm but lack
            // CPF_OutParm. So:
            //   - Return value (CPF_ReturnParm)               → Output
            //   - Out param (CPF_OutParm), incl. ref-out      → Output
            //   - Anything else (incl. const-ref inputs)      → Input
            if (bIsReturn || bIsOut)
            {
                Outputs.Add(MakeShared<FJsonValueObject>(ParamObj));
            }
            else
            {
                Inputs.Add(MakeShared<FJsonValueObject>(ParamObj));
            }
        }
        Obj->SetArrayField(TEXT("inputs"), Inputs);
        Obj->SetArrayField(TEXT("outputs"), Outputs);
        return Obj;
    }

    /** Classify a UEdGraph (Ubergraph / Function / Macro / ConstructionScript / Animation / Other). */
    static FString ClassifyGraph(UBlueprint* Blueprint, UEdGraph* Graph)
    {
        if (!Graph) return TEXT("Unknown");
        if (Blueprint->UbergraphPages.Contains(Graph))         return TEXT("Ubergraph");
        if (Blueprint->FunctionGraphs.Contains(Graph))         return TEXT("Function");
        if (Blueprint->MacroGraphs.Contains(Graph))            return TEXT("Macro");
        if (Blueprint->IntermediateGeneratedGraphs.Contains(Graph)) return TEXT("Intermediate");
        if (Blueprint->DelegateSignatureGraphs.Contains(Graph)) return TEXT("DelegateSignature");
        return TEXT("Other");
    }

    /** Status enum → string. */
    static FString BlueprintStatusToString(EBlueprintStatus Status)
    {
        switch (Status)
        {
            case BS_UpToDate:                   return TEXT("UpToDate");
            case BS_Dirty:                      return TEXT("Dirty");
            case BS_Error:                      return TEXT("Error");
            case BS_UpToDateWithWarnings:       return TEXT("UpToDateWithWarnings");
            case BS_BeingCreated:               return TEXT("BeingCreated");
            case BS_Unknown:
            default:                            return TEXT("Unknown");
        }
    }

    /** Blueprint type → string (Blueprint / AnimBlueprint / WidgetBlueprint / Other). */
    static FString BlueprintTypeToString(UBlueprint* Blueprint)
    {
        if (Blueprint->IsA<UAnimBlueprint>())   return TEXT("AnimBlueprint");
        if (Blueprint->IsA<UWidgetBlueprint>()) return TEXT("WidgetBlueprint");
        return TEXT("Blueprint");
    }

    /** Pre-built O(1)-lookup index over an SCS tree — eliminates O(N²)
     *  parent-walks and root-membership scans when serializing many nodes. */
    struct FSCSIndex
    {
        TMap<USCS_Node*, USCS_Node*> ChildToParent;   // every child → its parent
        TSet<USCS_Node*> Roots;                       // root-node membership

        explicit FSCSIndex(USimpleConstructionScript* SCS)
        {
            if (!SCS) return;
            for (USCS_Node* Root : SCS->GetRootNodes())
            {
                Roots.Add(Root);
            }
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (!Node) continue;
                for (USCS_Node* Child : Node->GetChildNodes())
                {
                    if (Child) ChildToParent.Add(Child, Node);
                }
            }
        }
    };

    /** SCS node → JSON entry (without CDO snapshot). */
    static TSharedPtr<FJsonObject> SCSNodeToJson(USCS_Node* Node, const FSCSIndex& Index)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
        Obj->SetStringField(TEXT("guid"), Node->VariableGuid.ToString());
        Obj->SetStringField(TEXT("class"),
            Node->ComponentClass ? Node->ComponentClass->GetPathName() : TEXT("None"));
        if (USCS_Node* const* Parent = Index.ChildToParent.Find(Node))
        {
            Obj->SetStringField(TEXT("parent"), (*Parent)->GetVariableName().ToString());
        }
        Obj->SetBoolField(TEXT("is_root"), Index.Roots.Contains(Node));
        Obj->SetStringField(TEXT("attach_socket"), Node->AttachToName.ToString());
        return Obj;
    }
}

// ─── HandleBpInspect ────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpInspect(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"), Path);
    Result->SetStringField(TEXT("name"), Blueprint->GetName());
    Result->SetStringField(TEXT("blueprint_type"), BlueprintTypeToString(Blueprint));
    Result->SetStringField(TEXT("parent_class"),
        Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT("None"));
    Result->SetStringField(TEXT("status"), BlueprintStatusToString(Blueprint->Status));
    Result->SetBoolField(TEXT("is_data_only"), FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint));

    // Components
    TArray<TSharedPtr<FJsonValue>> Components;
    if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
        Components.Reserve(AllNodes.Num());
        const FSCSIndex Index(SCS);
        for (USCS_Node* Node : AllNodes)
        {
            if (!Node) continue;
            Components.Add(MakeShared<FJsonValueObject>(SCSNodeToJson(Node, Index)));
        }
    }
    Result->SetArrayField(TEXT("components"), Components);

    TArray<TSharedPtr<FJsonValue>> Variables;
    Variables.Reserve(Blueprint->NewVariables.Num());
    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        Variables.Add(MakeShared<FJsonValueObject>(VariableToJson(Var)));
    }
    Result->SetArrayField(TEXT("variables"), Variables);

    TArray<TSharedPtr<FJsonValue>> Functions;
    Functions.Reserve(Blueprint->FunctionGraphs.Num());
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
        FuncObj->SetStringField(TEXT("name"), Graph->GetName());
        FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

        // Single pass: collect flags from FunctionEntry and pin shapes from Entry + Result nodes.
        TArray<TSharedPtr<FJsonValue>> Inputs, Outputs;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
            {
                WriteFunctionFlagsToJson(FuncObj, static_cast<EFunctionFlags>(Entry->GetFunctionFlags()));
                FuncObj->SetStringField(TEXT("category"), Entry->MetaData.Category.ToString());

                for (UEdGraphPin* Pin : Entry->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                    {
                        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
                        P->SetStringField(TEXT("name"), Pin->PinName.ToString());
                        P->SetObjectField(TEXT("type"), PinTypeToJson(Pin->PinType));
                        P->SetStringField(TEXT("default_value"), Pin->DefaultValue);
                        Inputs.Add(MakeShared<FJsonValueObject>(P));
                    }
                }
            }
            else if (UK2Node_FunctionResult* FuncResult = Cast<UK2Node_FunctionResult>(Node))
            {
                for (UEdGraphPin* Pin : FuncResult->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                    {
                        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
                        P->SetStringField(TEXT("name"), Pin->PinName.ToString());
                        P->SetObjectField(TEXT("type"), PinTypeToJson(Pin->PinType));
                        Outputs.Add(MakeShared<FJsonValueObject>(P));
                    }
                }
            }
        }
        FuncObj->SetArrayField(TEXT("inputs"), Inputs);
        FuncObj->SetArrayField(TEXT("outputs"), Outputs);
        Functions.Add(MakeShared<FJsonValueObject>(FuncObj));
    }
    Result->SetArrayField(TEXT("functions"), Functions);

    TArray<TSharedPtr<FJsonValue>> EventGraphs;
    EventGraphs.Reserve(Blueprint->UbergraphPages.Num());
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
        GraphObj->SetStringField(TEXT("name"), Graph->GetName());
        GraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
        TArray<TSharedPtr<FJsonValue>> Events;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                TSharedPtr<FJsonObject> EvObj = MakeShared<FJsonObject>();
                EvObj->SetStringField(TEXT("name"), EventNode->EventReference.GetMemberName().ToString());
                EvObj->SetStringField(TEXT("title"), EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
                EvObj->SetStringField(TEXT("node_handle"), EventNode->GetName());
                EvObj->SetBoolField(TEXT("is_custom"), EventNode->IsA<UK2Node_CustomEvent>());
                EvObj->SetBoolField(TEXT("is_override"), EventNode->bOverrideFunction);
                Events.Add(MakeShared<FJsonValueObject>(EvObj));
            }
        }
        GraphObj->SetArrayField(TEXT("events"), Events);
        EventGraphs.Add(MakeShared<FJsonValueObject>(GraphObj));
    }
    Result->SetArrayField(TEXT("event_graphs"), EventGraphs);

    TArray<TSharedPtr<FJsonValue>> Macros;
    Macros.Reserve(Blueprint->MacroGraphs.Num());
    for (UEdGraph* Graph : Blueprint->MacroGraphs)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("name"), Graph->GetName());
        M->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
        Macros.Add(MakeShared<FJsonValueObject>(M));
    }
    Result->SetArrayField(TEXT("macros"), Macros);

    TArray<TSharedPtr<FJsonValue>> Interfaces;
    Interfaces.Reserve(Blueprint->ImplementedInterfaces.Num());
    for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
    {
        if (!Iface.Interface) continue;
        TSharedPtr<FJsonObject> IfaceObj = MakeShared<FJsonObject>();
        IfaceObj->SetStringField(TEXT("interface"), Iface.Interface->GetPathName());
        TArray<TSharedPtr<FJsonValue>> ImplGraphs;
        for (UEdGraph* G : Iface.Graphs)
        {
            if (G) ImplGraphs.Add(MakeShared<FJsonValueString>(G->GetName()));
        }
        IfaceObj->SetArrayField(TEXT("implemented_graphs"), ImplGraphs);
        Interfaces.Add(MakeShared<FJsonValueObject>(IfaceObj));
    }
    Result->SetArrayField(TEXT("interfaces"), Interfaces);

    // Event dispatchers — stored as DelegateSignatureGraphs; parameters on their FunctionEntry.
    TArray<TSharedPtr<FJsonValue>> Delegates;
    Delegates.Reserve(Blueprint->DelegateSignatureGraphs.Num());
    for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> D = MakeShared<FJsonObject>();
        D->SetStringField(TEXT("name"), Graph->GetName());
        TArray<TSharedPtr<FJsonValue>> ParamArray;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(Node))
            {
                for (UEdGraphPin* Pin : E->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                    {
                        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
                        P->SetStringField(TEXT("name"), Pin->PinName.ToString());
                        P->SetObjectField(TEXT("type"), PinTypeToJson(Pin->PinType));
                        ParamArray.Add(MakeShared<FJsonValueObject>(P));
                    }
                }
            }
        }
        D->SetArrayField(TEXT("params"), ParamArray);
        Delegates.Add(MakeShared<FJsonValueObject>(D));
    }
    Result->SetArrayField(TEXT("delegates"), Delegates);

    FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(Blueprint));
    TSharedPtr<FJsonObject> RefsObj = MakeShared<FJsonObject>();
    if (AssetData.IsValid())
    {
        TArray<FAssetIdentifier> Referencers, Dependencies;
        AssetRegistry.Get().GetReferencers(AssetData.PackageName, Referencers);
        AssetRegistry.Get().GetDependencies(AssetData.PackageName, Dependencies);
        RefsObj->SetNumberField(TEXT("referenced_by_count"), Referencers.Num());
        RefsObj->SetNumberField(TEXT("depends_on_count"), Dependencies.Num());
    }
    else
    {
        RefsObj->SetNumberField(TEXT("referenced_by_count"), 0);
        RefsObj->SetNumberField(TEXT("depends_on_count"), 0);
    }
    Result->SetObjectField(TEXT("references"), RefsObj);

    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSummary ───────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSummary(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"), Path);
    Result->SetStringField(TEXT("name"), Blueprint->GetName());
    Result->SetStringField(TEXT("blueprint_type"), BlueprintTypeToString(Blueprint));
    Result->SetStringField(TEXT("parent_class"),
        Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT("None"));
    Result->SetStringField(TEXT("status"), BlueprintStatusToString(Blueprint->Status));
    Result->SetBoolField(TEXT("is_data_only"), FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint));

    TSharedPtr<FJsonObject> Counts = MakeShared<FJsonObject>();
    Counts->SetNumberField(TEXT("variables"), Blueprint->NewVariables.Num());
    Counts->SetNumberField(TEXT("components"),
        Blueprint->SimpleConstructionScript ? Blueprint->SimpleConstructionScript->GetAllNodes().Num() : 0);
    Counts->SetNumberField(TEXT("functions"), Blueprint->FunctionGraphs.Num());
    Counts->SetNumberField(TEXT("macros"), Blueprint->MacroGraphs.Num());
    Counts->SetNumberField(TEXT("event_graphs"), Blueprint->UbergraphPages.Num());
    Counts->SetNumberField(TEXT("interfaces"), Blueprint->ImplementedInterfaces.Num());
    Counts->SetNumberField(TEXT("delegates"), Blueprint->DelegateSignatureGraphs.Num());

    int32 EventCount = 0;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (!Graph) continue;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Cast<UK2Node_Event>(Node)) ++EventCount;
        }
    }
    Counts->SetNumberField(TEXT("events"), EventCount);
    Result->SetObjectField(TEXT("counts"), Counts);

    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpGetFunctionFlow ───────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpGetFunctionFlow(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path, FunctionName;
    if (!Params->TryGetStringField(TEXT("path"), Path) ||
        !Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' or 'function_name' parameter"));
    }

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    // Find the named graph in functions or ubergraph (so the user can ask for "EventGraph" too)
    UEdGraph* Graph = nullptr;
    for (UEdGraph* G : Blueprint->FunctionGraphs)
    {
        if (G && G->GetName() == FunctionName) { Graph = G; break; }
    }
    if (!Graph)
    {
        for (UEdGraph* G : Blueprint->UbergraphPages)
        {
            if (G && G->GetName() == FunctionName) { Graph = G; break; }
        }
    }
    if (!Graph)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Graph '%s' not found in Blueprint %s"), *FunctionName, *Path));
    }

    // Find the entry: K2Node_FunctionEntry or first event in ubergraph
    UEdGraphNode* StartNode = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Cast<UK2Node_FunctionEntry>(Node)) { StartNode = Node; break; }
    }
    if (!StartNode)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Cast<UK2Node_Event>(Node)) { StartNode = Node; break; }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("function_name"), FunctionName);
    Result->SetStringField(TEXT("graph_type"), ClassifyGraph(Blueprint, Graph));
    if (!StartNode)
    {
        Result->SetArrayField(TEXT("exec_chain"), TArray<TSharedPtr<FJsonValue>>());
        Result->SetStringField(TEXT("warning"), TEXT("No FunctionEntry or Event found in graph"));
        return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
    }

    // Walk exec output → linked node, repeat
    TArray<TSharedPtr<FJsonValue>> Chain;
    TSet<UEdGraphNode*> Visited;
    UEdGraphNode* Current = StartNode;
    while (Current && !Visited.Contains(Current))
    {
        Visited.Add(Current);
        TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
        Step->SetStringField(TEXT("node_handle"), Current->GetName());
        Step->SetStringField(TEXT("class"), Current->GetClass()->GetName());
        Step->SetStringField(TEXT("title"), Current->GetNodeTitle(ENodeTitleType::ListView).ToString());
        Chain.Add(MakeShared<FJsonValueObject>(Step));

        UEdGraphNode* Next = nullptr;
        for (UEdGraphPin* Pin : Current->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output &&
                Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
                Pin->LinkedTo.Num() > 0)
            {
                if (UEdGraphPin* Linked = Pin->LinkedTo[0])
                {
                    Next = Linked->GetOwningNode();
                    break;
                }
            }
        }
        Current = Next;
    }

    Result->SetArrayField(TEXT("exec_chain"), Chain);
    Result->SetNumberField(TEXT("chain_length"), Chain.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpListOverridableFunctions ──────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpListOverridableFunctions(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    if (!Blueprint->ParentClass)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no parent class"));
    }

    // Walk parent UClass for BlueprintEvent functions (overridable from BP)
    TArray<TSharedPtr<FJsonValue>> Funcs;
    TSet<FName> AlreadyOverridden;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph) AlreadyOverridden.Add(*Graph->GetName());
    }
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (!Graph) continue;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
            {
                if (Event->bOverrideFunction)
                {
                    AlreadyOverridden.Add(Event->EventReference.GetMemberName());
                }
            }
        }
    }

    for (TFieldIterator<UFunction> It(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        UFunction* Func = *It;
        if (!Func) continue;
        const bool bIsBpEvent = Func->HasAnyFunctionFlags(FUNC_BlueprintEvent);
        const bool bIsBpCallable = Func->HasAnyFunctionFlags(FUNC_BlueprintCallable);
        if (!bIsBpEvent && !bIsBpCallable) continue;
        if (Func->HasAnyFunctionFlags(FUNC_Final)) continue;
        if (Func->HasAnyFunctionFlags(FUNC_Private)) continue;

        TSharedPtr<FJsonObject> FuncObj = FunctionSignatureToJson(Func);
        FuncObj->SetStringField(TEXT("from_class"),
            Func->GetOuterUClass() ? Func->GetOuterUClass()->GetPathName() : TEXT("Unknown"));
        FuncObj->SetBoolField(TEXT("is_overridden"), AlreadyOverridden.Contains(Func->GetFName()));
        Funcs.Add(MakeShared<FJsonValueObject>(FuncObj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("functions"), Funcs);
    Result->SetNumberField(TEXT("function_count"), Funcs.Num());
    Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetPathName());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpGetComponents ─────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpGetComponents(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    TArray<TSharedPtr<FJsonValue>> SCSComps;
    if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& AllNodes = SCS->GetAllNodes();
        SCSComps.Reserve(AllNodes.Num());
        const FSCSIndex Index(SCS);
        for (USCS_Node* Node : AllNodes)
        {
            if (!Node) continue;
            SCSComps.Add(MakeShared<FJsonValueObject>(SCSNodeToJson(Node, Index)));
        }
    }

    // Native components from parent CDO — not visible in BP's SCS.
    TArray<TSharedPtr<FJsonValue>> InheritedComps;
    if (Blueprint->ParentClass && Blueprint->ParentClass->GetDefaultObject())
    {
        AActor* CDO = Cast<AActor>(Blueprint->ParentClass->GetDefaultObject());
        if (CDO)
        {
            TArray<UActorComponent*> NativeComps;
            CDO->GetComponents(NativeComps);
            for (UActorComponent* Comp : NativeComps)
            {
                if (!Comp) continue;
                TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
                CompObj->SetStringField(TEXT("name"), Comp->GetName());
                CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetPathName());
                CompObj->SetBoolField(TEXT("is_native"), true);
                InheritedComps.Add(MakeShared<FJsonValueObject>(CompObj));
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("scs_components"), SCSComps);
    Result->SetArrayField(TEXT("inherited_components"), InheritedComps);
    Result->SetNumberField(TEXT("scs_count"), SCSComps.Num());
    Result->SetNumberField(TEXT("inherited_count"), InheritedComps.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpGetInterfaces ─────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpGetInterfaces(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    TArray<TSharedPtr<FJsonValue>> Interfaces;
    for (const FBPInterfaceDescription& Iface : Blueprint->ImplementedInterfaces)
    {
        if (!Iface.Interface) continue;
        TSharedPtr<FJsonObject> IfaceObj = MakeShared<FJsonObject>();
        IfaceObj->SetStringField(TEXT("interface"), Iface.Interface->GetPathName());

        // List functions on the interface UClass
        TArray<TSharedPtr<FJsonValue>> Funcs;
        for (TFieldIterator<UFunction> It(Iface.Interface, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        {
            Funcs.Add(MakeShared<FJsonValueObject>(FunctionSignatureToJson(*It)));
        }
        IfaceObj->SetArrayField(TEXT("interface_functions"), Funcs);

        // List which functions this BP has implemented (graphs)
        TArray<TSharedPtr<FJsonValue>> ImplGraphs;
        for (UEdGraph* G : Iface.Graphs)
        {
            if (G) ImplGraphs.Add(MakeShared<FJsonValueString>(G->GetName()));
        }
        IfaceObj->SetArrayField(TEXT("implemented_graphs"), ImplGraphs);
        Interfaces.Add(MakeShared<FJsonValueObject>(IfaceObj));
    }

    // Also report interfaces inherited from parent class (native)
    TArray<TSharedPtr<FJsonValue>> NativeIfaces;
    if (Blueprint->ParentClass)
    {
        for (const FImplementedInterface& Native : Blueprint->ParentClass->Interfaces)
        {
            if (!Native.Class) continue;
            TSharedPtr<FJsonObject> NIObj = MakeShared<FJsonObject>();
            NIObj->SetStringField(TEXT("interface"), Native.Class->GetPathName());
            NIObj->SetBoolField(TEXT("is_native"), true);
            NativeIfaces.Add(MakeShared<FJsonValueObject>(NIObj));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("implemented_interfaces"), Interfaces);
    Result->SetArrayField(TEXT("native_interfaces"), NativeIfaces);
    Result->SetNumberField(TEXT("implemented_count"), Interfaces.Num());
    Result->SetNumberField(TEXT("native_count"), NativeIfaces.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── Phase 2: bulk graph editing ────────────────────────────────────────

namespace AgenticBpEdit
{
    // ResolveClassByName, ResolveFunctionByPath, FindGraphByName, FindNodeByExactName,
    // FindPinByName, NodeToBriefJson, PinToBriefJson, ApplyNodeConfig all live in
    // AgenticMCPBlueprintHelpers so future sibling command classes can reuse them.
    // Re-exposed here so `using namespace AgenticBpEdit;` at the call site keeps
    // working unchanged across this file.
    using AgenticMCPBlueprintHelpers::ResolveClassByName;
    using AgenticMCPBlueprintHelpers::ResolveFunctionByPath;
    using AgenticMCPBlueprintHelpers::FindGraphByName;
    using AgenticMCPBlueprintHelpers::FindNodeByExactName;
    using AgenticMCPBlueprintHelpers::FindPinByName;
    using AgenticMCPBlueprintHelpers::NodeToBriefJson;
    using AgenticMCPBlueprintHelpers::PinToBriefJson;
    using AgenticMCPBlueprintHelpers::ApplyNodeConfig;

}  // namespace AgenticBpEdit

// ─── HandleBpAddNodes ───────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpAddNodes(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;

    FString Path, GraphName;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("graph"), GraphName))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph' (graph name)"));

    const TArray<TSharedPtr<FJsonValue>>* NodeSpecs = nullptr;
    if (!Params->TryGetArrayField(TEXT("nodes"), NodeSpecs) || !NodeSpecs)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'nodes' array"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Graph '%s' not found in %s"), *GraphName, *Path));
    }

    Blueprint->Modify();
    Graph->Modify();

    TArray<TSharedPtr<FJsonValue>> Created;
    int32 ErrorCount = 0;
    Created.Reserve(NodeSpecs->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *NodeSpecs)
    {
        TSharedPtr<FJsonObject> ResultEntry = MakeShared<FJsonObject>();
        if (!Entry.IsValid() || Entry->Type != EJson::Object)
        {
            ResultEntry->SetStringField(TEXT("error"), TEXT("node entry is not a JSON object"));
            ++ErrorCount;
            Created.Add(MakeShared<FJsonValueObject>(ResultEntry));
            continue;
        }
        const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();
        FString IdLabel; Spec->TryGetStringField(TEXT("id"), IdLabel);
        if (!IdLabel.IsEmpty()) ResultEntry->SetStringField(TEXT("id"), IdLabel);

        FString NodeClassSpec;
        if (!Spec->TryGetStringField(TEXT("node_class"), NodeClassSpec))
        {
            ResultEntry->SetStringField(TEXT("error"), TEXT("node_class missing"));
            ++ErrorCount;
            Created.Add(MakeShared<FJsonValueObject>(ResultEntry));
            continue;
        }
        ResultEntry->SetStringField(TEXT("requested_class"), NodeClassSpec);

        UClass* NodeClass = ResolveClassByName(NodeClassSpec);
        if (!NodeClass || !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
        {
            ResultEntry->SetStringField(TEXT("error"),
                FString::Printf(TEXT("Class '%s' is not a UEdGraphNode subclass"), *NodeClassSpec));
            ++ErrorCount;
            Created.Add(MakeShared<FJsonValueObject>(ResultEntry));
            continue;
        }
        if (NodeClass->HasAnyClassFlags(CLASS_Abstract))
        {
            ResultEntry->SetStringField(TEXT("error"),
                FString::Printf(TEXT("Class '%s' is abstract"), *NodeClassSpec));
            ++ErrorCount;
            Created.Add(MakeShared<FJsonValueObject>(ResultEntry));
            continue;
        }

        double X = 0.0, Y = 0.0;
        Spec->TryGetNumberField(TEXT("x"), X);
        Spec->TryGetNumberField(TEXT("y"), Y);
        FString Comment;
        Spec->TryGetStringField(TEXT("comment"), Comment);

        const TSharedPtr<FJsonObject>* ConfigPtr = nullptr;
        Spec->TryGetObjectField(TEXT("config"), ConfigPtr);
        TSharedPtr<FJsonObject> Config = (ConfigPtr && ConfigPtr->IsValid()) ? *ConfigPtr : nullptr;

        UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeClass);
        Node->SetFlags(RF_Transactional);

        const FString CfgErr = ApplyNodeConfig(Node, Blueprint, Config);
        if (!CfgErr.IsEmpty())
        {
            // Drop the transient node before reporting.
            Node->ConditionalBeginDestroy();
            ResultEntry->SetStringField(TEXT("error"), CfgErr);
            ++ErrorCount;
            Created.Add(MakeShared<FJsonValueObject>(ResultEntry));
            continue;
        }

        Node->NodePosX = static_cast<int32>(X);
        Node->NodePosY = static_cast<int32>(Y);
        if (!Comment.IsEmpty()) Node->NodeComment = Comment;

        Graph->AddNode(Node, /*bFromUI=*/false, /*bSelectNewNode=*/false);
        Node->CreateNewGuid();
        Node->PostPlacedNewNode();
        Node->AllocateDefaultPins();

        ResultEntry->SetObjectField(TEXT("node"), NodeToBriefJson(Node));

        TArray<TSharedPtr<FJsonValue>> PinList;
        PinList.Reserve(Node->Pins.Num());
        for (UEdGraphPin* P : Node->Pins)
        {
            if (P) PinList.Add(MakeShared<FJsonValueObject>(PinToBriefJson(P)));
        }
        ResultEntry->SetArrayField(TEXT("pins"), PinList);

        Created.Add(MakeShared<FJsonValueObject>(ResultEntry));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),   Path);
    Result->SetStringField(TEXT("graph"),       GraphName);
    Result->SetArrayField (TEXT("nodes"),       Created);
    Result->SetNumberField(TEXT("node_count"),  Created.Num());
    Result->SetNumberField(TEXT("error_count"), ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpConfigureNode ──────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpConfigureNode(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;

    FString Path, GraphName, NodeName;
    if (!Params->TryGetStringField(TEXT("path"),  Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("graph"), GraphName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph'"));
    if (!Params->TryGetStringField(TEXT("node"),  NodeName))  return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node'"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
    UEdGraphNode* Node = FindNodeByExactName(Graph, NodeName);
    if (!Node) return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Node '%s' not found in graph '%s'"), *NodeName, *GraphName));

    Blueprint->Modify();
    Graph->Modify();
    Node->Modify();

    TArray<TSharedPtr<FJsonValue>> Applied;
    TArray<TSharedPtr<FJsonValue>> Errors;
    bool bNeedsReconstruct = false;

    // properties: { propName: jsonValue, ... } — direct FProperty reflection on the node.
    const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && PropertiesPtr->IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PropertiesPtr)->Values)
        {
            FString SetErr;
            if (FAgenticMCPCommonUtils::SetObjectProperty(Node, Pair.Key, Pair.Value, SetErr))
            {
                Applied.Add(MakeShared<FJsonValueString>(Pair.Key));
                bNeedsReconstruct = true;
            }
            else
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetStringField(TEXT("property"), Pair.Key);
                E->SetStringField(TEXT("error"),    SetErr);
                Errors.Add(MakeShared<FJsonValueObject>(E));
            }
        }
    }

    // pin_defaults: { pinName: "stringValue", ... } — sets DefaultValue on input pins.
    const TSharedPtr<FJsonObject>* PinDefaultsPtr = nullptr;
    int32 PinDefaultsApplied = 0;
    if (Params->TryGetObjectField(TEXT("pin_defaults"), PinDefaultsPtr) && PinDefaultsPtr && PinDefaultsPtr->IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PinDefaultsPtr)->Values)
        {
            UEdGraphPin* Pin = FindPinByName(Node, Pair.Key, EGPD_Input);
            if (!Pin)
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetStringField(TEXT("pin"),   Pair.Key);
                E->SetStringField(TEXT("error"), TEXT("Input pin not found"));
                Errors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            FString StrVal;
            if (Pair.Value.IsValid()) Pair.Value->TryGetString(StrVal);
            Pin->Modify();
            Pin->DefaultValue = StrVal;
            ++PinDefaultsApplied;
        }
    }

    // Optional explicit position update.
    double X = 0.0, Y = 0.0;
    bool bMoved = false;
    if (Params->TryGetNumberField(TEXT("x"), X)) { Node->NodePosX = static_cast<int32>(X); bMoved = true; }
    if (Params->TryGetNumberField(TEXT("y"), Y)) { Node->NodePosY = static_cast<int32>(Y); bMoved = true; }

    bool bForceReconstruct = false;
    Params->TryGetBoolField(TEXT("reconstruct"), bForceReconstruct);
    if (bNeedsReconstruct || bForceReconstruct)
    {
        Node->ReconstructNode();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),     Path);
    Result->SetStringField(TEXT("graph"),         GraphName);
    Result->SetStringField(TEXT("node"),          NodeName);
    Result->SetArrayField (TEXT("applied"),       Applied);
    Result->SetNumberField(TEXT("pin_defaults_applied"), PinDefaultsApplied);
    Result->SetArrayField (TEXT("errors"),        Errors);
    Result->SetBoolField  (TEXT("moved"),         bMoved);
    Result->SetBoolField  (TEXT("reconstructed"), bNeedsReconstruct || bForceReconstruct);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpLinkNodes ──────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpLinkNodes(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;

    FString Path, GraphName;
    if (!Params->TryGetStringField(TEXT("path"),  Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("graph"), GraphName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph'"));

    const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
    if (!Params->TryGetArrayField(TEXT("links"), Links) || !Links)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'links' array"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

    const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
    if (!Schema) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Graph schema is not K2"));

    Blueprint->Modify();
    Graph->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 LinkedCount = 0;
    int32 ErrorCount = 0;
    Results.Reserve(Links->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *Links)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        if (!Entry.IsValid() || Entry->Type != EJson::Object)
        {
            R->SetStringField(TEXT("error"), TEXT("link entry is not a JSON object"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        const TSharedPtr<FJsonObject>& Link = Entry->AsObject();
        FString FromNode, FromPin, ToNode, ToPin;
        if (!Link->TryGetStringField(TEXT("from_node"), FromNode) ||
            !Link->TryGetStringField(TEXT("from_pin"),  FromPin)  ||
            !Link->TryGetStringField(TEXT("to_node"),   ToNode)   ||
            !Link->TryGetStringField(TEXT("to_pin"),    ToPin))
        {
            R->SetStringField(TEXT("error"), TEXT("link needs from_node/from_pin/to_node/to_pin"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("from"), FString::Printf(TEXT("%s:%s"), *FromNode, *FromPin));
        R->SetStringField(TEXT("to"),   FString::Printf(TEXT("%s:%s"), *ToNode,   *ToPin));

        UEdGraphNode* SrcNode = FindNodeByExactName(Graph, FromNode);
        UEdGraphNode* DstNode = FindNodeByExactName(Graph, ToNode);
        if (!SrcNode || !DstNode)
        {
            R->SetStringField(TEXT("error"), FString::Printf(TEXT("node not found: %s"),
                !SrcNode ? *FromNode : *ToNode));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        UEdGraphPin* SrcPin = FindPinByName(SrcNode, FromPin, EGPD_Output);
        UEdGraphPin* DstPin = FindPinByName(DstNode, ToPin,   EGPD_Input);
        if (!SrcPin || !DstPin)
        {
            R->SetStringField(TEXT("error"), FString::Printf(TEXT("pin not found: %s"),
                !SrcPin ? *FromPin : *ToPin));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        const bool bConnected = Schema->TryCreateConnection(SrcPin, DstPin);
        R->SetBoolField(TEXT("connected"), bConnected);
        if (!bConnected)
        {
            R->SetStringField(TEXT("error"),
                TEXT("Schema rejected connection (incompatible types or pin direction)"));
            ++ErrorCount;
        }
        else
        {
            ++LinkedCount;
        }
        Results.Add(MakeShared<FJsonValueObject>(R));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),    Path);
    Result->SetStringField(TEXT("graph"),        GraphName);
    Result->SetArrayField (TEXT("links"),        Results);
    Result->SetNumberField(TEXT("link_count"),   LinkedCount);
    Result->SetNumberField(TEXT("error_count"),  ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpRemoveNodes ────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpRemoveNodes(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;

    FString Path, GraphName;
    if (!Params->TryGetStringField(TEXT("path"),  Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("graph"), GraphName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph'"));

    const TArray<TSharedPtr<FJsonValue>>* NodeNames = nullptr;
    if (!Params->TryGetArrayField(TEXT("nodes"), NodeNames) || !NodeNames)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'nodes' array of names"));

    bool bAllowTerminators = false;
    Params->TryGetBoolField(TEXT("allow_terminators"), bAllowTerminators);

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

    Blueprint->Modify();
    Graph->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 RemovedCount = 0;
    int32 ErrorCount = 0;
    Results.Reserve(NodeNames->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *NodeNames)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        FString NodeName;
        if (!Entry.IsValid() || !Entry->TryGetString(NodeName))
        {
            R->SetStringField(TEXT("error"), TEXT("node entry must be a string name"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("node"), NodeName);
        UEdGraphNode* Node = FindNodeByExactName(Graph, NodeName);
        if (!Node)
        {
            R->SetStringField(TEXT("error"), TEXT("node not found"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        if (!bAllowTerminators && Node->IsA<UK2Node_FunctionTerminator>())
        {
            R->SetStringField(TEXT("error"), TEXT("function terminator preserved (pass allow_terminators=true to override)"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        if (!Node->CanUserDeleteNode())
        {
            R->SetStringField(TEXT("error"), TEXT("node forbids user deletion"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        Node->Modify();
        Node->BreakAllNodeLinks();
        Graph->RemoveNode(Node);
        R->SetBoolField(TEXT("removed"), true);
        ++RemovedCount;
        Results.Add(MakeShared<FJsonValueObject>(R));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),     Path);
    Result->SetStringField(TEXT("graph"),         GraphName);
    Result->SetArrayField (TEXT("results"),       Results);
    Result->SetNumberField(TEXT("removed_count"), RemovedCount);
    Result->SetNumberField(TEXT("error_count"),   ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpArrangeGraph ───────────────────────────────────────────────
// Two-phase layout: (A) exec-chain nodes are row-packed per BFS-depth
// column left-to-right, defining the Y anchors. (B) Data-only nodes
// (variable getters, math ops, constants) are placed near the Y of their
// downstream consumer, processed right-to-left so chained data nodes
// (var → op → exec) each resolve to their consumer's already-placed Y.

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpArrangeGraph(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;

    FString Path, GraphName;
    if (!Params->TryGetStringField(TEXT("path"),  Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("graph"), GraphName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph'"));

    int32 ColSpacing = 360;
    int32 RowSpacing = 200;
    int32 OriginX    = 0;
    int32 OriginY    = 0;
    double Tmp = 0.0;
    if (Params->TryGetNumberField(TEXT("column_spacing"), Tmp)) ColSpacing = static_cast<int32>(Tmp);
    if (Params->TryGetNumberField(TEXT("row_spacing"),    Tmp)) RowSpacing = static_cast<int32>(Tmp);
    if (Params->TryGetNumberField(TEXT("origin_x"),       Tmp)) OriginX    = static_cast<int32>(Tmp);
    if (Params->TryGetNumberField(TEXT("origin_y"),       Tmp)) OriginY    = static_cast<int32>(Tmp);

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

    Blueprint->Modify();
    Graph->Modify();

    // Compute exec-flow depth: BFS from nodes with no incoming exec edge.
    TMap<UEdGraphNode*, int32> Depth;
    Depth.Reserve(Graph->Nodes.Num());

    auto HasIncomingExec = [](UEdGraphNode* N) -> bool
    {
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P) continue;
            if (P->Direction != EGPD_Input) continue;
            if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
            if (P->LinkedTo.Num() > 0) return true;
        }
        return false;
    };

    TArray<UEdGraphNode*> Queue;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (!N) continue;
        if (!HasIncomingExec(N))
        {
            Depth.Add(N, 0);
            Queue.Add(N);
        }
    }

    while (Queue.Num() > 0)
    {
        UEdGraphNode* N = Queue[0];
        Queue.RemoveAt(0);
        const int32 D = Depth[N];
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P || P->Direction != EGPD_Output) continue;
            if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
            for (UEdGraphPin* Linked : P->LinkedTo)
            {
                if (!Linked || !Linked->GetOwningNode()) continue;
                UEdGraphNode* Child = Linked->GetOwningNode();
                int32* Existing = Depth.Find(Child);
                if (!Existing || *Existing < D + 1)
                {
                    Depth.Add(Child, D + 1);
                    Queue.Add(Child);
                }
            }
        }
    }

    // Data-only nodes: place one column left of their leftmost consumer.
    // Negative depths are allowed so chains (var → op → fn → exec) spread
    // across distinct columns instead of collapsing into column 0.
    // Fixed-point iteration resolves chains in arbitrary order. Depths
    // only increase so convergence is guaranteed.
    TSet<UEdGraphNode*> ExecSet;
    for (const TPair<UEdGraphNode*, int32>& Pair : Depth)
        ExecSet.Add(Pair.Key);

    // Seed unassigned (data-only) nodes at MIN_int32/2 so any consumer
    // link is an improvement.
    constexpr int32 Unassigned = MIN_int32 / 2;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (N && !Depth.Contains(N))
            Depth.Add(N, Unassigned);
    }

    bool bChanged = true;
    while (bChanged)
    {
        bChanged = false;
        for (UEdGraphNode* N : Graph->Nodes)
        {
            if (!N || ExecSet.Contains(N)) continue;
            int32 Best = Unassigned;
            for (UEdGraphPin* P : N->Pins)
            {
                if (!P || P->Direction != EGPD_Output) continue;
                if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
                for (UEdGraphPin* Linked : P->LinkedTo)
                {
                    UEdGraphNode* Consumer = Linked ? Linked->GetOwningNode() : nullptr;
                    if (!Consumer) continue;
                    if (const int32* D = Depth.Find(Consumer))
                    {
                        if (*D != Unassigned)
                            Best = FMath::Max(Best, *D - 1);
                    }
                }
            }
            // Nodes with no resolved consumer stay at 0.
            const int32 NewDepth = (Best == Unassigned) ? 0 : Best;
            if (NewDepth != Depth[N])
            {
                Depth.Add(N, NewDepth);
                bChanged = true;
            }
        }
    }

    // Shift all depths so the minimum becomes 0 (no negative columns).
    int32 MinDepth = 0;
    for (const TPair<UEdGraphNode*, int32>& Pair : Depth)
        MinDepth = FMath::Min(MinDepth, Pair.Value);
    if (MinDepth < 0)
    {
        for (TPair<UEdGraphNode*, int32>& Pair : Depth)
            Pair.Value -= MinDepth;
    }

    // ── Classify: exec-chain nodes vs data-only nodes ─────────────────
    auto IsOnExecChain = [](UEdGraphNode* N) -> bool
    {
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P) continue;
            if (P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
            if (P->LinkedTo.Num() > 0) return true;
        }
        return false;
    };

    TMap<int32, TArray<UEdGraphNode*>> ExecColumns;
    TMap<int32, TArray<UEdGraphNode*>> DataColumns;
    int32 MaxCol = 0;
    for (const TPair<UEdGraphNode*, int32>& Pair : Depth)
    {
        if (IsOnExecChain(Pair.Key))
            ExecColumns.FindOrAdd(Pair.Value).Add(Pair.Key);
        else
            DataColumns.FindOrAdd(Pair.Value).Add(Pair.Key);
        MaxCol = FMath::Max(MaxCol, Pair.Value);
    }

    // Sort exec nodes by current Y for stable ordering.
    for (auto& Pair : ExecColumns)
    {
        Pair.Value.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
        {
            return A.NodePosY < B.NodePosY;
        });
    }

    // ── Phase A: Place exec-chain nodes (row-packed per column) ──────
    int32 MovedCount = 0;
    TMap<int32, TSet<int32>> OccupiedY; // Col → set of occupied Y slots

    for (const auto& Pair : ExecColumns)
    {
        const int32 Col = Pair.Key;
        const int32 X   = OriginX + Col * ColSpacing;
        int32 RowIdx = 0;
        for (UEdGraphNode* N : Pair.Value)
        {
            const int32 Y = OriginY + RowIdx * RowSpacing;
            N->Modify();
            N->NodePosX = X;
            N->NodePosY = Y;
            OccupiedY.FindOrAdd(Col).Add(Y);
            ++RowIdx;
            ++MovedCount;
        }
    }

    // ── Phase B: Place data-only nodes near their downstream consumers
    // Process columns right-to-left so chained data nodes (var→op→exec)
    // resolve: the rightmost data node gets placed first using the exec
    // node's Y, then the next-left data node uses the now-placed data
    // node's Y. Result: each data node sits near its consumer.
    auto GetAffinityY = [&](UEdGraphNode* N) -> int32
    {
        int32 TotalY = 0;
        int32 Count  = 0;
        for (UEdGraphPin* P : N->Pins)
        {
            if (!P || P->Direction != EGPD_Output) continue;
            if (P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
            for (UEdGraphPin* Linked : P->LinkedTo)
            {
                UEdGraphNode* Consumer = Linked ? Linked->GetOwningNode() : nullptr;
                if (!Consumer) continue;
                TotalY += Consumer->NodePosY;
                ++Count;
            }
        }
        return Count > 0 ? TotalY / Count : OriginY;
    };

    for (int32 Col = MaxCol; Col >= 0; --Col)
    {
        TArray<UEdGraphNode*>* NodesPtr = DataColumns.Find(Col);
        if (!NodesPtr || NodesPtr->Num() == 0) continue;
        TArray<UEdGraphNode*>& Nodes = *NodesPtr;

        // Compute affinity Y per node and sort by it.
        TMap<UEdGraphNode*, int32> Affinity;
        for (UEdGraphNode* N : Nodes)
            Affinity.Add(N, GetAffinityY(N));

        Nodes.Sort([&Affinity](const UEdGraphNode& A, const UEdGraphNode& B)
        {
            const int32* YA = Affinity.Find(const_cast<UEdGraphNode*>(&A));
            const int32* YB = Affinity.Find(const_cast<UEdGraphNode*>(&B));
            return (YA ? *YA : 0) < (YB ? *YB : 0);
        });

        // Place each node at the nearest free slot to its affinity Y.
        const int32 X = OriginX + Col * ColSpacing;
        TSet<int32>& ColOccupied = OccupiedY.FindOrAdd(Col);

        for (UEdGraphNode* N : Nodes)
        {
            int32 TargetY = Affinity[N];
            // Snap to row grid (round to nearest).
            TargetY = ((TargetY + RowSpacing / 2) / RowSpacing) * RowSpacing;
            // Search downward for first unoccupied slot.
            while (ColOccupied.Contains(TargetY))
                TargetY += RowSpacing;

            ColOccupied.Add(TargetY);
            N->Modify();
            N->NodePosX = X;
            N->NodePosY = TargetY;
            ++MovedCount;
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),     Path);
    Result->SetStringField(TEXT("graph"),         GraphName);
    Result->SetNumberField(TEXT("moved"),         MovedCount);
    Result->SetNumberField(TEXT("column_count"),  MaxCol + 1);
    Result->SetNumberField(TEXT("column_spacing"),ColSpacing);
    Result->SetNumberField(TEXT("row_spacing"),   RowSpacing);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpClearGraph ─────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpClearGraph(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;

    FString Path, GraphName;
    if (!Params->TryGetStringField(TEXT("path"),  Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("graph"), GraphName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph'"));

    bool bPreserveTerminators = true;
    Params->TryGetBoolField(TEXT("preserve_terminators"), bPreserveTerminators);

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);
    if (!Graph) return FAgenticMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Graph '%s' not found"), *GraphName));

    Blueprint->Modify();
    Graph->Modify();

    TArray<UEdGraphNode*> Survivors;
    TArray<UEdGraphNode*> ToRemove;
    Survivors.Reserve(Graph->Nodes.Num());
    ToRemove .Reserve(Graph->Nodes.Num());
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (!N) continue;
        const bool bIsTerminator = N->IsA<UK2Node_FunctionTerminator>();
        if (bPreserveTerminators && bIsTerminator)
        {
            Survivors.Add(N);
        }
        else if (!N->CanUserDeleteNode())
        {
            Survivors.Add(N);
        }
        else
        {
            ToRemove.Add(N);
        }
    }

    int32 RemovedCount = 0;
    for (UEdGraphNode* N : ToRemove)
    {
        N->Modify();
        N->BreakAllNodeLinks();
        Graph->RemoveNode(N);
        ++RemovedCount;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),     Path);
    Result->SetStringField(TEXT("graph"),         GraphName);
    Result->SetNumberField(TEXT("removed_count"), RemovedCount);
    Result->SetNumberField(TEXT("preserved"),     Survivors.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpBuildGraph ─────────────────────────────────────────────────
// Composite: spec.nodes (add) → spec.configure (per-node properties / pin defaults)
// → spec.links (wire) → optional auto-arrange. Returns aggregated diagnostics.

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpBuildGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString Path, GraphName;
    if (!Params->TryGetStringField(TEXT("path"),  Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("graph"), GraphName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'graph'"));

    const TSharedPtr<FJsonObject>* SpecPtr = nullptr;
    if (!Params->TryGetObjectField(TEXT("spec"), SpecPtr) || !SpecPtr || !SpecPtr->IsValid())
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'spec' object {nodes, configure, links, auto_arrange?}"));
    const TSharedPtr<FJsonObject>& Spec = *SpecPtr;

    // 1) Add nodes — re-uses HandleBpAddNodes verbatim.
    const TArray<TSharedPtr<FJsonValue>>* SpecNodes = nullptr;
    TSharedPtr<FJsonObject> AddResult;
    if (Spec->TryGetArrayField(TEXT("nodes"), SpecNodes) && SpecNodes && SpecNodes->Num() > 0)
    {
        TSharedPtr<FJsonObject> AddParams = MakeShared<FJsonObject>();
        AddParams->SetStringField(TEXT("path"),  Path);
        AddParams->SetStringField(TEXT("graph"), GraphName);
        AddParams->SetArrayField (TEXT("nodes"), *SpecNodes);
        AddResult = HandleBpAddNodes(AddParams);
        if (!AddResult.IsValid())
        {
            return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("HandleBpAddNodes returned null"));
        }
        bool bSuccess = false;
        AddResult->TryGetBoolField(TEXT("success"), bSuccess);
        if (!bSuccess) return AddResult;
    }

    // Build id→assigned_name map from add result for downstream link/configure resolution.
    // CreateSuccessResponse flattens fields onto the top-level response, so the
    // `nodes` array lives directly on AddResult — there is no `data` sub-object.
    TMap<FString, FString> IdToAssigned;
    if (AddResult.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* CreatedNodes = nullptr;
        if (AddResult->TryGetArrayField(TEXT("nodes"), CreatedNodes) && CreatedNodes)
        {
            for (const TSharedPtr<FJsonValue>& V : *CreatedNodes)
            {
                if (!V.IsValid() || V->Type != EJson::Object) continue;
                const TSharedPtr<FJsonObject>& O = V->AsObject();
                FString Id;
                if (!O->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty()) continue;
                const TSharedPtr<FJsonObject>* NodeBrief = nullptr;
                if (O->TryGetObjectField(TEXT("node"), NodeBrief) && NodeBrief && NodeBrief->IsValid())
                {
                    FString Assigned;
                    if ((*NodeBrief)->TryGetStringField(TEXT("name"), Assigned))
                    {
                        IdToAssigned.Add(Id, Assigned);
                    }
                }
            }
        }
    }

    auto ResolveRef = [&IdToAssigned](const FString& Ref) -> FString
    {
        if (const FString* Hit = IdToAssigned.Find(Ref)) return *Hit;
        return Ref;
    };

    // 2) Configure — array of {node|id, properties?, pin_defaults?, x?, y?}
    const TArray<TSharedPtr<FJsonValue>>* SpecConfigs = nullptr;
    TArray<TSharedPtr<FJsonValue>> ConfigureResults;
    if (Spec->TryGetArrayField(TEXT("configure"), SpecConfigs) && SpecConfigs)
    {
        for (const TSharedPtr<FJsonValue>& V : *SpecConfigs)
        {
            if (!V.IsValid() || V->Type != EJson::Object) continue;
            const TSharedPtr<FJsonObject>& C = V->AsObject();
            FString NodeRef;
            if (!C->TryGetStringField(TEXT("node"), NodeRef))
                C->TryGetStringField(TEXT("id"), NodeRef);
            if (NodeRef.IsEmpty()) continue;

            TSharedPtr<FJsonObject> CfgParams = MakeShared<FJsonObject>();
            CfgParams->SetStringField(TEXT("path"),  Path);
            CfgParams->SetStringField(TEXT("graph"), GraphName);
            CfgParams->SetStringField(TEXT("node"),  ResolveRef(NodeRef));
            const TSharedPtr<FJsonObject>* Props = nullptr;
            if (C->TryGetObjectField(TEXT("properties"), Props) && Props)
                CfgParams->SetObjectField(TEXT("properties"), *Props);
            const TSharedPtr<FJsonObject>* PD = nullptr;
            if (C->TryGetObjectField(TEXT("pin_defaults"), PD) && PD)
                CfgParams->SetObjectField(TEXT("pin_defaults"), *PD);
            double Tmp = 0.0;
            if (C->TryGetNumberField(TEXT("x"), Tmp)) CfgParams->SetNumberField(TEXT("x"), Tmp);
            if (C->TryGetNumberField(TEXT("y"), Tmp)) CfgParams->SetNumberField(TEXT("y"), Tmp);
            TSharedPtr<FJsonObject> CfgRes = HandleBpConfigureNode(CfgParams);
            if (CfgRes.IsValid())
            {
                ConfigureResults.Add(MakeShared<FJsonValueObject>(CfgRes));
            }
        }
    }

    // 3) Links — accept id-or-name in from_node/to_node.
    const TArray<TSharedPtr<FJsonValue>>* SpecLinks = nullptr;
    TSharedPtr<FJsonObject> LinkResult;
    if (Spec->TryGetArrayField(TEXT("links"), SpecLinks) && SpecLinks && SpecLinks->Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> ResolvedLinks;
        ResolvedLinks.Reserve(SpecLinks->Num());
        for (const TSharedPtr<FJsonValue>& V : *SpecLinks)
        {
            if (!V.IsValid() || V->Type != EJson::Object) continue;
            const TSharedPtr<FJsonObject>& L = V->AsObject();
            TSharedPtr<FJsonObject> Resolved = MakeShared<FJsonObject>();
            FString S;
            if (L->TryGetStringField(TEXT("from_node"), S)) Resolved->SetStringField(TEXT("from_node"), ResolveRef(S));
            if (L->TryGetStringField(TEXT("from_pin"),  S)) Resolved->SetStringField(TEXT("from_pin"),  S);
            if (L->TryGetStringField(TEXT("to_node"),   S)) Resolved->SetStringField(TEXT("to_node"),   ResolveRef(S));
            if (L->TryGetStringField(TEXT("to_pin"),    S)) Resolved->SetStringField(TEXT("to_pin"),    S);
            ResolvedLinks.Add(MakeShared<FJsonValueObject>(Resolved));
        }
        TSharedPtr<FJsonObject> LinkParams = MakeShared<FJsonObject>();
        LinkParams->SetStringField(TEXT("path"),  Path);
        LinkParams->SetStringField(TEXT("graph"), GraphName);
        LinkParams->SetArrayField (TEXT("links"), ResolvedLinks);
        LinkResult = HandleBpLinkNodes(LinkParams);
    }

    // 4) Optional auto-arrange.
    bool bAutoArrange = false;
    TSharedPtr<FJsonObject> ArrangeResult;
    if (Spec->TryGetBoolField(TEXT("auto_arrange"), bAutoArrange) && bAutoArrange)
    {
        TSharedPtr<FJsonObject> ArrParams = MakeShared<FJsonObject>();
        ArrParams->SetStringField(TEXT("path"),  Path);
        ArrParams->SetStringField(TEXT("graph"), GraphName);
        double Tmp = 0.0;
        if (Spec->TryGetNumberField(TEXT("column_spacing"), Tmp)) ArrParams->SetNumberField(TEXT("column_spacing"), Tmp);
        if (Spec->TryGetNumberField(TEXT("row_spacing"),    Tmp)) ArrParams->SetNumberField(TEXT("row_spacing"),    Tmp);
        ArrangeResult = HandleBpArrangeGraph(ArrParams);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"), Path);
    Result->SetStringField(TEXT("graph"),     GraphName);
    if (AddResult.IsValid())     Result->SetObjectField(TEXT("add_result"),     AddResult);
    Result->SetArrayField (TEXT("configure_results"), ConfigureResults);
    if (LinkResult.IsValid())    Result->SetObjectField(TEXT("link_result"),    LinkResult);
    if (ArrangeResult.IsValid()) Result->SetObjectField(TEXT("arrange_result"), ArrangeResult);
    Result->SetNumberField(TEXT("id_map_size"), IdToAssigned.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── Phase 3: variables, functions, dispatchers, RPCs ───────────────────

namespace AgenticBpAuthor
{
    static bool BuildPinTypeFromJson(const TSharedPtr<FJsonObject>& Spec,
                                     FEdGraphPinType& OutPinType,
                                     FString& OutErr)
    {
        return FAgenticMCPCommonUtils::BuildPinTypeFromJson(Spec, OutPinType, OutErr);
    }

    /** Find a member variable description (mutable ref into Blueprint->NewVariables). */
    static FBPVariableDescription* FindMemberVariable(UBlueprint* Blueprint, const FName& VarName)
    {
        for (FBPVariableDescription& V : Blueprint->NewVariables)
        {
            if (V.VarName == VarName) return &V;
        }
        return nullptr;
    }

    /** Resolve "InitialOnly|OwnerOnly|SkipOwner|SimulatedOnly|AutonomousOnly|SimulatedOrPhysics|InitialOrOwner|Custom|None" → ELifetimeCondition. */
    static bool ResolveLifetimeCondition(const FString& Spec, ELifetimeCondition& Out)
    {
        if (Spec.Equals(TEXT("None"),                ESearchCase::IgnoreCase)) { Out = COND_None;                return true; }
        if (Spec.Equals(TEXT("InitialOnly"),         ESearchCase::IgnoreCase)) { Out = COND_InitialOnly;         return true; }
        if (Spec.Equals(TEXT("OwnerOnly"),           ESearchCase::IgnoreCase)) { Out = COND_OwnerOnly;           return true; }
        if (Spec.Equals(TEXT("SkipOwner"),           ESearchCase::IgnoreCase)) { Out = COND_SkipOwner;           return true; }
        if (Spec.Equals(TEXT("SimulatedOnly"),       ESearchCase::IgnoreCase)) { Out = COND_SimulatedOnly;       return true; }
        if (Spec.Equals(TEXT("AutonomousOnly"),      ESearchCase::IgnoreCase)) { Out = COND_AutonomousOnly;      return true; }
        if (Spec.Equals(TEXT("SimulatedOrPhysics"), ESearchCase::IgnoreCase))  { Out = COND_SimulatedOrPhysics;  return true; }
        if (Spec.Equals(TEXT("InitialOrOwner"),      ESearchCase::IgnoreCase)) { Out = COND_InitialOrOwner;      return true; }
        if (Spec.Equals(TEXT("Custom"),              ESearchCase::IgnoreCase)) { Out = COND_Custom;              return true; }
        if (Spec.Equals(TEXT("ReplayOrOwner"),       ESearchCase::IgnoreCase)) { Out = COND_ReplayOrOwner;       return true; }
        if (Spec.Equals(TEXT("ReplayOnly"),          ESearchCase::IgnoreCase)) { Out = COND_ReplayOnly;          return true; }
        if (Spec.Equals(TEXT("SimulatedOnlyNoReplay"),       ESearchCase::IgnoreCase)) { Out = COND_SimulatedOnlyNoReplay;       return true; }
        if (Spec.Equals(TEXT("SimulatedOrPhysicsNoReplay"), ESearchCase::IgnoreCase))  { Out = COND_SimulatedOrPhysicsNoReplay;  return true; }
        if (Spec.Equals(TEXT("SkipReplay"),          ESearchCase::IgnoreCase)) { Out = COND_SkipReplay;          return true; }
        if (Spec.Equals(TEXT("Never"),               ESearchCase::IgnoreCase)) { Out = COND_Never;               return true; }
        return false;
    }

    /** Find a function graph by name in Blueprint->FunctionGraphs. */
    static UEdGraph* FindFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
    {
        for (UEdGraph* G : Blueprint->FunctionGraphs)
        {
            if (G && G->GetName() == FunctionName) return G;
        }
        return nullptr;
    }

    /** Find function entry / result on a function graph. */
    static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* FunctionGraph)
    {
        if (!FunctionGraph) return nullptr;
        for (UEdGraphNode* N : FunctionGraph->Nodes)
        {
            if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(N)) return E;
        }
        return nullptr;
    }

}  // namespace AgenticBpAuthor

// ─── HandleBpAddVariables ───────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpAddVariables(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAuthor;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* Vars = nullptr;
    if (!Params->TryGetArrayField(TEXT("variables"), Vars) || !Vars)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variables' array"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    Blueprint->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 ErrorCount = 0;
    Results.Reserve(Vars->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *Vars)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        if (!Entry.IsValid() || Entry->Type != EJson::Object)
        {
            R->SetStringField(TEXT("error"), TEXT("variable entry must be a JSON object"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();
        FString VarName;
        if (!Spec->TryGetStringField(TEXT("name"), VarName))
        {
            R->SetStringField(TEXT("error"), TEXT("variable.name missing"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("name"), VarName);

        const TSharedPtr<FJsonObject>* TypeSpec = nullptr;
        if (!Spec->TryGetObjectField(TEXT("type"), TypeSpec) || !TypeSpec || !TypeSpec->IsValid())
        {
            R->SetStringField(TEXT("error"), TEXT("variable.type missing"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        FEdGraphPinType PinType;
        FString TypeErr;
        if (!BuildPinTypeFromJson(*TypeSpec, PinType, TypeErr))
        {
            R->SetStringField(TEXT("error"), FString::Printf(TEXT("type build failed: %s"), *TypeErr));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        FString DefaultValue;
        Spec->TryGetStringField(TEXT("default_value"), DefaultValue);

        const bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType, DefaultValue);
        if (!bAdded)
        {
            R->SetStringField(TEXT("error"), TEXT("AddMemberVariable failed (duplicate name or invalid type)"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        // Post-creation flag/category/replication/tooltip application.
        FBPVariableDescription* Var = FindMemberVariable(Blueprint, FName(*VarName));
        if (Var)
        {
            FString Category;
            if (Spec->TryGetStringField(TEXT("category"), Category)) Var->Category = FText::FromString(Category);

            bool bVal = false;
            if (Spec->TryGetBoolField(TEXT("instance_editable"), bVal))
            { if (bVal) Var->PropertyFlags |= CPF_Edit;                 else Var->PropertyFlags &= ~CPF_Edit; }
            if (Spec->TryGetBoolField(TEXT("expose_on_spawn"), bVal))
            { if (bVal) Var->PropertyFlags |= CPF_ExposeOnSpawn;        else Var->PropertyFlags &= ~CPF_ExposeOnSpawn; }
            if (Spec->TryGetBoolField(TEXT("private"), bVal))
            { if (bVal) Var->PropertyFlags |= CPF_DisableEditOnInstance; else Var->PropertyFlags &= ~CPF_DisableEditOnInstance; }
            if (Spec->TryGetBoolField(TEXT("blueprint_read_only"), bVal))
            { if (bVal) Var->PropertyFlags |= CPF_BlueprintReadOnly;    else Var->PropertyFlags &= ~CPF_BlueprintReadOnly; }
            if (Spec->TryGetBoolField(TEXT("transient"), bVal))
            { if (bVal) Var->PropertyFlags |= CPF_Transient;           else Var->PropertyFlags &= ~CPF_Transient; }
            if (Spec->TryGetBoolField(TEXT("save_game"), bVal))
            { if (bVal) Var->PropertyFlags |= CPF_SaveGame;            else Var->PropertyFlags &= ~CPF_SaveGame; }
            if (Spec->TryGetBoolField(TEXT("config"), bVal))
            { if (bVal) Var->PropertyFlags |= CPF_Config;              else Var->PropertyFlags &= ~CPF_Config; }

            FString RepCondition;
            if (Spec->TryGetStringField(TEXT("replication_condition"), RepCondition))
            {
                ELifetimeCondition Cond;
                if (ResolveLifetimeCondition(RepCondition, Cond))
                {
                    Var->ReplicationCondition = Cond;
                    if (Cond != COND_None) Var->PropertyFlags |= CPF_Net;
                }
            }
            FString RepNotify;
            if (Spec->TryGetStringField(TEXT("rep_notify_func"), RepNotify))
            {
                Var->RepNotifyFunc = FName(*RepNotify);
                Var->PropertyFlags |= CPF_RepNotify | CPF_Net;
            }

            // Metadata: tooltip, ClampMin, ClampMax, EditCondition, etc.
            const TSharedPtr<FJsonObject>* MetaSpec = nullptr;
            if (Spec->TryGetObjectField(TEXT("metadata"), MetaSpec) && MetaSpec && MetaSpec->IsValid())
            {
                for (const TPair<FString, TSharedPtr<FJsonValue>>& MetaPair : (*MetaSpec)->Values)
                {
                    FString MetaVal;
                    if (MetaPair.Value.IsValid() && MetaPair.Value->TryGetString(MetaVal))
                    {
                        FBlueprintEditorUtils::SetBlueprintVariableMetaData(
                            Blueprint, FName(*VarName), nullptr,
                            FName(*MetaPair.Key), MetaVal);
                    }
                }
            }
        }

        R->SetBoolField(TEXT("added"), true);
        if (Var) R->SetStringField(TEXT("guid"), Var->VarGuid.ToString());
        Results.Add(MakeShared<FJsonValueObject>(R));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),  Path);
    Result->SetArrayField (TEXT("variables"),  Results);
    Result->SetNumberField(TEXT("added_count"),Results.Num() - ErrorCount);
    Result->SetNumberField(TEXT("error_count"),ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpRenameVariable ─────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpRenameVariable(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path, OldName, NewName;
    if (!Params->TryGetStringField(TEXT("path"),     Path))    return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("old_name"), OldName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_name'"));
    if (!Params->TryGetStringField(TEXT("new_name"), NewName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name'"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    Blueprint->Modify();
    FBlueprintEditorUtils::RenameMemberVariable(Blueprint, FName(*OldName), FName(*NewName));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),Path);
    Result->SetStringField(TEXT("old_name"), OldName);
    Result->SetStringField(TEXT("new_name"), NewName);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetVariableDefault ─────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetVariableDefault(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAuthor;

    FString Path, VarName, Value;
    if (!Params->TryGetStringField(TEXT("path"),  Path))    return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("name"),  VarName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
    if (!Params->TryGetStringField(TEXT("value"), Value))   return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value'"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    FBPVariableDescription* Var = FindMemberVariable(Blueprint, FName(*VarName));
    if (!Var)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Variable '%s' not found on Blueprint"), *VarName));
    }

    Blueprint->Modify();
    Var->DefaultValue = Value;
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"), Path);
    Result->SetStringField(TEXT("name"),      VarName);
    Result->SetStringField(TEXT("value"),     Value);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetVariableFlags ───────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetVariableFlags(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAuthor;

    FString Path, VarName;
    if (!Params->TryGetStringField(TEXT("path"), Path))    return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("name"), VarName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    FBPVariableDescription* Var = FindMemberVariable(Blueprint, FName(*VarName));
    if (!Var)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Variable '%s' not found"), *VarName));
    }

    Blueprint->Modify();

    auto SetFlag = [&](const TCHAR* Key, uint64 Bit, bool bUseEditableHelper=false)
    {
        bool bValue = false;
        if (Params->TryGetBoolField(Key, bValue))
        {
            if (bUseEditableHelper)
            {
                FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, FName(*VarName), !bValue);
                return;
            }
            if (bValue) Var->PropertyFlags |= Bit;
            else        Var->PropertyFlags &= ~Bit;
        }
    };
    SetFlag(TEXT("instance_editable"),   CPF_Edit,                    true);
    SetFlag(TEXT("expose_on_spawn"),     CPF_ExposeOnSpawn);
    SetFlag(TEXT("blueprint_read_only"), CPF_BlueprintReadOnly);
    SetFlag(TEXT("transient"),           CPF_Transient);
    SetFlag(TEXT("save_game"),           CPF_SaveGame);
    SetFlag(TEXT("config"),              CPF_Config);

    FString Category;
    if (Params->TryGetStringField(TEXT("category"), Category))
    {
        Var->Category = FText::FromString(Category);
    }

    FString Tooltip;
    if (Params->TryGetStringField(TEXT("tooltip"), Tooltip))
    {
        FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*VarName), nullptr,
            FName(TEXT("Tooltip")), Tooltip);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"), Path);
    Result->SetStringField(TEXT("name"),      VarName);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpAddFunction ────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpAddFunction(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAuthor;

    FString Path, FunctionName;
    if (!Params->TryGetStringField(TEXT("path"), Path))         return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("name"), FunctionName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' (function name)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    if (FindFunctionGraph(Blueprint, FunctionName))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function '%s' already exists"), *FunctionName));
    }

    Blueprint->Modify();

    // anim_layer=true creates a self-implemented anim layer function — matches Epic's
    // FBlueprintEditor::NewDocument_OnClicked(CGT_NewAnimationLayer) flow
    // (BlueprintEditor.cpp:9714-9718): UAnimationGraph + UAnimationGraphSchema, added
    // via AddDomainSpecificGraph (NOT AddFunctionGraph — that injects K2 FunctionEntry/
    // Result terminals incompatible with pose-flow layers). The schema's
    // CreateDefaultNodesForGraph spawns the Root; we then add a LinkedInputPose
    // manually since Epic only auto-creates LinkedInputPose for interface-implemented
    // layers (AnimationGraphSchema.cpp:462-475), not self-implemented ones.
    bool bAnimLayer = false;
    Params->TryGetBoolField(TEXT("anim_layer"), bAnimLayer);
    if (bAnimLayer && !Blueprint->IsA<UAnimBlueprint>())
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            TEXT("anim_layer=true requires the Blueprint to be a UAnimBlueprint"));
    }

    UClass* GraphClass  = bAnimLayer ? UAnimationGraph::StaticClass()       : UEdGraph::StaticClass();
    UClass* SchemaClass = bAnimLayer ? UAnimationGraphSchema::StaticClass() : UEdGraphSchema_K2::StaticClass();

    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint, FName(*FunctionName),
        GraphClass, SchemaClass);
    if (!NewGraph)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("CreateNewGraph returned null"));
    }
    if (bAnimLayer)
    {
        FBlueprintEditorUtils::AddDomainSpecificGraph(Blueprint, NewGraph);

        // Self-implemented anim layer: schema added the Root, now add a LinkedInputPose
        // named "<FunctionName>InputPose" — Epic's convention for layer input naming
        // (matches CR_ZenBlinkLayer.InputPose, etc.). The LinkedInputPose carries the
        // layer's function-reference + input-pose-index metadata so the AnimBP compiler
        // wires this layer's caller-side pose pin to it.
        FGraphNodeCreator<UAnimGraphNode_LinkedInputPose> InputCreator(*NewGraph);
        UAnimGraphNode_LinkedInputPose* InputPose = InputCreator.CreateNode();
        InputPose->Node.Name = FName(*(FunctionName + TEXT("InputPose")));
        InputPose->InputPoseIndex = 0;
        InputPose->FunctionReference.SetSelfMember(FName(*FunctionName));
        InputPose->NodePosX = -400;
        InputPose->NodePosY = 0;
        InputCreator.Finalize();
        InputPose->ReconstructNode();
    }
    else
    {
        FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, /*bIsUserCreated=*/true, /*SignatureFromObject=*/nullptr);
    }

    UK2Node_FunctionEntry* Entry = FindFunctionEntry(NewGraph);
    UK2Node_FunctionResult* ResultNode = nullptr;

    // Apply pure flag if requested.
    bool bPure = false;
    if (Params->TryGetBoolField(TEXT("pure"), bPure) && bPure && Entry)
    {
        Entry->AddExtraFlags(FUNC_BlueprintPure);
    }

    // Apply access modifier (default Public; AddFunctionGraph already sets FUNC_Public).
    FString Access;
    if (Params->TryGetStringField(TEXT("access"), Access) && Entry)
    {
        Entry->ClearExtraFlags(FUNC_Public | FUNC_Protected | FUNC_Private);
        if      (Access.Equals(TEXT("Protected"), ESearchCase::IgnoreCase)) Entry->AddExtraFlags(FUNC_Protected);
        else if (Access.Equals(TEXT("Private"),   ESearchCase::IgnoreCase)) Entry->AddExtraFlags(FUNC_Private);
        else                                                                Entry->AddExtraFlags(FUNC_Public);
    }

    // Inputs go on FunctionEntry (its outputs become the function's input pins from the caller's view).
    int32 InputsAdded = 0;
    TArray<TSharedPtr<FJsonValue>> InputErrors;
    const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
    if (Params->TryGetArrayField(TEXT("inputs"), Inputs) && Inputs && Entry)
    {
        for (int32 Idx = 0; Idx < Inputs->Num(); ++Idx)
        {
            const TSharedPtr<FJsonValue>& V = (*Inputs)[Idx];
            if (!V.IsValid() || V->Type != EJson::Object)
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("error"), TEXT("Invalid entry (expected object)"));
                InputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            const TSharedPtr<FJsonObject>& P = V->AsObject();
            FString PName;
            if (!P->TryGetStringField(TEXT("name"), PName))
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("error"), TEXT("Missing 'name'"));
                InputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            const TSharedPtr<FJsonObject>* TypeSpec2 = nullptr;
            if (!P->TryGetObjectField(TEXT("type"), TypeSpec2) || !TypeSpec2)
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("name"), PName);
                E->SetStringField(TEXT("error"), TEXT("Missing 'type'"));
                InputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            FEdGraphPinType T;
            FString TErr;
            if (!BuildPinTypeFromJson(*TypeSpec2, T, TErr))
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("name"), PName);
                E->SetStringField(TEXT("error"), FString::Printf(TEXT("Invalid type: %s"), *TErr));
                InputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            if (Entry->CreateUserDefinedPin(FName(*PName), T, EGPD_Output, /*bUseUniqueName=*/true))
            {
                ++InputsAdded;
            }
        }
    }

    // Outputs go on a FunctionResult node — created on demand.
    int32 OutputsAdded = 0;
    TArray<TSharedPtr<FJsonValue>> OutputErrors;
    const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
    if (Params->TryGetArrayField(TEXT("outputs"), Outputs) && Outputs && Outputs->Num() > 0 && Entry)
    {
        ResultNode = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
        for (int32 Idx = 0; Idx < Outputs->Num(); ++Idx)
        {
            const TSharedPtr<FJsonValue>& V = (*Outputs)[Idx];
            if (!V.IsValid() || V->Type != EJson::Object)
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("error"), TEXT("Invalid entry (expected object)"));
                OutputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            const TSharedPtr<FJsonObject>& P = V->AsObject();
            FString PName;
            if (!P->TryGetStringField(TEXT("name"), PName))
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("error"), TEXT("Missing 'name'"));
                OutputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            const TSharedPtr<FJsonObject>* TypeSpec2 = nullptr;
            if (!P->TryGetObjectField(TEXT("type"), TypeSpec2) || !TypeSpec2)
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("name"), PName);
                E->SetStringField(TEXT("error"), TEXT("Missing 'type'"));
                OutputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            FEdGraphPinType T;
            FString TErr;
            if (!BuildPinTypeFromJson(*TypeSpec2, T, TErr))
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetNumberField(TEXT("index"), Idx);
                E->SetStringField(TEXT("name"), PName);
                E->SetStringField(TEXT("error"), FString::Printf(TEXT("Invalid type: %s"), *TErr));
                OutputErrors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            if (ResultNode && ResultNode->CreateUserDefinedPin(FName(*PName), T, EGPD_Input, /*bUseUniqueName=*/true))
            {
                ++OutputsAdded;
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),    Path);
    Result->SetStringField(TEXT("function"),     FunctionName);
    Result->SetStringField(TEXT("graph"),        NewGraph->GetName());
    Result->SetStringField(TEXT("entry"),        Entry ? Entry->GetName() : TEXT(""));
    Result->SetStringField(TEXT("result"),       ResultNode ? ResultNode->GetName() : TEXT(""));
    Result->SetNumberField(TEXT("inputs_added"), InputsAdded);
    Result->SetNumberField(TEXT("outputs_added"),OutputsAdded);
    if (InputErrors.Num() > 0)  Result->SetArrayField(TEXT("input_errors"), InputErrors);
    if (OutputErrors.Num() > 0) Result->SetArrayField(TEXT("output_errors"), OutputErrors);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetFunctionAccess ──────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetFunctionAccess(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAuthor;

    FString Path, FunctionName, Access;
    if (!Params->TryGetStringField(TEXT("path"),     Path))         return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("function"), FunctionName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function'"));
    if (!Params->TryGetStringField(TEXT("access"),   Access))       return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'access' (Public|Protected|Private)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* G = FindFunctionGraph(Blueprint, FunctionName);
    UK2Node_FunctionEntry* Entry = FindFunctionEntry(G);
    if (!Entry)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }

    Blueprint->Modify();
    Entry->Modify();
    Entry->ClearExtraFlags(FUNC_Public | FUNC_Protected | FUNC_Private);
    if      (Access.Equals(TEXT("Protected"), ESearchCase::IgnoreCase)) Entry->AddExtraFlags(FUNC_Protected);
    else if (Access.Equals(TEXT("Private"),   ESearchCase::IgnoreCase)) Entry->AddExtraFlags(FUNC_Private);
    else                                                                Entry->AddExtraFlags(FUNC_Public);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),Path);
    Result->SetStringField(TEXT("function"), FunctionName);
    Result->SetStringField(TEXT("access"),   Access);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetFunctionPure ────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetFunctionPure(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAuthor;

    FString Path, FunctionName;
    bool bPure = false;
    if (!Params->TryGetStringField(TEXT("path"),     Path))         return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("function"), FunctionName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function'"));
    if (!Params->TryGetBoolField  (TEXT("pure"),     bPure))        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pure' (bool)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* G = FindFunctionGraph(Blueprint, FunctionName);
    UK2Node_FunctionEntry* Entry = FindFunctionEntry(G);
    if (!Entry)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    Blueprint->Modify();
    Entry->Modify();
    if (bPure) Entry->AddExtraFlags  (FUNC_BlueprintPure);
    else       Entry->ClearExtraFlags(FUNC_BlueprintPure);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),Path);
    Result->SetStringField(TEXT("function"), FunctionName);
    Result->SetBoolField  (TEXT("pure"),     bPure);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetFunctionReplication ─────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetFunctionReplication(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAuthor;

    FString Path, FunctionName, RPCType;
    if (!Params->TryGetStringField(TEXT("path"),     Path))         return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("function"), FunctionName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function'"));
    if (!Params->TryGetStringField(TEXT("type"),     RPCType))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' (None|Server|Client|Multicast)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UEdGraph* G = FindFunctionGraph(Blueprint, FunctionName);
    UK2Node_FunctionEntry* Entry = FindFunctionEntry(G);
    if (!Entry)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Function '%s' not found"), *FunctionName));
    }
    Blueprint->Modify();
    Entry->Modify();
    Entry->ClearExtraFlags(FUNC_NetServer | FUNC_NetClient | FUNC_NetMulticast | FUNC_NetReliable | FUNC_NetValidate);

    if      (RPCType.Equals(TEXT("Server"),    ESearchCase::IgnoreCase)) Entry->AddExtraFlags(FUNC_NetServer);
    else if (RPCType.Equals(TEXT("Client"),    ESearchCase::IgnoreCase)) Entry->AddExtraFlags(FUNC_NetClient);
    else if (RPCType.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase)) Entry->AddExtraFlags(FUNC_NetMulticast);
    else if (!RPCType.Equals(TEXT("None"),     ESearchCase::IgnoreCase))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown RPC type: %s"), *RPCType));
    }

    bool bReliable = false, bValidation = false;
    if (Params->TryGetBoolField(TEXT("reliable"),       bReliable)   && bReliable)   Entry->AddExtraFlags(FUNC_NetReliable);
    if (Params->TryGetBoolField(TEXT("with_validation"),bValidation) && bValidation) Entry->AddExtraFlags(FUNC_NetValidate);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),       Path);
    Result->SetStringField(TEXT("function"),        FunctionName);
    Result->SetStringField(TEXT("type"),            RPCType);
    Result->SetBoolField  (TEXT("reliable"),        bReliable);
    Result->SetBoolField  (TEXT("with_validation"), bValidation);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpImplementInterface ─────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpImplementInterface(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;  // ResolveClassByName

    FString Path, IfaceSpec;
    if (!Params->TryGetStringField(TEXT("path"),      Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("interface"), IfaceSpec)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface' (class path or short name)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    UClass* IfaceClass = ResolveClassByName(IfaceSpec);
    if (!IfaceClass)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Interface class not found: %s"), *IfaceSpec));
    }
    if (!IfaceClass->HasAnyClassFlags(CLASS_Interface))
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Class '%s' is not an interface"), *IfaceSpec));
    }

    Blueprint->Modify();
    const bool bAdded = FBlueprintEditorUtils::ImplementNewInterface(Blueprint, IfaceClass->GetClassPathName());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"), Path);
    Result->SetStringField(TEXT("interface"), IfaceClass->GetPathName());
    Result->SetBoolField  (TEXT("added"),     bAdded);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpRemoveInterface ────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpRemoveInterface(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;

    FString Path, IfaceSpec;
    if (!Params->TryGetStringField(TEXT("path"),      Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("interface"), IfaceSpec)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface'"));

    bool bPreserveFunctions = false;
    Params->TryGetBoolField(TEXT("preserve_functions"), bPreserveFunctions);

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;
    UClass* IfaceClass = ResolveClassByName(IfaceSpec);
    if (!IfaceClass)
    {
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Interface class not found: %s"), *IfaceSpec));
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::RemoveInterface(Blueprint, IfaceClass->GetClassPathName(), bPreserveFunctions);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),         Path);
    Result->SetStringField(TEXT("interface"),         IfaceClass->GetPathName());
    Result->SetBoolField  (TEXT("preserve_functions"),bPreserveFunctions);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// BP-Expert Phase 4 — SCS / Components
// =============================================================================

namespace AgenticBpComponents
{
    /** Find an SCS node by its VariableName (case-sensitive). Returns nullptr if missing. */
    static USCS_Node* FindSCSNodeByName(USimpleConstructionScript* SCS, const FString& InName)
    {
        if (!SCS) return nullptr;
        const FName Target(*InName);
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (Node && Node->GetVariableName() == Target) return Node;
        }
        return nullptr;
    }

    /** Map "Static"/"Stationary"/"Movable" → EComponentMobility. Returns false on bad name. */
    static bool ResolveMobility(const FString& InSpec, EComponentMobility::Type& OutMobility)
    {
        const FString S = InSpec.TrimStartAndEnd();
        if (S.Equals(TEXT("Static"),     ESearchCase::IgnoreCase)) { OutMobility = EComponentMobility::Static;     return true; }
        if (S.Equals(TEXT("Stationary"), ESearchCase::IgnoreCase)) { OutMobility = EComponentMobility::Stationary; return true; }
        if (S.Equals(TEXT("Movable"),    ESearchCase::IgnoreCase)) { OutMobility = EComponentMobility::Movable;    return true; }
        return false;
    }

    /** Map "NoCollision"/"QueryOnly"/"PhysicsOnly"/"QueryAndPhysics"/"ProbeOnly"/"QueryAndProbe" → ECollisionEnabled::Type. */
    static bool ResolveCollisionEnabled(const FString& InSpec, ECollisionEnabled::Type& OutEnabled)
    {
        const FString S = InSpec.TrimStartAndEnd();
        if (S.Equals(TEXT("NoCollision"),      ESearchCase::IgnoreCase)) { OutEnabled = ECollisionEnabled::NoCollision;      return true; }
        if (S.Equals(TEXT("QueryOnly"),        ESearchCase::IgnoreCase)) { OutEnabled = ECollisionEnabled::QueryOnly;        return true; }
        if (S.Equals(TEXT("PhysicsOnly"),      ESearchCase::IgnoreCase)) { OutEnabled = ECollisionEnabled::PhysicsOnly;      return true; }
        if (S.Equals(TEXT("QueryAndPhysics"),  ESearchCase::IgnoreCase)) { OutEnabled = ECollisionEnabled::QueryAndPhysics;  return true; }
        if (S.Equals(TEXT("ProbeOnly"),        ESearchCase::IgnoreCase)) { OutEnabled = ECollisionEnabled::ProbeOnly;        return true; }
        if (S.Equals(TEXT("QueryAndProbe"),    ESearchCase::IgnoreCase)) { OutEnabled = ECollisionEnabled::QueryAndProbe;    return true; }
        return false;
    }

    /** Map "Block"/"Overlap"/"Ignore" (any case) → ECollisionResponse. */
    static bool ResolveCollisionResponse(const FString& InSpec, ECollisionResponse& OutResponse)
    {
        const FString S = InSpec.TrimStartAndEnd();
        if (S.Equals(TEXT("Ignore"),  ESearchCase::IgnoreCase)) { OutResponse = ECR_Ignore;  return true; }
        if (S.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase)) { OutResponse = ECR_Overlap; return true; }
        if (S.Equals(TEXT("Block"),   ESearchCase::IgnoreCase)) { OutResponse = ECR_Block;   return true; }
        return false;
    }

    /** Map "WorldStatic"/"WorldDynamic"/"Pawn"/.../"Vehicle"/custom-name → ECollisionChannel.
     *  Accepts engine enum names ("ECC_WorldStatic"), short forms ("WorldStatic"), and project
     *  channel display names from DefaultEngine.ini (e.g. "Target", "Camera"). */
    static bool ResolveCollisionChannel(const FString& InSpec, ECollisionChannel& OutChannel)
    {
        FString S = InSpec.TrimStartAndEnd();
        if (S.StartsWith(TEXT("ECC_")))
        {
            S = S.RightChop(4);
        }

        // Engine fixed channels.
        if (S.Equals(TEXT("WorldStatic"),       ESearchCase::IgnoreCase)) { OutChannel = ECC_WorldStatic;       return true; }
        if (S.Equals(TEXT("WorldDynamic"),      ESearchCase::IgnoreCase)) { OutChannel = ECC_WorldDynamic;      return true; }
        if (S.Equals(TEXT("Pawn"),              ESearchCase::IgnoreCase)) { OutChannel = ECC_Pawn;              return true; }
        if (S.Equals(TEXT("Visibility"),        ESearchCase::IgnoreCase)) { OutChannel = ECC_Visibility;        return true; }
        if (S.Equals(TEXT("Camera"),            ESearchCase::IgnoreCase)) { OutChannel = ECC_Camera;            return true; }
        if (S.Equals(TEXT("PhysicsBody"),       ESearchCase::IgnoreCase)) { OutChannel = ECC_PhysicsBody;       return true; }
        if (S.Equals(TEXT("Vehicle"),           ESearchCase::IgnoreCase)) { OutChannel = ECC_Vehicle;           return true; }
        if (S.Equals(TEXT("Destructible"),      ESearchCase::IgnoreCase)) { OutChannel = ECC_Destructible;      return true; }

        // Custom project channels via DefaultEngine.ini display names.
        if (UCollisionProfile* Profile = UCollisionProfile::Get())
        {
            for (int32 Idx = 0; Idx < ECC_MAX; ++Idx)
            {
                const FName DisplayName = Profile->ReturnChannelNameFromContainerIndex(Idx);
                if (DisplayName != NAME_None && DisplayName.ToString().Equals(S, ESearchCase::IgnoreCase))
                {
                    OutChannel = static_cast<ECollisionChannel>(Idx);
                    return true;
                }
            }
        }

        return false;
    }

    /** Format an FVector as {x,y,z} JSON. */
    static TSharedPtr<FJsonObject> VectorToJson(const FVector& V)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("x"), V.X);
        O->SetNumberField(TEXT("y"), V.Y);
        O->SetNumberField(TEXT("z"), V.Z);
        return O;
    }
    /** Format an FRotator as {pitch,yaw,roll} JSON. */
    static TSharedPtr<FJsonObject> RotatorToJson(const FRotator& R)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("pitch"), R.Pitch);
        O->SetNumberField(TEXT("yaw"),   R.Yaw);
        O->SetNumberField(TEXT("roll"),  R.Roll);
        return O;
    }
    /** EComponentMobility → string. */
    static const TCHAR* MobilityToString(EComponentMobility::Type M)
    {
        switch (M)
        {
            case EComponentMobility::Static:     return TEXT("Static");
            case EComponentMobility::Stationary: return TEXT("Stationary");
            case EComponentMobility::Movable:    return TEXT("Movable");
            default:                             return TEXT("Unknown");
        }
    }
    /** ECollisionEnabled → string. */
    static const TCHAR* CollisionEnabledToString(ECollisionEnabled::Type E)
    {
        switch (E)
        {
            case ECollisionEnabled::NoCollision:     return TEXT("NoCollision");
            case ECollisionEnabled::QueryOnly:       return TEXT("QueryOnly");
            case ECollisionEnabled::PhysicsOnly:     return TEXT("PhysicsOnly");
            case ECollisionEnabled::QueryAndPhysics: return TEXT("QueryAndPhysics");
            case ECollisionEnabled::ProbeOnly:       return TEXT("ProbeOnly");
            case ECollisionEnabled::QueryAndProbe:   return TEXT("QueryAndProbe");
            default:                                 return TEXT("Unknown");
        }
    }
    /** ECollisionResponse → string. */
    static const TCHAR* CollisionResponseToString(ECollisionResponse R)
    {
        switch (R)
        {
            case ECR_Ignore:  return TEXT("Ignore");
            case ECR_Overlap: return TEXT("Overlap");
            case ECR_Block:   return TEXT("Block");
            default:          return TEXT("Unknown");
        }
    }
}

// ─── HandleBpAddComponents ──────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpAddComponents(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;       // ResolveClassByName
    using namespace AgenticBpComponents;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* Comps = nullptr;
    if (!Params->TryGetArrayField(TEXT("components"), Comps) || !Comps)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'components' array"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript (only Actor-derived BPs have SCS)"));

    Blueprint->Modify();
    SCS->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 AddedCount = 0;
    int32 ErrorCount = 0;
    Results.Reserve(Comps->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *Comps)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        if (!Entry.IsValid() || Entry->Type != EJson::Object)
        {
            R->SetStringField(TEXT("error"), TEXT("component entry must be a JSON object"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();

        FString CompName, ClassSpec;
        if (!Spec->TryGetStringField(TEXT("name"), CompName))
        {
            R->SetStringField(TEXT("error"), TEXT("component.name missing"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("name"), CompName);

        if (!Spec->TryGetStringField(TEXT("class"), ClassSpec))
        {
            R->SetStringField(TEXT("error"), TEXT("component.class missing"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        UClass* CompClass = ResolveClassByName(ClassSpec);
        if (!CompClass)
        {
            R->SetStringField(TEXT("error"), FString::Printf(TEXT("class not found: %s"), *ClassSpec));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        if (!CompClass->IsChildOf(UActorComponent::StaticClass()))
        {
            R->SetStringField(TEXT("error"), FString::Printf(TEXT("'%s' is not a UActorComponent subclass"), *ClassSpec));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        if (FindSCSNodeByName(SCS, CompName))
        {
            R->SetStringField(TEXT("error"), TEXT("component with this name already exists"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        USCS_Node* NewNode = SCS->CreateNode(CompClass, FName(*CompName));
        if (!NewNode)
        {
            R->SetStringField(TEXT("error"), TEXT("SCS->CreateNode returned null (invalid name or class)"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        // Attach: explicit parent name (USceneComponent only) → AddChildNode; else → AddNode (root).
        FString ParentName;
        const bool bHasParent = Spec->TryGetStringField(TEXT("parent"), ParentName) && !ParentName.IsEmpty();
        USCS_Node* ParentNode = bHasParent ? FindSCSNodeByName(SCS, ParentName) : nullptr;

        if (bHasParent)
        {
            if (!ParentNode)
            {
                SCS->RemoveNode(NewNode);
                R->SetStringField(TEXT("error"), FString::Printf(TEXT("parent SCS node not found: %s"), *ParentName));
                ++ErrorCount;
                Results.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }
            ParentNode->AddChildNode(NewNode);
        }
        else
        {
            SCS->AddNode(NewNode);
        }

        // Optional attach socket (USceneComponent children only).
        FString AttachSocket;
        if (Spec->TryGetStringField(TEXT("attach_socket"), AttachSocket) && !AttachSocket.IsEmpty())
        {
            NewNode->AttachToName = FName(*AttachSocket);
        }

        // Optional relative transform on USceneComponent templates.
        if (USceneComponent* SceneTpl = Cast<USceneComponent>(NewNode->ComponentTemplate))
        {
            if (Spec->HasField(TEXT("location")))
                SceneTpl->SetRelativeLocation(FAgenticMCPCommonUtils::GetVectorFromJson(Spec, TEXT("location")));
            if (Spec->HasField(TEXT("rotation")))
                SceneTpl->SetRelativeRotation(FAgenticMCPCommonUtils::GetRotatorFromJson(Spec, TEXT("rotation")));
            if (Spec->HasField(TEXT("scale")))
                SceneTpl->SetRelativeScale3D(FAgenticMCPCommonUtils::GetVectorFromJson(Spec, TEXT("scale")));
        }

        R->SetStringField(TEXT("class"), CompClass->GetPathName());
        R->SetStringField(TEXT("guid"),  NewNode->VariableGuid.ToString());
        R->SetBoolField  (TEXT("is_root"), SCS->GetRootNodes().Contains(NewNode));
        if (ParentNode) R->SetStringField(TEXT("parent"), ParentNode->GetVariableName().ToString());
        R->SetBoolField  (TEXT("added"), true);
        Results.Add(MakeShared<FJsonValueObject>(R));
        ++AddedCount;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),   Path);
    Result->SetArrayField (TEXT("components"),  Results);
    Result->SetNumberField(TEXT("added_count"), AddedCount);
    Result->SetNumberField(TEXT("error_count"), ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpRemoveComponents ───────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpRemoveComponents(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Params->TryGetArrayField(TEXT("components"), Items) || !Items)
    {
        // Legacy convenience: accept a flat names[] array of strings.
        if (!Params->TryGetArrayField(TEXT("names"), Items) || !Items)
            return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'components' array (entries: { name, cascade?, promote_to_root? }) or 'names' array of strings"));
    }

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    Blueprint->Modify();
    SCS->Modify();

    TArray<TSharedPtr<FJsonValue>> Results;
    int32 RemovedCount = 0;
    int32 ErrorCount = 0;
    Results.Reserve(Items->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *Items)
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();

        FString NodeName;
        bool bCascade = false;
        FString PromoteName;
        if (Entry.IsValid() && Entry->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject>& Spec = Entry->AsObject();
            if (!Spec->TryGetStringField(TEXT("name"), NodeName))
            {
                R->SetStringField(TEXT("error"), TEXT("component.name missing"));
                ++ErrorCount;
                Results.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }
            Spec->TryGetBoolField  (TEXT("cascade"),          bCascade);
            Spec->TryGetStringField(TEXT("promote_to_root"),  PromoteName);
        }
        else if (Entry.IsValid() && Entry->TryGetString(NodeName))
        {
            // string-only entry — pure removal, no flags
        }
        else
        {
            R->SetStringField(TEXT("error"), TEXT("component entry must be a string or object with .name"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        R->SetStringField(TEXT("name"), NodeName);

        USCS_Node* Node = FindSCSNodeByName(SCS, NodeName);
        if (!Node)
        {
            R->SetStringField(TEXT("error"), TEXT("component not found"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        const bool bIsRoot = SCS->GetRootNodes().Contains(Node);
        const bool bHasChildren = Node->GetChildNodes().Num() > 0;

        if (bHasChildren && !bCascade)
        {
            R->SetStringField(TEXT("error"),
                TEXT("component has children — pass cascade=true to remove descendants, or detach them first via bp_attach_component"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        // Scene-root invariant: removing the root scene component requires promotion.
        const bool bIsSceneRoot = bIsRoot && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>();
        if (bIsSceneRoot && !PromoteName.IsEmpty())
        {
            USCS_Node* NewRoot = FindSCSNodeByName(SCS, PromoteName);
            if (!NewRoot)
            {
                R->SetStringField(TEXT("error"),
                    FString::Printf(TEXT("promote_to_root='%s' not found"), *PromoteName));
                ++ErrorCount;
                Results.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }
            if (!NewRoot->ComponentTemplate || !NewRoot->ComponentTemplate->IsA<USceneComponent>())
            {
                R->SetStringField(TEXT("error"),
                    TEXT("promote_to_root must be a USceneComponent so it can replace the scene root"));
                ++ErrorCount;
                Results.Add(MakeShared<FJsonValueObject>(R));
                continue;
            }
            // Detach the new root from any current parent so it becomes a root candidate.
            // bRemoveFromAllNodes=true so the subsequent SCS->AddNode can re-add cleanly
            // without duplicating in AllNodes.
            if (USCS_Node* OldParent = SCS->FindParentNode(NewRoot))
            {
                OldParent->RemoveChildNode(NewRoot, /*bRemoveFromAllNodes=*/true);
            }
            SCS->AddNode(NewRoot);  // re-adds to RootNodes + AllNodes
        }
        else if (bIsSceneRoot)
        {
            R->SetStringField(TEXT("error"),
                TEXT("cannot remove scene root — pass promote_to_root=<other-scene-component-name>"));
            ++ErrorCount;
            Results.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }

        // Cascade descendants if requested. UE's RemoveNode does NOT remove descendants
        // — they'd be left orphaned in AllNodes — so we walk the subtree manually.
        TArray<USCS_Node*> ToRemove;
        if (bCascade)
        {
            TArray<USCS_Node*> Stack;
            Stack.Push(Node);
            while (Stack.Num() > 0)
            {
                USCS_Node* Cur = Stack.Pop();
                for (USCS_Node* Child : Cur->GetChildNodes())
                {
                    if (Child) Stack.Push(Child);
                }
                ToRemove.Add(Cur);
            }
            // Reverse so deepest descendants are removed first.
            Algo::Reverse(ToRemove);
        }
        else
        {
            ToRemove.Add(Node);
        }

        int32 CascadeCount = 0;
        for (USCS_Node* Cur : ToRemove)
        {
            if (!Cur) continue;
            // For children: explicit remove strips ChildNodes + AllNodes so the SCS->RemoveNode
            // below is a no-op for non-roots. For roots: skip this branch and let SCS->RemoveNode
            // handle the RootNodes + AllNodes strip.
            if (USCS_Node* Parent = SCS->FindParentNode(Cur))
            {
                Parent->RemoveChildNode(Cur, /*bRemoveFromAllNodes=*/true);
            }
            SCS->RemoveNode(Cur, /*bValidateSceneRootNodes=*/false);
            ++CascadeCount;
        }

        R->SetBoolField  (TEXT("removed"),       true);
        R->SetNumberField(TEXT("cascade_count"), CascadeCount);
        Results.Add(MakeShared<FJsonValueObject>(R));
        ++RemovedCount;
    }

    SCS->ValidateSceneRootNodes();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),    Path);
    Result->SetArrayField (TEXT("components"),   Results);
    Result->SetNumberField(TEXT("removed_count"),RemovedCount);
    Result->SetNumberField(TEXT("error_count"),  ErrorCount);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpAttachComponent ────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpAttachComponent(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path, ChildName, ParentName;
    if (!Params->TryGetStringField(TEXT("path"),  Path))      return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("child"), ChildName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'child' (component name)"));
    // 'parent' may be empty/missing → promote child to root.
    Params->TryGetStringField(TEXT("parent"), ParentName);

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    USCS_Node* Child = FindSCSNodeByName(SCS, ChildName);
    if (!Child)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Child component not found: %s"), *ChildName));

    USCS_Node* NewParent = nullptr;
    if (!ParentName.IsEmpty())
    {
        NewParent = FindSCSNodeByName(SCS, ParentName);
        if (!NewParent)
            return FAgenticMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Parent component not found: %s"), *ParentName));
        if (NewParent == Child)
            return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Cannot parent a component to itself"));
        // Cycle check: walk up NewParent's ancestor chain looking for Child.
        for (USCS_Node* P = SCS->FindParentNode(NewParent); P != nullptr; P = SCS->FindParentNode(P))
        {
            if (P == Child)
                return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Cycle: new parent is a descendant of the child"));
        }
    }

    Blueprint->Modify();
    SCS->Modify();

    // Detach + re-attach using the canonical engine pattern. Key invariant: every node must
    // appear exactly once in SCS->AllNodes. RemoveChildNode + AddChildNode round-trip AllNodes
    // (when bRemoveFromAllNodes=true on the strip side); SCS->RemoveNode + SCS->AddNode does
    // the same for root nodes. Reference: Engine BlueprintEditorTests.cpp re-root sequence.
    USCS_Node* CurParent = SCS->FindParentNode(Child);
    const FString OldParentName = CurParent ? CurParent->GetVariableName().ToString() : TEXT("<root>");

    if (CurParent)
    {
        CurParent->RemoveChildNode(Child, /*bRemoveFromAllNodes=*/true);
    }
    else
    {
        // Child is currently a root. Strip from RootNodes + AllNodes so the destination call re-adds cleanly.
        SCS->RemoveNode(Child, /*bValidateSceneRootNodes=*/false);
    }

    if (NewParent)
    {
        NewParent->AddChildNode(Child);  // adds to ChildNodes + AllNodes
    }
    else
    {
        SCS->AddNode(Child);              // adds to RootNodes + AllNodes
    }

    // Optional attach socket update.
    FString AttachSocket;
    const bool bHasSocket = Params->TryGetStringField(TEXT("attach_socket"), AttachSocket);
    if (bHasSocket)
    {
        Child->AttachToName = FName(*AttachSocket);
    }

    SCS->ValidateSceneRootNodes();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),  Path);
    Result->SetStringField(TEXT("child"),      ChildName);
    Result->SetStringField(TEXT("old_parent"), OldParentName);
    Result->SetStringField(TEXT("new_parent"), NewParent ? NewParent->GetVariableName().ToString() : TEXT("<root>"));
    if (bHasSocket) Result->SetStringField(TEXT("attach_socket"), AttachSocket);
    Result->SetBoolField  (TEXT("is_root"),    SCS->GetRootNodes().Contains(Child));
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpRenameComponent ────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpRenameComponent(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path, OldName, NewName;
    if (!Params->TryGetStringField(TEXT("path"),     Path))    return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("old_name"), OldName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_name'"));
    if (!Params->TryGetStringField(TEXT("new_name"), NewName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name'"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    USCS_Node* Node = FindSCSNodeByName(SCS, OldName);
    if (!Node)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Component not found: %s"), *OldName));
    if (FindSCSNodeByName(SCS, NewName))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("A component named '%s' already exists"), *NewName));

    Blueprint->Modify();
    Node->Modify();

    FBlueprintEditorUtils::RenameComponentMemberVariable(Blueprint, Node, FName(*NewName));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"), Path);
    Result->SetStringField(TEXT("old_name"),  OldName);
    Result->SetStringField(TEXT("new_name"),  Node->GetVariableName().ToString());
    Result->SetStringField(TEXT("guid"),      Node->VariableGuid.ToString());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpGetComponentDetails ────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpGetComponentDetails(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path, CompName;
    if (!Params->TryGetStringField(TEXT("path"),      Path))     return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("component"), CompName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component' (name)"));

    bool bIncludeProperties = true;
    Params->TryGetBoolField(TEXT("include_properties"), bIncludeProperties);

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    USCS_Node* Node = FindSCSNodeByName(SCS, CompName);
    if (!Node)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Component not found: %s"), *CompName));

    UActorComponent* Tpl = Node->ComponentTemplate;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),    Path);
    Result->SetStringField(TEXT("name"),         CompName);
    Result->SetStringField(TEXT("guid"),         Node->VariableGuid.ToString());
    Result->SetStringField(TEXT("class"),
        Node->ComponentClass ? Node->ComponentClass->GetPathName() : TEXT("None"));
    Result->SetBoolField  (TEXT("is_root"),      SCS->GetRootNodes().Contains(Node));
    if (USCS_Node* Parent = SCS->FindParentNode(Node))
    {
        Result->SetStringField(TEXT("parent"),   Parent->GetVariableName().ToString());
    }
    Result->SetStringField(TEXT("attach_socket"),Node->AttachToName.ToString());

    // USceneComponent details — transform + mobility.
    if (USceneComponent* SceneTpl = Cast<USceneComponent>(Tpl))
    {
        TSharedPtr<FJsonObject> Xform = MakeShared<FJsonObject>();
        Xform->SetObjectField(TEXT("location"), VectorToJson(SceneTpl->GetRelativeLocation()));
        Xform->SetObjectField(TEXT("rotation"), RotatorToJson(SceneTpl->GetRelativeRotation()));
        Xform->SetObjectField(TEXT("scale"),    VectorToJson(SceneTpl->GetRelativeScale3D()));
        Result->SetObjectField(TEXT("transform"), Xform);
        Result->SetStringField(TEXT("mobility"),  MobilityToString(SceneTpl->Mobility));
    }

    // UPrimitiveComponent details — collision profile + enabled + responses.
    if (UPrimitiveComponent* PrimTpl = Cast<UPrimitiveComponent>(Tpl))
    {
        const FBodyInstance& Body = PrimTpl->BodyInstance;
        TSharedPtr<FJsonObject> Coll = MakeShared<FJsonObject>();
        Coll->SetStringField(TEXT("profile"), Body.GetCollisionProfileName().ToString());
        Coll->SetStringField(TEXT("enabled"), CollisionEnabledToString(Body.GetCollisionEnabled()));

        // Per-channel responses for the 8 engine fixed channels.
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        const FCollisionResponseContainer& Container = Body.GetResponseToChannels();
        static const TPair<const TCHAR*, ECollisionChannel> Fixed[] = {
            {TEXT("WorldStatic"),  ECC_WorldStatic},
            {TEXT("WorldDynamic"), ECC_WorldDynamic},
            {TEXT("Pawn"),         ECC_Pawn},
            {TEXT("Visibility"),   ECC_Visibility},
            {TEXT("Camera"),       ECC_Camera},
            {TEXT("PhysicsBody"),  ECC_PhysicsBody},
            {TEXT("Vehicle"),      ECC_Vehicle},
            {TEXT("Destructible"), ECC_Destructible},
        };
        for (const auto& Pair : Fixed)
        {
            Resp->SetStringField(Pair.Key, CollisionResponseToString(Container.GetResponse(Pair.Value)));
        }
        Coll->SetObjectField(TEXT("responses"), Resp);
        Result->SetObjectField(TEXT("collision"), Coll);
    }

    // Optional FProperty snapshot — non-default editable values on the template.
    if (bIncludeProperties && Tpl)
    {
        TArray<TSharedPtr<FJsonValue>> Props;
        UObject* CDO = Tpl->GetClass()->GetDefaultObject();
        for (TFieldIterator<FProperty> It(Tpl->GetClass()); It; ++It)
        {
            FProperty* Prop = *It;
            if (!Prop) continue;
            if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) continue;
            if (Prop->HasAnyPropertyFlags(CPF_Transient)) continue;

            const void* TplAddr = Prop->ContainerPtrToValuePtr<void>(Tpl);
            const void* DefAddr = CDO ? Prop->ContainerPtrToValuePtr<void>(CDO) : nullptr;
            if (DefAddr && Prop->Identical(TplAddr, DefAddr, PPF_DeepComparison)) continue;

            FString StringVal;
            Prop->ExportTextItem_Direct(StringVal, TplAddr, DefAddr, nullptr, PPF_None);

            TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
            PropObj->SetStringField(TEXT("name"),  Prop->GetName());
            PropObj->SetStringField(TEXT("type"),  Prop->GetClass()->GetName());
            PropObj->SetStringField(TEXT("value"), StringVal);
            Props.Add(MakeShared<FJsonValueObject>(PropObj));
        }
        Result->SetArrayField(TEXT("properties"), Props);
        Result->SetNumberField(TEXT("non_default_property_count"), Props.Num());
    }

    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetComponentProperty ───────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path, CompName;
    if (!Params->TryGetStringField(TEXT("path"),      Path))     return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("component"), CompName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component' (name)"));

    const TSharedPtr<FJsonObject>* PropSpec = nullptr;
    if (!Params->TryGetObjectField(TEXT("properties"), PropSpec) || !PropSpec || !PropSpec->IsValid())
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' object (name → value)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    USCS_Node* Node = FindSCSNodeByName(SCS, CompName);
    if (!Node || !Node->ComponentTemplate)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Component not found or has no template: %s"), *CompName));

    Blueprint->Modify();
    Node->Modify();
    Node->ComponentTemplate->Modify();

    TArray<TSharedPtr<FJsonValue>> Applied;
    TArray<TSharedPtr<FJsonValue>> Errors;

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PropSpec)->Values)
    {
        FString PropErr;
        const bool bOk = FAgenticMCPCommonUtils::SetObjectProperty(
            Node->ComponentTemplate, Pair.Key, Pair.Value, PropErr);
        if (bOk)
        {
            Applied.Add(MakeShared<FJsonValueString>(Pair.Key));
        }
        else
        {
            TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
            E->SetStringField(TEXT("name"),  Pair.Key);
            E->SetStringField(TEXT("error"), PropErr);
            Errors.Add(MakeShared<FJsonValueObject>(E));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),    Path);
    Result->SetStringField(TEXT("component"),    CompName);
    Result->SetArrayField (TEXT("applied"),      Applied);
    Result->SetArrayField (TEXT("errors"),       Errors);
    Result->SetNumberField(TEXT("applied_count"),Applied.Num());
    Result->SetNumberField(TEXT("error_count"),  Errors.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetComponentCollision ──────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetComponentCollision(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path, CompName;
    if (!Params->TryGetStringField(TEXT("path"),      Path))     return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("component"), CompName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component' (name)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    USCS_Node* Node = FindSCSNodeByName(SCS, CompName);
    if (!Node || !Node->ComponentTemplate)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Component not found or has no template: %s"), *CompName));

    UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Node->ComponentTemplate);
    if (!Prim)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("'%s' is not a UPrimitiveComponent (collision not applicable)"), *CompName));

    Blueprint->Modify();
    Prim->Modify();

    TArray<TSharedPtr<FJsonValue>> Applied;
    TArray<TSharedPtr<FJsonValue>> Errors;

    // 1) Profile (must apply before per-channel responses so they override the profile).
    FString Profile;
    if (Params->TryGetStringField(TEXT("profile"), Profile))
    {
        Prim->SetCollisionProfileName(FName(*Profile));
        TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
        A->SetStringField(TEXT("profile"), Profile);
        Applied.Add(MakeShared<FJsonValueObject>(A));
    }

    // 2) CollisionEnabled.
    FString EnabledStr;
    if (Params->TryGetStringField(TEXT("enabled"), EnabledStr))
    {
        ECollisionEnabled::Type EnabledVal;
        if (ResolveCollisionEnabled(EnabledStr, EnabledVal))
        {
            Prim->SetCollisionEnabled(EnabledVal);
            TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
            A->SetStringField(TEXT("enabled"), CollisionEnabledToString(EnabledVal));
            Applied.Add(MakeShared<FJsonValueObject>(A));
        }
        else
        {
            TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
            E->SetStringField(TEXT("field"), TEXT("enabled"));
            E->SetStringField(TEXT("error"),
                FString::Printf(TEXT("unknown ECollisionEnabled: '%s' (NoCollision/QueryOnly/PhysicsOnly/QueryAndPhysics/ProbeOnly/QueryAndProbe)"), *EnabledStr));
            Errors.Add(MakeShared<FJsonValueObject>(E));
        }
    }

    // 3) Per-channel responses.
    const TSharedPtr<FJsonObject>* RespSpec = nullptr;
    if (Params->TryGetObjectField(TEXT("responses"), RespSpec) && RespSpec && RespSpec->IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*RespSpec)->Values)
        {
            ECollisionChannel Channel;
            if (!ResolveCollisionChannel(Pair.Key, Channel))
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetStringField(TEXT("field"), Pair.Key);
                E->SetStringField(TEXT("error"), TEXT("unknown collision channel"));
                Errors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            FString RespStr;
            if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(RespStr))
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetStringField(TEXT("field"), Pair.Key);
                E->SetStringField(TEXT("error"), TEXT("response must be a string (Block/Overlap/Ignore)"));
                Errors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            ECollisionResponse RespVal;
            if (!ResolveCollisionResponse(RespStr, RespVal))
            {
                TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
                E->SetStringField(TEXT("field"), Pair.Key);
                E->SetStringField(TEXT("error"),
                    FString::Printf(TEXT("unknown response '%s' (Block/Overlap/Ignore)"), *RespStr));
                Errors.Add(MakeShared<FJsonValueObject>(E));
                continue;
            }
            Prim->SetCollisionResponseToChannel(Channel, RespVal);
            TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
            A->SetStringField(Pair.Key, CollisionResponseToString(RespVal));
            Applied.Add(MakeShared<FJsonValueObject>(A));
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"),    Path);
    Result->SetStringField(TEXT("component"),    CompName);
    Result->SetArrayField (TEXT("applied"),      Applied);
    Result->SetArrayField (TEXT("errors"),       Errors);
    Result->SetNumberField(TEXT("applied_count"),Applied.Num());
    Result->SetNumberField(TEXT("error_count"),  Errors.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetComponentMobility ───────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetComponentMobility(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path, CompName, MobilityStr;
    if (!Params->TryGetStringField(TEXT("path"),      Path))        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("component"), CompName))    return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component' (name)"));
    if (!Params->TryGetStringField(TEXT("mobility"),  MobilityStr)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mobility' (Static/Stationary/Movable)"));

    EComponentMobility::Type Mobility;
    if (!ResolveMobility(MobilityStr, Mobility))
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown mobility '%s' (Static/Stationary/Movable)"), *MobilityStr));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    USCS_Node* Node = FindSCSNodeByName(SCS, CompName);
    if (!Node || !Node->ComponentTemplate)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Component not found or has no template: %s"), *CompName));

    USceneComponent* Scene = Cast<USceneComponent>(Node->ComponentTemplate);
    if (!Scene)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("'%s' is not a USceneComponent (mobility not applicable)"), *CompName));

    Blueprint->Modify();
    Scene->Modify();
    Scene->SetMobility(Mobility);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"), Path);
    Result->SetStringField(TEXT("component"), CompName);
    Result->SetStringField(TEXT("mobility"),  MobilityToString(Scene->Mobility));
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSetComponentTransform ──────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSetComponentTransform(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpComponents;

    FString Path, CompName;
    if (!Params->TryGetStringField(TEXT("path"),      Path))     return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("component"), CompName)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component' (name)"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint has no SimpleConstructionScript"));

    USCS_Node* Node = FindSCSNodeByName(SCS, CompName);
    if (!Node || !Node->ComponentTemplate)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Component not found or has no template: %s"), *CompName));

    USceneComponent* Scene = Cast<USceneComponent>(Node->ComponentTemplate);
    if (!Scene)
        return FAgenticMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("'%s' is not a USceneComponent (transform not applicable)"), *CompName));

    Blueprint->Modify();
    Scene->Modify();
    Node->Modify();

    TArray<TSharedPtr<FJsonValue>> Applied;
    if (Params->HasField(TEXT("location")))
    {
        const FVector V = FAgenticMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        Scene->SetRelativeLocation(V);
        Applied.Add(MakeShared<FJsonValueString>(TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        const FRotator R = FAgenticMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
        Scene->SetRelativeRotation(R);
        Applied.Add(MakeShared<FJsonValueString>(TEXT("rotation")));
    }
    if (Params->HasField(TEXT("scale")))
    {
        const FVector S = FAgenticMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
        Scene->SetRelativeScale3D(S);
        Applied.Add(MakeShared<FJsonValueString>(TEXT("scale")));
    }
    FString AttachSocket;
    if (Params->TryGetStringField(TEXT("attach_socket"), AttachSocket))
    {
        Node->AttachToName = FName(*AttachSocket);
        Applied.Add(MakeShared<FJsonValueString>(TEXT("attach_socket")));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("blueprint"), Path);
    Result->SetStringField(TEXT("component"), CompName);
    Result->SetObjectField(TEXT("location"),  VectorToJson (Scene->GetRelativeLocation()));
    Result->SetObjectField(TEXT("rotation"),  RotatorToJson(Scene->GetRelativeRotation()));
    Result->SetObjectField(TEXT("scale"),     VectorToJson (Scene->GetRelativeScale3D()));
    Result->SetStringField(TEXT("attach_socket"), Node->AttachToName.ToString());
    Result->SetArrayField (TEXT("applied"),   Applied);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// BP-Expert Phase 6 — Compilation, validation, analysis
// =============================================================================

namespace AgenticBpAnalysis
{

    /** Walk every K2 node in every graph and emit a complexity report. */
    struct FComplexityCounts
    {
        int32 GraphCount       = 0;
        int32 NodeCount        = 0;
        int32 PureNodeCount    = 0;
        int32 BranchCount      = 0;  // K2Node_IfThenElse + K2Node_Switch*
        int32 SequenceCount    = 0;  // K2Node_ExecutionSequence
        int32 LatentCount      = 0;  // CallFunction targets with FUNC_BlueprintLatentFunction
        int32 EventCount       = 0;
        int32 CustomEventCount = 0;
        int32 TickEventCount   = 0;  // Receive*Tick events
        int32 ExecEdges        = 0;
        int32 DataEdges        = 0;
    };

    static void CountGraph(UEdGraph* Graph, FComplexityCounts& Out)
    {
        if (!Graph) return;
        ++Out.GraphCount;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) continue;
            // Skip pure visual reroutes — they're not "real" nodes.
            if (Node->IsA<UK2Node_Knot>()) continue;
            ++Out.NodeCount;

            if (Node->IsA<UK2Node_IfThenElse>())          ++Out.BranchCount;
            else if (Node->GetClass()->GetName().StartsWith(TEXT("K2Node_Switch"))) ++Out.BranchCount;
            else if (Node->IsA<UK2Node_ExecutionSequence>()) ++Out.SequenceCount;
            else if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
            {
                ++Out.EventCount;
                ++Out.CustomEventCount;
            }
            else if (UK2Node_Event* Ev = Cast<UK2Node_Event>(Node))
            {
                ++Out.EventCount;
                const FString EventName = Ev->EventReference.GetMemberName().ToString();
                if (EventName.Contains(TEXT("Tick"))) ++Out.TickEventCount;
            }
            else if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
            {
                if (UFunction* Fn = Call->GetTargetFunction())
                {
                    if (Fn->HasAnyFunctionFlags(FUNC_BlueprintPure)) ++Out.PureNodeCount;
                    if (Fn->HasMetaData(TEXT("Latent")))             ++Out.LatentCount;
                }
            }

            // Count edges (per directional pin, halved to avoid double counting).
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output) continue;
                const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
                if (bExec) Out.ExecEdges += Pin->LinkedTo.Num();
                else       Out.DataEdges += Pin->LinkedTo.Num();
            }
        }
    }

    /** McCabe cyclomatic complexity over the exec subgraph. M = E - N + 2*P where
     *  E = exec edges, N = exec-meaningful nodes, P = connected components.
     *  P approximated as the function entry count (each entry / event is one component). */
    static int32 CyclomaticComplexity(const FComplexityCounts& C)
    {
        const int32 P = FMath::Max(C.EventCount + C.SequenceCount, 1);
        return FMath::Max(0, C.ExecEdges - C.NodeCount + 2 * P);
    }

    /** Walker for bp_diff — pulls the comparable bits off a BP. */
    struct FBpSnapshot
    {
        FString ParentClassPath;
        TArray<FString> InterfacePaths;
        TArray<FString> ComponentNames;             // SCS variable names
        TMap<FString, FString> ComponentClasses;    // name → class path
        TMap<FString, FString> Variables;           // name → "category[/subcategory]"
        TMap<FString, int32> FunctionNodeCounts;    // function name → node count
        TArray<FString> DispatcherNames;
    };

    static FBpSnapshot SnapshotBp(UBlueprint* Blueprint)
    {
        FBpSnapshot S;
        S.ParentClassPath = Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT("None");
        for (const FBPInterfaceDescription& I : Blueprint->ImplementedInterfaces)
        {
            if (I.Interface) S.InterfacePaths.Add(I.Interface->GetPathName());
        }
        if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
        {
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (!Node) continue;
                const FString N = Node->GetVariableName().ToString();
                S.ComponentNames.Add(N);
                S.ComponentClasses.Add(N, Node->ComponentClass ? Node->ComponentClass->GetPathName() : TEXT("None"));
            }
        }
        for (const FBPVariableDescription& V : Blueprint->NewVariables)
        {
            FString Type = V.VarType.PinCategory.ToString();
            if (!V.VarType.PinSubCategory.IsNone())
            {
                Type += TEXT("/") + V.VarType.PinSubCategory.ToString();
            }
            S.Variables.Add(V.VarName.ToString(), Type);
        }
        for (UEdGraph* G : Blueprint->FunctionGraphs)
        {
            if (G) S.FunctionNodeCounts.Add(G->GetName(), G->Nodes.Num());
        }
        for (UEdGraph* G : Blueprint->DelegateSignatureGraphs)
        {
            if (G) S.DispatcherNames.Add(G->GetName());
        }
        return S;
    }

}

// ─── HandleBpEstimateComplexity ─────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpEstimateComplexity(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAnalysis;

    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path))
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    FComplexityCounts C;
    for (UEdGraph* G : Blueprint->UbergraphPages)   CountGraph(G, C);
    for (UEdGraph* G : Blueprint->FunctionGraphs)   CountGraph(G, C);
    for (UEdGraph* G : Blueprint->MacroGraphs)      CountGraph(G, C);

    const int32 Cyclomatic = CyclomaticComplexity(C);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),                Path);
    Result->SetNumberField(TEXT("graph_count"),         C.GraphCount);
    Result->SetNumberField(TEXT("node_count"),          C.NodeCount);
    Result->SetNumberField(TEXT("pure_node_count"),     C.PureNodeCount);
    Result->SetNumberField(TEXT("branch_count"),        C.BranchCount);
    Result->SetNumberField(TEXT("sequence_count"),      C.SequenceCount);
    Result->SetNumberField(TEXT("latent_count"),        C.LatentCount);
    Result->SetNumberField(TEXT("event_count"),         C.EventCount);
    Result->SetNumberField(TEXT("custom_event_count"),  C.CustomEventCount);
    Result->SetNumberField(TEXT("tick_event_count"),    C.TickEventCount);
    Result->SetNumberField(TEXT("exec_edges"),          C.ExecEdges);
    Result->SetNumberField(TEXT("data_edges"),          C.DataEdges);
    Result->SetNumberField(TEXT("cyclomatic_complexity"), Cyclomatic);
    Result->SetNumberField(TEXT("variable_count"),      Blueprint->NewVariables.Num());
    Result->SetNumberField(TEXT("interface_count"),     Blueprint->ImplementedInterfaces.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpSearch ─────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpSearch(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;

    FString Path, Query;
    if (!Params->TryGetStringField(TEXT("path"),  Path))  return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
    if (!Params->TryGetStringField(TEXT("query"), Query)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'query'"));

    bool bCaseSensitive = false;
    Params->TryGetBoolField(TEXT("case_sensitive"), bCaseSensitive);
    const ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;

    TSharedPtr<FJsonObject> Err;
    UBlueprint* Blueprint = LoadBlueprintOrError(Path, Err);
    if (!Blueprint) return Err;

    TArray<TSharedPtr<FJsonValue>> Matches;

    auto AddMatch = [&Matches](const FString& Type, const FString& Name, const FString& Location, const FString& Context = TEXT(""))
    {
        TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
        M->SetStringField(TEXT("type"),     Type);
        M->SetStringField(TEXT("name"),     Name);
        M->SetStringField(TEXT("location"), Location);
        if (!Context.IsEmpty()) M->SetStringField(TEXT("context"), Context);
        Matches.Add(MakeShared<FJsonValueObject>(M));
    };

    // 1) Variables.
    for (const FBPVariableDescription& V : Blueprint->NewVariables)
    {
        const FString N = V.VarName.ToString();
        if (N.Contains(Query, SearchCase))
            AddMatch(TEXT("variable"), N, TEXT("NewVariables"), V.VarType.PinCategory.ToString());
    }

    // 2) Functions.
    for (UEdGraph* G : Blueprint->FunctionGraphs)
    {
        if (!G) continue;
        const FString N = G->GetName();
        if (N.Contains(Query, SearchCase))
            AddMatch(TEXT("function"), N, TEXT("FunctionGraphs"));
    }

    // 3) Dispatchers.
    for (UEdGraph* G : Blueprint->DelegateSignatureGraphs)
    {
        if (!G) continue;
        const FString N = G->GetName();
        if (N.Contains(Query, SearchCase))
            AddMatch(TEXT("dispatcher"), N, TEXT("DelegateSignatureGraphs"));
    }

    // 4) Components.
    if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            if (!Node) continue;
            const FString N = Node->GetVariableName().ToString();
            if (N.Contains(Query, SearchCase))
                AddMatch(TEXT("component"), N, TEXT("SimpleConstructionScript"),
                    Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT(""));
        }
    }

    // 5) Node titles + pin defaults across all graphs.
    auto SearchGraph = [&](UEdGraph* G)
    {
        if (!G) return;
        const FString GraphName = G->GetName();
        for (UEdGraphNode* Node : G->Nodes)
        {
            if (!Node) continue;
            const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
            if (Title.Contains(Query, SearchCase))
                AddMatch(TEXT("node"), Node->GetName(), GraphName, Title);
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->DefaultValue.IsEmpty()) continue;
                if (Pin->DefaultValue.Contains(Query, SearchCase))
                    AddMatch(TEXT("pin_literal"), Node->GetName() + TEXT(":") + Pin->PinName.ToString(),
                        GraphName, Pin->DefaultValue);
            }
        }
    };
    for (UEdGraph* G : Blueprint->UbergraphPages)   SearchGraph(G);
    for (UEdGraph* G : Blueprint->FunctionGraphs)   SearchGraph(G);
    for (UEdGraph* G : Blueprint->MacroGraphs)      SearchGraph(G);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"),        Path);
    Result->SetStringField(TEXT("query"),       Query);
    Result->SetBoolField  (TEXT("case_sensitive"),bCaseSensitive);
    Result->SetArrayField (TEXT("matches"),     Matches);
    Result->SetNumberField(TEXT("match_count"), Matches.Num());
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpDiff ───────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpDiff(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpAnalysis;

    FString LeftPath, RightPath;
    if (!Params->TryGetStringField(TEXT("left"),  LeftPath))  return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'left' (BP content path)"));
    if (!Params->TryGetStringField(TEXT("right"), RightPath)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'right' (BP content path)"));

    TSharedPtr<FJsonObject> ErrL, ErrR;
    UBlueprint* Left  = LoadBlueprintOrError(LeftPath,  ErrL); if (!Left)  return ErrL;
    UBlueprint* Right = LoadBlueprintOrError(RightPath, ErrR); if (!Right) return ErrR;

    const FBpSnapshot L = SnapshotBp(Left);
    const FBpSnapshot R = SnapshotBp(Right);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("left"),  LeftPath);
    Result->SetStringField(TEXT("right"), RightPath);

    // Parent class.
    TSharedPtr<FJsonObject> ParentObj = MakeShared<FJsonObject>();
    ParentObj->SetStringField(TEXT("left"),    L.ParentClassPath);
    ParentObj->SetStringField(TEXT("right"),   R.ParentClassPath);
    ParentObj->SetBoolField  (TEXT("changed"), L.ParentClassPath != R.ParentClassPath);
    Result->SetObjectField(TEXT("parent_class"), ParentObj);

    auto DiffStringArrays = [](const TArray<FString>& A, const TArray<FString>& B,
                               TArray<TSharedPtr<FJsonValue>>& Added,
                               TArray<TSharedPtr<FJsonValue>>& Removed)
    {
        const TSet<FString> SetA(A), SetB(B);
        for (const FString& E : SetB) if (!SetA.Contains(E)) Added.Add(MakeShared<FJsonValueString>(E));
        for (const FString& E : SetA) if (!SetB.Contains(E)) Removed.Add(MakeShared<FJsonValueString>(E));
    };

    auto DiffStringMaps = [](const TMap<FString, FString>& A, const TMap<FString, FString>& B,
                             TArray<TSharedPtr<FJsonValue>>& Added,
                             TArray<TSharedPtr<FJsonValue>>& Removed,
                             TArray<TSharedPtr<FJsonValue>>& Changed)
    {
        for (const auto& P : B)
        {
            const FString* AV = A.Find(P.Key);
            if (!AV)
            {
                TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
                O->SetStringField(TEXT("name"),  P.Key);
                O->SetStringField(TEXT("value"), P.Value);
                Added.Add(MakeShared<FJsonValueObject>(O));
            }
            else if (*AV != P.Value)
            {
                TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
                O->SetStringField(TEXT("name"),  P.Key);
                O->SetStringField(TEXT("left"),  *AV);
                O->SetStringField(TEXT("right"), P.Value);
                Changed.Add(MakeShared<FJsonValueObject>(O));
            }
        }
        for (const auto& P : A)
        {
            if (!B.Contains(P.Key))
            {
                TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
                O->SetStringField(TEXT("name"),  P.Key);
                O->SetStringField(TEXT("value"), P.Value);
                Removed.Add(MakeShared<FJsonValueObject>(O));
            }
        }
    };

    // Interfaces.
    TArray<TSharedPtr<FJsonValue>> IfAdded, IfRemoved;
    DiffStringArrays(L.InterfacePaths, R.InterfacePaths, IfAdded, IfRemoved);
    TSharedPtr<FJsonObject> IfObj = MakeShared<FJsonObject>();
    IfObj->SetArrayField(TEXT("added"),   IfAdded);
    IfObj->SetArrayField(TEXT("removed"), IfRemoved);
    Result->SetObjectField(TEXT("interfaces"), IfObj);

    // Components (compare by class path map).
    TArray<TSharedPtr<FJsonValue>> CmpAdded, CmpRemoved, CmpChanged;
    DiffStringMaps(L.ComponentClasses, R.ComponentClasses, CmpAdded, CmpRemoved, CmpChanged);
    TSharedPtr<FJsonObject> CmpObj = MakeShared<FJsonObject>();
    CmpObj->SetArrayField(TEXT("added"),   CmpAdded);
    CmpObj->SetArrayField(TEXT("removed"), CmpRemoved);
    CmpObj->SetArrayField(TEXT("changed"), CmpChanged);
    Result->SetObjectField(TEXT("components"), CmpObj);

    // Variables.
    TArray<TSharedPtr<FJsonValue>> VAdded, VRemoved, VChanged;
    DiffStringMaps(L.Variables, R.Variables, VAdded, VRemoved, VChanged);
    TSharedPtr<FJsonObject> VObj = MakeShared<FJsonObject>();
    VObj->SetArrayField(TEXT("added"),   VAdded);
    VObj->SetArrayField(TEXT("removed"), VRemoved);
    VObj->SetArrayField(TEXT("changed"), VChanged);
    Result->SetObjectField(TEXT("variables"), VObj);

    // Functions (compare names + node counts).
    TMap<FString, FString> LFn, RFn;
    for (const auto& P : L.FunctionNodeCounts) LFn.Add(P.Key, FString::FromInt(P.Value));
    for (const auto& P : R.FunctionNodeCounts) RFn.Add(P.Key, FString::FromInt(P.Value));
    TArray<TSharedPtr<FJsonValue>> FAdded, FRemoved, FChanged;
    DiffStringMaps(LFn, RFn, FAdded, FRemoved, FChanged);
    TSharedPtr<FJsonObject> FObj = MakeShared<FJsonObject>();
    FObj->SetArrayField(TEXT("added"),   FAdded);
    FObj->SetArrayField(TEXT("removed"), FRemoved);
    FObj->SetArrayField(TEXT("changed_node_counts"), FChanged);
    Result->SetObjectField(TEXT("functions"), FObj);

    // Dispatchers.
    TArray<TSharedPtr<FJsonValue>> DAdded, DRemoved;
    DiffStringArrays(L.DispatcherNames, R.DispatcherNames, DAdded, DRemoved);
    TSharedPtr<FJsonObject> DObj = MakeShared<FJsonObject>();
    DObj->SetArrayField(TEXT("added"),   DAdded);
    DObj->SetArrayField(TEXT("removed"), DRemoved);
    Result->SetObjectField(TEXT("dispatchers"), DObj);

    // Summary: total number of structural differences.
    const int32 TotalDiffs =
        (ParentObj->GetBoolField(TEXT("changed")) ? 1 : 0)
        + IfAdded.Num() + IfRemoved.Num()
        + CmpAdded.Num() + CmpRemoved.Num() + CmpChanged.Num()
        + VAdded.Num() + VRemoved.Num() + VChanged.Num()
        + FAdded.Num() + FRemoved.Num() + FChanged.Num()
        + DAdded.Num() + DRemoved.Num();
    Result->SetNumberField(TEXT("total_diffs"), TotalDiffs);
    Result->SetBoolField  (TEXT("identical"),   TotalDiffs == 0);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// ─── HandleBpReplaceReferences ──────────────────────────────────────────

TSharedPtr<FJsonObject> FAgenticMCPBlueprintCommands::HandleBpReplaceReferences(const TSharedPtr<FJsonObject>& Params)
{
    using namespace AgenticBpInspect;
    using namespace AgenticBpEdit;       // ResolveClassByName

    FString OldSpec, NewSpec;
    if (!Params->TryGetStringField(TEXT("old"), OldSpec)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old' (BP path or class spec)"));
    if (!Params->TryGetStringField(TEXT("new"), NewSpec)) return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new' (BP path or class spec)"));

    UClass* OldClass = ResolveClassByName(OldSpec);
    UClass* NewClass = ResolveClassByName(NewSpec);
    if (!OldClass) return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Old class not found: %s"), *OldSpec));
    if (!NewClass) return FAgenticMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("New class not found: %s"), *NewSpec));
    if (OldClass == NewClass)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Old and new classes are the same — nothing to do"));

    bool bDryRun = false;
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
        return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));

    TArray<TSharedPtr<FJsonValue>> Replaced;
    int32 ScannedCount = 0;
    int32 ReplacedCount = 0;

    // Snapshot-then-mutate. TActorIterator's FActorIteratorState hooks
    // AddOnActorSpawnedHandler at construction (EngineUtils.h:261-262) and
    // operator++ walks both ActorArray AND SpawnedActorArray. If NewClass IsA
    // OldClass — the common "swap V1 → V2 where V2 extends V1" case — every
    // replacement spawned inside the loop would itself qualify and be replaced
    // again, looping until the dispatcher timeout fires. Snapshot the match
    // list first, then mutate in a second pass (matches UE's
    // UEditorActorSubsystem::ConvertActorsFromClass at
    // Subsystems/EditorActorSubsystem.cpp:2216-2267).
    TArray<AActor*> Matches;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;
        ++ScannedCount;
        if (Actor->IsA(OldClass)) Matches.Add(Actor);
    }

    for (AActor* Actor : Matches)
    {
        if (!Actor || !IsValid(Actor)) continue;

        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetStringField(TEXT("label"),     Actor->GetActorLabel());
        R->SetStringField(TEXT("path"),      Actor->GetPathName());
        R->SetStringField(TEXT("old_class"), Actor->GetClass()->GetPathName());

        if (bDryRun)
        {
            R->SetBoolField(TEXT("would_replace"), true);
            Replaced.Add(MakeShared<FJsonValueObject>(R));
            ++ReplacedCount;
            continue;
        }

        // Capture transform + label before destroy.
        const FTransform Xform = Actor->GetActorTransform();
        const FString Label    = Actor->GetActorLabel();

        FActorSpawnParameters SpawnParams;
        SpawnParams.OverrideLevel = Actor->GetLevel();
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AActor* NewActor = World->SpawnActor<AActor>(NewClass, Xform, SpawnParams);
        if (!NewActor)
        {
            R->SetStringField(TEXT("error"), TEXT("World->SpawnActor returned null"));
            Replaced.Add(MakeShared<FJsonValueObject>(R));
            continue;
        }
        NewActor->SetActorLabel(Label);

        // Preserve property values that exist on both classes.
        UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
        CopyParams.bNotifyObjectReplacement = true;
        UEngine::CopyPropertiesForUnrelatedObjects(Actor, NewActor, CopyParams);

        World->DestroyActor(Actor);

        R->SetStringField(TEXT("new_path"), NewActor->GetPathName());
        R->SetBoolField  (TEXT("replaced"), true);
        Replaced.Add(MakeShared<FJsonValueObject>(R));
        ++ReplacedCount;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("old"),             OldClass->GetPathName());
    Result->SetStringField(TEXT("new"),             NewClass->GetPathName());
    Result->SetStringField(TEXT("world"),           World->GetName());
    Result->SetBoolField  (TEXT("dry_run"),         bDryRun);
    Result->SetNumberField(TEXT("scanned_actors"),  ScannedCount);
    Result->SetNumberField(TEXT("replaced_count"),  ReplacedCount);
    Result->SetArrayField (TEXT("replaced"),        Replaced);
    return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}
