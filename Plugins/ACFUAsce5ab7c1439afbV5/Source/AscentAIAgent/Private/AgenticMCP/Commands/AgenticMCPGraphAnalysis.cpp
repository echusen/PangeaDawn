// Copyright Insodimension Studios. All Rights Reserved.

#include "Commands/AgenticMCPGraphAnalysis.h"
#include "Commands/AgenticMCPCommonUtils.h"
#include "AgenticMCPModule.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimStateEntryNode.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Root.h"

#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_BlendListByEnum.h"

#include "AnimGraphNode_StateMachine.h"
#include "AnimationStateMachineSchema.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"

#include "Animation/BlendProfile.h"
#include "Animation/Skeleton.h"

// ---------------------------------------------------------------------------
// Local Helpers
// ---------------------------------------------------------------------------

namespace
{

/** Try to extract a required string parameter, returning an error response on failure. */
TSharedPtr<FJsonObject> RequireStringParam(
	const TSharedPtr<FJsonObject>& InParams,
	const TCHAR* InFieldName,
	FString& OutValue)
{
	if (InParams->TryGetStringField(InFieldName, OutValue))
	{
		return nullptr;
	}
	return FAgenticMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Missing required parameter: %s"), InFieldName));
}

/** Load a UBlueprint from path, returning an error response on failure. */
TSharedPtr<FJsonObject> LoadBlueprintChecked(
	const FString& InPath,
	UBlueprint*& OutBlueprint)
{
	OutBlueprint = LoadObject<UBlueprint>(nullptr, *InPath);
	if (OutBlueprint)
	{
		return nullptr;
	}
	return FAgenticMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Failed to load Blueprint at: %s"), *InPath));
}

/** Load a UAnimBlueprint from path, returning an error response on failure. */
TSharedPtr<FJsonObject> LoadAnimBlueprintChecked(
	const FString& InPath,
	UAnimBlueprint*& OutAnimBlueprint)
{
	OutAnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *InPath);
	if (OutAnimBlueprint)
	{
		return nullptr;
	}
	return FAgenticMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Failed to load AnimBlueprint at: %s"), *InPath));
}

/** Find a node by name (exact or substring match) in a graph. */
UEdGraphNode* FindNodeByName(UEdGraph* InGraph, const FString& InNodeName)
{
	for (UEdGraphNode* Node : InGraph->Nodes)
	{
		if (!Node) continue;
		if (Node->GetName() == InNodeName || Node->GetName().Contains(InNodeName))
		{
			return Node;
		}
	}
	return nullptr;
}

/** Find a node by FGuid::Parse-able string. Used by the converged schema so
 *  callers spawned via the agentic_anim_* family can address nodes by NodeGuid
 *  without first calling summary() to discover the editor name. */
UEdGraphNode* FindNodeByGuid(UEdGraph* InGraph, const FString& InGuidString)
{
	FGuid AsGuid;
	if (!FGuid::Parse(InGuidString, AsGuid))
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Node && Node->NodeGuid == AsGuid)
		{
			return Node;
		}
	}
	return nullptr;
}

/** Pull a string parameter trying multiple keys in order — first non-empty match wins.
 *  Used to bridge the legacy `blueprint_path` / `node_name` / `graph_name` schema
 *  with the converged `path` / `node_guid` / `graph` schema introduced by the
 *  agentic_anim_* family without forking every handler. */
bool TryGetStringParamAny(
	const TSharedPtr<FJsonObject>& InParams,
	std::initializer_list<const TCHAR*> InKeys,
	FString& OutValue)
{
	for (const TCHAR* Key : InKeys)
	{
		FString Candidate;
		if (InParams->TryGetStringField(Key, Candidate) && !Candidate.IsEmpty())
		{
			OutValue = MoveTemp(Candidate);
			return true;
		}
	}
	return false;
}

/** Collect all node names from a graph for diagnostic error messages. */
FString GetAvailableNodeNames(UEdGraph* InGraph)
{
	TArray<FString> Names;
	for (UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Node) Names.Add(Node->GetName());
	}
	return FString::Join(Names, TEXT(", "));
}

/** Convert a JSON value (string, bool, or number) to an import-text string. */
FString JsonValueToImportString(const TSharedPtr<FJsonValue>& InValue)
{
	FString ValueStr;
	if (InValue->TryGetString(ValueStr))
	{
		return ValueStr;
	}

	bool bBoolVal;
	if (InValue->TryGetBool(bBoolVal))
	{
		return bBoolVal ? TEXT("True") : TEXT("False");
	}

	double NumVal;
	if (InValue->TryGetNumber(NumVal))
	{
		return FString::SanitizeFloat(NumVal);
	}

	return FString();
}

/** Convert EBlendProfileMode to string. */
FString BlendProfileModeToString(EBlendProfileMode InMode)
{
	switch (InMode)
	{
	case EBlendProfileMode::TimeFactor:   return TEXT("TimeFactor");
	case EBlendProfileMode::WeightFactor: return TEXT("WeightFactor");
	case EBlendProfileMode::BlendMask:    return TEXT("BlendMask");
	default:                              return TEXT("Unknown");
	}
}

} // anonymous namespace

// =============================================================================
// Command Routing
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleCommand(
	const FString& InCommandType,
	const TSharedPtr<FJsonObject>& InParams)
{
	if (InCommandType == TEXT("analyze_animgraph"))
	{
		return HandleAnalyzeAnimGraph(InParams);
	}
	else if (InCommandType == TEXT("add_animgraph_node"))
	{
		return HandleAddAnimGraphNode(InParams);
	}
	else if (InCommandType == TEXT("connect_animgraph_nodes"))
	{
		return HandleConnectAnimGraphNodes(InParams);
	}
	else if (InCommandType == TEXT("set_animgraph_node_properties"))
	{
		return HandleSetAnimGraphNodeProperties(InParams);
	}
	else if (InCommandType == TEXT("remove_animgraph_node"))
	{
		return HandleRemoveAnimGraphNode(InParams);
	}
	else if (InCommandType == TEXT("animgraph_pin"))
	{
		return HandleAnimGraphPin(InParams);
	}
	else if (InCommandType == TEXT("blend_profile"))
	{
		return HandleBlendProfile(InParams);
	}
	else if (InCommandType == TEXT("state_machine"))
	{
		return HandleStateMachine(InParams);
	}

	return FAgenticMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown graph analysis command: %s"), *InCommandType));
}

// =============================================================================
// analyze_animgraph
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleAnalyzeAnimGraph(
	const TSharedPtr<FJsonObject>& InParams)
{
	FString BlueprintPath;
	if (auto Err = RequireStringParam(InParams, TEXT("blueprint_path"), BlueprintPath)) return Err;

	UAnimBlueprint* AnimBlueprint = nullptr;
	if (auto Err = LoadAnimBlueprintChecked(BlueprintPath, AnimBlueprint)) return Err;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"), AnimBlueprint->GetName());
	Result->SetStringField(TEXT("path"), BlueprintPath);
	Result->SetStringField(TEXT("target_skeleton"),
		AnimBlueprint->TargetSkeleton ? AnimBlueprint->TargetSkeleton->GetName() : TEXT("None"));

	TArray<TSharedPtr<FJsonValue>> EventGraphs = CollectEventGraphs(AnimBlueprint);
	if (EventGraphs.Num() > 0)
	{
		Result->SetArrayField(TEXT("event_graphs"), EventGraphs);
	}
	Result->SetArrayField(TEXT("variables"), CollectVariables(AnimBlueprint));
	TSharedPtr<FJsonObject> AnimGraphObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> StateMachines;
	TArray<TSharedPtr<FJsonValue>> BlendNodes;
	TArray<TSharedPtr<FJsonValue>> RootNodes;

	for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
	{
		if (!Graph || !Graph->GetSchema()) continue;
		if (!Graph->GetSchema()->IsA<UAnimationGraphSchema>()) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			if (UAnimGraphNode_StateMachineBase* StateMachineNode = Cast<UAnimGraphNode_StateMachineBase>(Node))
			{
				TSharedPtr<FJsonObject> SMDescription = DescribeStateMachine(StateMachineNode);
				if (SMDescription.IsValid())
				{
					StateMachines.Add(MakeShared<FJsonValueObject>(SMDescription));
				}
				continue;
			}

			if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node))
			{
				TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
				NodeObj->SetStringField(TEXT("type"), Node->GetClass()->GetName());
				NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

				FString NodeProperties = ExportAnimNodeProperties(Node);
				if (!NodeProperties.IsEmpty())
				{
					NodeObj->SetStringField(TEXT("properties"), NodeProperties);
				}

				TSharedPtr<FJsonObject> WrapperProps = ExportWrapperProperties(Node);
				if (WrapperProps.IsValid())
				{
					NodeObj->SetObjectField(TEXT("wrapper_properties"), WrapperProps);
				}

				FString ClassName = Node->GetClass()->GetName();
				if (ClassName.Contains(TEXT("Root")) || ClassName.Contains(TEXT("OutputPose")))
				{
					RootNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
				}
				else
				{
					BlendNodes.Add(MakeShared<FJsonValueObject>(NodeObj));
				}
				continue;
			}
		}
	}

	AnimGraphObj->SetArrayField(TEXT("root_nodes"), RootNodes);
	AnimGraphObj->SetArrayField(TEXT("state_machines"), StateMachines);
	AnimGraphObj->SetArrayField(TEXT("blend_nodes"), BlendNodes);
	Result->SetObjectField(TEXT("anim_graph"), AnimGraphObj);

	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// Blueprint Structure Helpers
// =============================================================================

TArray<TSharedPtr<FJsonValue>> FAgenticMCPGraphAnalysis::CollectVariables(UBlueprint* InBlueprint)
{
	TArray<TSharedPtr<FJsonValue>> Variables;

	for (const FBPVariableDescription& Var : InBlueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), PinTypeToString(Var.VarType));
		VarObj->SetBoolField(TEXT("instance_editable"), (Var.PropertyFlags & CPF_Edit) != 0);
		VarObj->SetBoolField(TEXT("expose_on_spawn"), (Var.PropertyFlags & CPF_ExposeOnSpawn) != 0);
		VarObj->SetBoolField(TEXT("is_array"), Var.VarType.IsArray());

		if (!Var.DefaultValue.IsEmpty())
		{
			VarObj->SetStringField(TEXT("default_value"), Var.DefaultValue);
		}

		Variables.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	return Variables;
}

TArray<TSharedPtr<FJsonValue>> FAgenticMCPGraphAnalysis::CollectEventGraphs(UBlueprint* InBlueprint)
{
	TArray<TSharedPtr<FJsonValue>> EventGraphs;
	for (UEdGraph* Graph : InBlueprint->UbergraphPages)
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
				Events.Add(MakeShared<FJsonValueString>(
					EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString()));
			}
		}
		GraphObj->SetArrayField(TEXT("events"), Events);
		EventGraphs.Add(MakeShared<FJsonValueObject>(GraphObj));
	}
	return EventGraphs;
}

// =============================================================================
// AnimGraph Helpers
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::DescribeStateMachine(
	UAnimGraphNode_StateMachineBase* InStateMachineNode)
{
	if (!InStateMachineNode) return nullptr;

	TSharedPtr<FJsonObject> SMObj = MakeShared<FJsonObject>();
	SMObj->SetStringField(TEXT("name"),
		InStateMachineNode->GetNodeTitle(ENodeTitleType::ListView).ToString());

	UAnimationStateMachineGraph* SMGraph = InStateMachineNode->EditorStateMachineGraph;
	if (!SMGraph)
	{
		SMObj->SetStringField(TEXT("error"), TEXT("No state machine graph available"));
		return SMObj;
	}

	if (SMGraph->EntryNode)
	{
		for (UEdGraphPin* Pin : SMGraph->EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
			{
				if (UAnimStateNode* EntryState = Cast<UAnimStateNode>(Pin->LinkedTo[0]->GetOwningNode()))
				{
					SMObj->SetStringField(TEXT("entry_state"), EntryState->GetStateName());
				}
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> States;
	TArray<TSharedPtr<FJsonValue>> Transitions;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (!Node) continue;

		if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			States.Add(MakeShared<FJsonValueObject>(DescribeAnimState(StateNode)));
		}
		else if (UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node))
		{
			Transitions.Add(MakeShared<FJsonValueObject>(DescribeAnimTransition(TransitionNode)));
		}
	}

	SMObj->SetArrayField(TEXT("states"), States);
	SMObj->SetArrayField(TEXT("transitions"), Transitions);

	return SMObj;
}

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::DescribeAnimState(UAnimStateNode* InStateNode)
{
	TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
	StateObj->SetStringField(TEXT("name"), InStateNode->GetStateName());

	UEdGraph* BoundGraph = InStateNode->BoundGraph;
	if (!BoundGraph)
	{
		return StateObj;
	}

	StateObj->SetNumberField(TEXT("node_count"), BoundGraph->Nodes.Num());

	TArray<TSharedPtr<FJsonValue>> InnerNodes;
	for (UEdGraphNode* InnerNode : BoundGraph->Nodes)
	{
		if (!InnerNode) continue;
		if (!Cast<UAnimGraphNode_Base>(InnerNode)) continue;

		TSharedPtr<FJsonObject> InnerObj = MakeShared<FJsonObject>();
		InnerObj->SetStringField(TEXT("type"), InnerNode->GetClass()->GetName());
		InnerObj->SetStringField(TEXT("title"),
			InnerNode->GetNodeTitle(ENodeTitleType::ListView).ToString());

		FString InnerProps = ExportAnimNodeProperties(InnerNode);
		if (!InnerProps.IsEmpty())
		{
			InnerObj->SetStringField(TEXT("properties"), InnerProps);
		}
		InnerNodes.Add(MakeShared<FJsonValueObject>(InnerObj));
	}
	StateObj->SetArrayField(TEXT("anim_nodes"), InnerNodes);

	return StateObj;
}

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::DescribeAnimTransition(
	UAnimStateTransitionNode* InTransitionNode)
{
	TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();

	UAnimStateNodeBase* PrevState = InTransitionNode->GetPreviousState();
	UAnimStateNodeBase* NextState = InTransitionNode->GetNextState();

	TransObj->SetStringField(TEXT("from"),
		PrevState ? PrevState->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown"));
	TransObj->SetStringField(TEXT("to"),
		NextState ? NextState->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("Unknown"));
	TransObj->SetBoolField(TEXT("has_rule"), InTransitionNode->BoundGraph != nullptr);
	if (InTransitionNode->BoundGraph)
	{
		TransObj->SetNumberField(TEXT("rule_node_count"), InTransitionNode->BoundGraph->Nodes.Num());
	}
	return TransObj;
}

// =============================================================================
// Graph Node Helpers
// =============================================================================

// =============================================================================
// Utility
// =============================================================================

FString FAgenticMCPGraphAnalysis::PinTypeToString(const FEdGraphPinType& InPinType)
{
	FString TypeStr;

	if (InPinType.PinSubCategoryObject.IsValid())
	{
		TypeStr = InPinType.PinSubCategoryObject->GetName();
	}
	else
	{
		TypeStr = InPinType.PinCategory.ToString();
	}

	if (InPinType.IsArray())
	{
		TypeStr = FString::Printf(TEXT("Array<%s>"), *TypeStr);
	}
	else if (InPinType.IsSet())
	{
		TypeStr = FString::Printf(TEXT("Set<%s>"), *TypeStr);
	}
	else if (InPinType.IsMap())
	{
		TypeStr = FString::Printf(TEXT("Map<%s, %s>"), *TypeStr,
			InPinType.PinValueType.TerminalSubCategoryObject.IsValid()
				? *InPinType.PinValueType.TerminalSubCategoryObject->GetName()
				: *InPinType.PinValueType.TerminalCategory.ToString());
	}

	return TypeStr;
}

FString FAgenticMCPGraphAnalysis::ExportAnimNodeProperties(UEdGraphNode* InNode)
{
	if (!InNode) return FString();
	if (!Cast<UAnimGraphNode_Base>(InNode)) return FString();

	FStructProperty* NodeProp = CastField<FStructProperty>(
		InNode->GetClass()->FindPropertyByName(TEXT("Node")));
	if (!NodeProp) return FString();

	FString ExportText;
	const void* ValuePtr = NodeProp->ContainerPtrToValuePtr<void>(InNode);
	NodeProp->ExportTextItem_Direct(ExportText, ValuePtr, nullptr, InNode, PPF_None);
	return ExportText;
}

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::ExportWrapperProperties(UEdGraphNode* InNode)
{
	if (!InNode) return nullptr;

	UAnimGraphNode_Base* AnimGraphNode = Cast<UAnimGraphNode_Base>(InNode);
	if (!AnimGraphNode) return nullptr;

	TSharedPtr<FJsonObject> WrapperObj = MakeShared<FJsonObject>();

	static const FName FunctionBindingNames[] = {
		TEXT("UpdateFunction"),
		TEXT("BecomeRelevantFunction"),
		TEXT("InitialUpdateFunction"),
	};

	for (const FName& PropName : FunctionBindingNames)
	{
		FProperty* Prop = AnimGraphNode->GetClass()->FindPropertyByName(PropName);
		if (!Prop) continue;

		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(AnimGraphNode);
		FString ExportText;
		Prop->ExportTextItem_Direct(ExportText, ValuePtr, nullptr, AnimGraphNode, PPF_None);

		if (!ExportText.IsEmpty() && ExportText != TEXT("()"))
		{
			WrapperObj->SetStringField(PropName.ToString(), ExportText);
		}
	}

	FProperty* ADPProp = AnimGraphNode->GetClass()->FindPropertyByName(TEXT("AlwaysDynamicProperties"));
	if (ADPProp)
	{
		const TSet<FName>* PropSet = ADPProp->ContainerPtrToValuePtr<TSet<FName>>(AnimGraphNode);
		if (PropSet && PropSet->Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ADPArray;
			for (const FName& Name : *PropSet)
			{
				ADPArray.Add(MakeShared<FJsonValueString>(Name.ToString()));
			}
			WrapperObj->SetArrayField(TEXT("AlwaysDynamicProperties"), ADPArray);
		}
	}

	return WrapperObj->Values.Num() > 0 ? WrapperObj : nullptr;
}

UEdGraph* FAgenticMCPGraphAnalysis::FindGraphByName(UBlueprint* InBlueprint, const FString& InGraphName)
{
	TArray<UEdGraph*> AllGraphs;
	InBlueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName() == InGraphName)
		{
			return Graph;
		}
	}

	return nullptr;
}

// =============================================================================
// add_animgraph_node
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleAddAnimGraphNode(
	const TSharedPtr<FJsonObject>& InParams)
{
	// Schema convergence: accept `blueprint_path`/`path` and `node_type`/`class`/`class_name`.
	FString BlueprintPath, NodeType;
	if (!TryGetStringParamAny(InParams, { TEXT("blueprint_path"), TEXT("path") }, BlueprintPath))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: blueprint_path (alias: path)"));
	}
	if (!TryGetStringParamAny(InParams, { TEXT("node_type"), TEXT("class"), TEXT("class_name") }, NodeType))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: node_type (aliases: class, class_name)"));
	}

	FString GraphName = TEXT("AnimGraph");
	TryGetStringParamAny(InParams, { TEXT("graph_name"), TEXT("graph") }, GraphName);

	int32 PosX = -300, PosY = 0;
	InParams->TryGetNumberField(TEXT("x"), PosX);
	InParams->TryGetNumberField(TEXT("y"), PosY);

	UAnimBlueprint* AnimBlueprint = nullptr;
	if (auto Err = LoadAnimBlueprintChecked(BlueprintPath, AnimBlueprint)) return Err;

	UEdGraph* TargetGraph = FindGraphByName(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Graph '%s' not found in AnimBlueprint"), *GraphName));
	}
	UClass* NodeClass = nullptr;
	if (NodeType == TEXT("SequenceEvaluator"))
	{
		NodeClass = UAnimGraphNode_SequenceEvaluator::StaticClass();
	}
	else if (NodeType == TEXT("SequencePlayer"))
	{
		NodeClass = UAnimGraphNode_SequencePlayer::StaticClass();
	}
	else if (NodeType == TEXT("SaveCachedPose"))
	{
		NodeClass = UAnimGraphNode_SaveCachedPose::StaticClass();
	}
	else if (NodeType == TEXT("UseCachedPose"))
	{
		NodeClass = UAnimGraphNode_UseCachedPose::StaticClass();
	}
	else
	{
		// Try to find by full class name
		FString FullClassName = FString::Printf(TEXT("AnimGraphNode_%s"), *NodeType);
		NodeClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::ExactClass);
		if (!NodeClass)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Unknown node type: '%s'. Supported: SequenceEvaluator, SequencePlayer, or full AnimGraphNode_* class name"), *NodeType));
		}
	}

	UAnimGraphNode_Base* NewNode = NewObject<UAnimGraphNode_Base>(TargetGraph, NodeClass);
	if (!NewNode)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create AnimGraph node"));
	}

	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->AllocateDefaultPins();
	TargetGraph->Modify();
	TargetGraph->AddNode(NewNode, false, false);

	FString AnimAssetPath;
	FString AnimationError;
	if (InParams->TryGetStringField(TEXT("animation"), AnimAssetPath))
	{
		UAnimationAsset* AnimAsset = LoadObject<UAnimationAsset>(nullptr, *AnimAssetPath);
		if (AnimAsset)
		{
			UAnimGraphNode_AssetPlayerBase* AssetPlayer = Cast<UAnimGraphNode_AssetPlayerBase>(NewNode);
			if (AssetPlayer)
			{
				AssetPlayer->SetAnimationAsset(AnimAsset);
			}
		}
		else
		{
			AnimationError = FString::Printf(TEXT("Failed to load animation at: %s"), *AnimAssetPath);
			UE_LOG(LogAgenticMCP, Warning, TEXT("AgenticMCP: %s"), *AnimationError);
		}
	}

	auto SetFunctionBinding = [&](const FString& ParamName, const FString& PropertyName)
	{
		FString FunctionName;
		if (InParams->TryGetStringField(ParamName, FunctionName))
		{
			FStructProperty* Prop = CastField<FStructProperty>(
				NewNode->GetClass()->FindPropertyByName(*PropertyName));
			if (Prop)
			{
				void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(NewNode);
				FString ImportStr = FString::Printf(
					TEXT("(MemberParent=None,MemberName=\"%s\",bSelfContext=True)"),
					*FunctionName);
				Prop->ImportText_Direct(*ImportStr, ValuePtr, NewNode, PPF_None);
			}
		}
	};

	SetFunctionBinding(TEXT("update_function"), TEXT("UpdateFunction"));
	SetFunctionBinding(TEXT("become_relevant_function"), TEXT("BecomeRelevantFunction"));
	SetFunctionBinding(TEXT("initial_update_function"), TEXT("InitialUpdateFunction"));

	// Handle cache_name for SaveCachedPose / UseCachedPose nodes
	FString CacheName;
	if (InParams->TryGetStringField(TEXT("cache_name"), CacheName))
	{
		if (UAnimGraphNode_SaveCachedPose* SaveNode = Cast<UAnimGraphNode_SaveCachedPose>(NewNode))
		{
			SaveNode->CacheName = CacheName;
		}
		else if (UAnimGraphNode_UseCachedPose* UseNode = Cast<UAnimGraphNode_UseCachedPose>(NewNode))
		{
			// Find the matching SaveCachedPose node in this graph and link to it
			for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
			{
				if (UAnimGraphNode_SaveCachedPose* SavePose = Cast<UAnimGraphNode_SaveCachedPose>(ExistingNode))
				{
					if (SavePose->CacheName == CacheName)
					{
						// Link via the internal pose link
						UseNode->SaveCachedPoseNode = SavePose;
						break;
					}
				}
			}
		}
	}

	const TSharedPtr<FJsonObject>* NodePropsPtr = nullptr;
	if (InParams->TryGetObjectField(TEXT("node_properties"), NodePropsPtr) && NodePropsPtr)
	{
		FStructProperty* NodeProp = CastField<FStructProperty>(
			NewNode->GetClass()->FindPropertyByName(TEXT("Node")));
		if (NodeProp)
		{
			void* StructPtr = NodeProp->ContainerPtrToValuePtr<void>(NewNode);
			UScriptStruct* StructType = NodeProp->Struct;

			for (const auto& Pair : (*NodePropsPtr)->Values)
			{
				FProperty* SubProp = StructType->FindPropertyByName(*Pair.Key);
				if (!SubProp) continue;

				FString ValueStr = JsonValueToImportString(Pair.Value);
				if (!ValueStr.IsEmpty())
				{
					void* SubValuePtr = SubProp->ContainerPtrToValuePtr<void>(StructPtr);
					SubProp->ImportText_Direct(*ValueStr, SubValuePtr, NewNode, PPF_None);
				}
			}
		}
	}

	AnimBlueprint->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("node_name"), NewNode->GetName());
	// Echo node_guid so callers can chain into agentic_anim_* mutators
	// (set_node_properties, link_nodes, bind_pins, animgraph_pin) without
	// an intermediate summary() lookup.
	Result->SetStringField(TEXT("node_guid"), NewNode->NodeGuid.ToString());
	Result->SetStringField(TEXT("node_class"), NewNode->GetClass()->GetName());
	Result->SetStringField(TEXT("node_title"), NewNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
	Result->SetNumberField(TEXT("x"), PosX);
	Result->SetNumberField(TEXT("y"), PosY);

	TArray<TSharedPtr<FJsonValue>> OutputPins;
	for (UEdGraphPin* Pin : NewNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("type"), PinTypeToString(Pin->PinType));
			OutputPins.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	Result->SetArrayField(TEXT("output_pins"), OutputPins);

	if (!AnimationError.IsEmpty())
	{
		Result->SetStringField(TEXT("animation_error"), AnimationError);
	}

	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// connect_animgraph_nodes
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleConnectAnimGraphNodes(
	const TSharedPtr<FJsonObject>& InParams)
{
	FString BlueprintPath, SourceNodeName, TargetNodeName;
	if (auto Err = RequireStringParam(InParams, TEXT("blueprint_path"), BlueprintPath)) return Err;
	if (auto Err = RequireStringParam(InParams, TEXT("source_node"), SourceNodeName)) return Err;
	if (auto Err = RequireStringParam(InParams, TEXT("target_node"), TargetNodeName)) return Err;

	FString SourcePinName = TEXT("Pose");
	InParams->TryGetStringField(TEXT("source_pin"), SourcePinName);
	FString TargetPinName = TEXT("Result");
	InParams->TryGetStringField(TEXT("target_pin"), TargetPinName);
	FString GraphName = TEXT("AnimGraph");
	InParams->TryGetStringField(TEXT("graph_name"), GraphName);

	UAnimBlueprint* AnimBlueprint = nullptr;
	if (auto Err = LoadAnimBlueprintChecked(BlueprintPath, AnimBlueprint)) return Err;

	UEdGraph* TargetGraph = FindGraphByName(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
	}

	UEdGraphNode* SourceNode = FindNodeByName(TargetGraph, SourceNodeName);
	UEdGraphNode* TargetNode = FindNodeByName(TargetGraph, TargetNodeName);

	if (!SourceNode)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Source node '%s' not found. Available: %s"),
				*SourceNodeName, *GetAvailableNodeNames(TargetGraph)));
	}

	if (!TargetNode)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Target node '%s' not found. Available: %s"),
				*TargetNodeName, *GetAvailableNodeNames(TargetGraph)));
	}

	UEdGraphPin* SourcePin = nullptr;
	for (UEdGraphPin* Pin : SourceNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output &&
			(Pin->PinName.ToString() == SourcePinName ||
			 Pin->PinName.ToString().Contains(SourcePinName)))
		{
			SourcePin = Pin;
			break;
		}
	}

	if (!SourcePin)
	{
		for (UEdGraphPin* Pin : SourceNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output &&
				Pin->PinType.PinCategory == TEXT("PoseLink"))
			{
				SourcePin = Pin;
				break;
			}
		}
	}

	if (!SourcePin)
	{
		TArray<FString> PinNames;
		for (UEdGraphPin* Pin : SourceNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
				PinNames.Add(FString::Printf(TEXT("%s(%s)"), *Pin->PinName.ToString(), *PinTypeToString(Pin->PinType)));
		}
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Source pin '%s' not found on '%s'. Output pins: %s"),
				*SourcePinName, *SourceNodeName, *FString::Join(PinNames, TEXT(", "))));
	}

	UEdGraphPin* TargetPin = nullptr;
	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input &&
			(Pin->PinName.ToString() == TargetPinName ||
			 Pin->PinName.ToString().Contains(TargetPinName)))
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!TargetPin)
	{
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input &&
				Pin->PinType.PinCategory == TEXT("PoseLink"))
			{
				TargetPin = Pin;
				break;
			}
		}
	}

	if (!TargetPin)
	{
		TArray<FString> PinNames;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input)
				PinNames.Add(FString::Printf(TEXT("%s(%s)"), *Pin->PinName.ToString(), *PinTypeToString(Pin->PinType)));
		}
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Target pin '%s' not found on '%s'. Input pins: %s"),
				*TargetPinName, *TargetNodeName, *FString::Join(PinNames, TEXT(", "))));
	}

	const UEdGraphSchema* Schema = TargetGraph->GetSchema();
	if (!Schema)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("No schema found for graph"));
	}

	TargetGraph->Modify();
	const bool bConnected = Schema->TryCreateConnection(SourcePin, TargetPin);
	if (!bConnected)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to connect %s.%s -> %s.%s (schema rejected connection)"),
				*SourceNode->GetName(), *SourcePin->PinName.ToString(),
				*TargetNode->GetName(), *TargetPin->PinName.ToString()));
	}

	AnimBlueprint->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("source"), FString::Printf(TEXT("%s.%s"), *SourceNode->GetName(), *SourcePin->PinName.ToString()));
	Result->SetStringField(TEXT("target"), FString::Printf(TEXT("%s.%s"), *TargetNode->GetName(), *TargetPin->PinName.ToString()));
	Result->SetBoolField(TEXT("connected"), true);
	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// set_animgraph_node_properties
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleSetAnimGraphNodeProperties(
	const TSharedPtr<FJsonObject>& InParams)
{
	// Schema convergence — accept legacy and agentic_anim_* keys.
	FString BlueprintPath, NodeToken;
	if (!TryGetStringParamAny(InParams, { TEXT("blueprint_path"), TEXT("path") }, BlueprintPath))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: blueprint_path (alias: path)"));
	}
	if (!TryGetStringParamAny(InParams, { TEXT("node_name"), TEXT("node_guid") }, NodeToken))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: node_name (alias: node_guid)"));
	}

	const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
	if (!InParams->TryGetObjectField(TEXT("properties"), PropertiesPtr) || !PropertiesPtr)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: properties (object with key-value pairs)"));
	}

	FString GraphName = TEXT("AnimGraph");
	TryGetStringParamAny(InParams, { TEXT("graph_name"), TEXT("graph") }, GraphName);

	UAnimBlueprint* AnimBlueprint = nullptr;
	if (auto Err = LoadAnimBlueprintChecked(BlueprintPath, AnimBlueprint)) return Err;

	UEdGraph* TargetGraph = FindGraphByName(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
	}

	// Try GUID first (fast-fails if not a GUID), fall back to name match.
	UEdGraphNode* ResolvedNode = FindNodeByGuid(TargetGraph, NodeToken);
	if (!ResolvedNode) { ResolvedNode = FindNodeByName(TargetGraph, NodeToken); }
	UAnimGraphNode_Base* TargetNode = Cast<UAnimGraphNode_Base>(ResolvedNode);
	if (!TargetNode)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found (tried GUID parse + name match). Available: %s"),
				*NodeToken, *GetAvailableNodeNames(TargetGraph)));
	}

	FStructProperty* NodeProp = CastField<FStructProperty>(
		TargetNode->GetClass()->FindPropertyByName(TEXT("Node")));
	if (!NodeProp)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Node has no inner 'Node' struct property"));
	}

	void* StructPtr = NodeProp->ContainerPtrToValuePtr<void>(TargetNode);
	UScriptStruct* StructType = NodeProp->Struct;

	TargetNode->Modify();

	int32 PropertiesSet = 0;
	TArray<TSharedPtr<FJsonValue>> Details;

	// Function bindings route to the WRAPPER (FMemberReference), not the inner FAnimNode.
	static const TSet<FString> WrapperOnlyProperties = {
		TEXT("UpdateFunction"),
		TEXT("BecomeRelevantFunction"),
		TEXT("InitialUpdateFunction"),
	};

	for (const auto& Pair : (*PropertiesPtr)->Values)
	{
		if (WrapperOnlyProperties.Contains(FString(*Pair.Key)))
		{
			FProperty* WrapperProp = TargetNode->GetClass()->FindPropertyByName(*Pair.Key);
			if (WrapperProp)
			{
				FString ValueStr;
				if (Pair.Value->TryGetString(ValueStr))
				{
					void* ValuePtr = WrapperProp->ContainerPtrToValuePtr<void>(TargetNode);
					WrapperProp->ImportText_Direct(*ValueStr, ValuePtr, TargetNode, PPF_None);
					PropertiesSet++;

					TSharedPtr<FJsonObject> Detail = MakeShared<FJsonObject>();
					Detail->SetStringField(TEXT("property"), Pair.Key);
					Detail->SetStringField(TEXT("value"), ValueStr);
					Detail->SetStringField(TEXT("target"), TEXT("wrapper"));
					Details.Add(MakeShared<FJsonValueObject>(Detail));
				}
			}
			continue;
		}

		// AlwaysDynamicProperties (TSet<FName>) prevents constant folding on the wrapper.
		if (Pair.Key == TEXT("AlwaysDynamicProperties"))
		{
			FProperty* ADPProp = TargetNode->GetClass()->FindPropertyByName(TEXT("AlwaysDynamicProperties"));
			if (ADPProp)
			{
				FString ValueStr;
				if (Pair.Value->TryGetString(ValueStr))
				{
					TSet<FName>* PropSet = ADPProp->ContainerPtrToValuePtr<TSet<FName>>(TargetNode);
					if (PropSet)
					{
						PropSet->Add(FName(*ValueStr));
						PropertiesSet++;

						TSharedPtr<FJsonObject> Detail = MakeShared<FJsonObject>();
						Detail->SetStringField(TEXT("property"), Pair.Key);
						Detail->SetStringField(TEXT("value"), ValueStr);
						Detail->SetStringField(TEXT("target"), TEXT("wrapper_always_dynamic"));
						Details.Add(MakeShared<FJsonValueObject>(Detail));
					}
				}
			}
			continue;
		}

		FProperty* SubProp = StructType->FindPropertyByName(*Pair.Key);
		if (!SubProp)
		{
			FProperty* WrapperProp = TargetNode->GetClass()->FindPropertyByName(*Pair.Key);
			if (WrapperProp)
			{
				FString ValueStr;
				if (Pair.Value->TryGetString(ValueStr))
				{
					void* ValuePtr = WrapperProp->ContainerPtrToValuePtr<void>(TargetNode);
					WrapperProp->ImportText_Direct(*ValueStr, ValuePtr, TargetNode, PPF_None);
					PropertiesSet++;

					TSharedPtr<FJsonObject> Detail = MakeShared<FJsonObject>();
					Detail->SetStringField(TEXT("property"), Pair.Key);
					Detail->SetStringField(TEXT("value"), ValueStr);
					Detail->SetStringField(TEXT("target"), TEXT("wrapper"));
					Details.Add(MakeShared<FJsonValueObject>(Detail));
				}
			}
			continue;
		}

		FString ValueStr = JsonValueToImportString(Pair.Value);
		if (!ValueStr.IsEmpty())
		{
			void* SubValuePtr = SubProp->ContainerPtrToValuePtr<void>(StructPtr);
			SubProp->ImportText_Direct(*ValueStr, SubValuePtr, TargetNode, PPF_None);
			PropertiesSet++;

			// Sync EdGraphPin DefaultValue (compiler reads pin defaults, not inner node)
			for (UEdGraphPin* Pin : TargetNode->Pins)
			{
				if (Pin && Pin->PinName.ToString() == *Pair.Key
					&& Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 0)
				{
					Pin->Modify();
					Pin->DefaultValue = ValueStr;
					break;
				}
			}

			TSharedPtr<FJsonObject> Detail = MakeShared<FJsonObject>();
			Detail->SetStringField(TEXT("property"), Pair.Key);
			Detail->SetStringField(TEXT("value"), ValueStr);
			Detail->SetStringField(TEXT("target"), TEXT("inner_node"));
			Details.Add(MakeShared<FJsonValueObject>(Detail));
		}
	}

	AnimBlueprint->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("node_name"), TargetNode->GetName());
	Result->SetNumberField(TEXT("properties_set"), PropertiesSet);
	Result->SetArrayField(TEXT("details"), Details);
	Result->SetStringField(TEXT("current_state"), ExportAnimNodeProperties(TargetNode));

	TSharedPtr<FJsonObject> WrapperState = ExportWrapperProperties(TargetNode);
	if (WrapperState.IsValid())
	{
		Result->SetObjectField(TEXT("wrapper_state"), WrapperState);
	}

	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// remove_animgraph_node
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleRemoveAnimGraphNode(
	const TSharedPtr<FJsonObject>& InParams)
{
	// Schema convergence — accept legacy and agentic_anim_* keys.
	FString BlueprintPath, NodeToken;
	if (!TryGetStringParamAny(InParams, { TEXT("blueprint_path"), TEXT("path") }, BlueprintPath))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: blueprint_path (alias: path)"));
	}
	if (!TryGetStringParamAny(InParams, { TEXT("node_name"), TEXT("node_guid") }, NodeToken))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: node_name (alias: node_guid)"));
	}

	FString GraphName = TEXT("AnimGraph");
	TryGetStringParamAny(InParams, { TEXT("graph_name"), TEXT("graph") }, GraphName);

	UBlueprint* Blueprint = nullptr;
	if (auto Err = LoadBlueprintChecked(BlueprintPath, Blueprint)) return Err;

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Graph '%s' not found in Blueprint"), *GraphName));
	}

	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, NodeToken);
	if (!TargetNode) { TargetNode = FindNodeByName(TargetGraph, NodeToken); }
	if (!TargetNode)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found in graph '%s' (tried GUID parse + name match)"), *NodeToken, *GraphName));
	}
	const FString ResolvedNodeName = TargetNode->GetName();
	const FString NodeClassName = TargetNode->GetClass()->GetName();
	if (NodeClassName.Contains(TEXT("Root")) || NodeClassName.Contains(TEXT("ResultNode")))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Cannot remove root/result node '%s' (class: %s)"),
				*ResolvedNodeName, *NodeClassName));
	}

	const FString NodeTitle = TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	const FString RemovedGuid = TargetNode->NodeGuid.ToString();
	FBlueprintEditorUtils::RemoveNode(Blueprint, TargetNode, true);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("removed_node"), ResolvedNodeName);
	Result->SetStringField(TEXT("removed_node_guid"), RemovedGuid);
	Result->SetStringField(TEXT("removed_node_title"), NodeTitle);
	Result->SetStringField(TEXT("removed_node_class"), NodeClassName);
	Result->SetStringField(TEXT("graph"), GraphName);
	Result->SetNumberField(TEXT("remaining_nodes"), TargetGraph->Nodes.Num());

	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// animgraph_pin
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleAnimGraphPin(
	const TSharedPtr<FJsonObject>& InParams)
{
	// Schema convergence: accept either the legacy keys (`blueprint_path`,
	// `node_name`, `graph_name`) or the converged agentic_anim_* keys
	// (`path`, `node_guid`, `graph`). Either set works; first match wins.
	FString BlueprintPath, NodeToken, Action;
	if (!TryGetStringParamAny(InParams, { TEXT("blueprint_path"), TEXT("path") }, BlueprintPath))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: blueprint_path (alias: path)"));
	}
	if (!TryGetStringParamAny(InParams, { TEXT("node_name"), TEXT("node_guid") }, NodeToken))
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: node_name (alias: node_guid)"));
	}
	if (auto Err = RequireStringParam(InParams, TEXT("action"), Action)) return Err;

	FString GraphName = TEXT("AnimGraph");
	TryGetStringParamAny(InParams, { TEXT("graph_name"), TEXT("graph") }, GraphName);

	UAnimBlueprint* AnimBlueprint = nullptr;
	if (auto Err = LoadAnimBlueprintChecked(BlueprintPath, AnimBlueprint)) return Err;

	UEdGraph* TargetGraph = FindGraphByName(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
	}

	// Try GUID first (zero-cost if not a GUID — FGuid::Parse just fails fast)
	// then fall back to editor-name lookup. Lets agentic_anim_add_nodes →
	// ue_animgraph_pin handshakes work without an intermediate summary() call.
	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, NodeToken);
	if (!TargetNode) { TargetNode = FindNodeByName(TargetGraph, NodeToken); }
	if (!TargetNode)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Node '%s' not found in graph '%s' (tried GUID parse + name match)"), *NodeToken, *GraphName));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("node_name"), TargetNode->GetName());
	Result->SetStringField(TEXT("node_guid"), TargetNode->NodeGuid.ToString());
	Result->SetStringField(TEXT("action"), Action);

	// `list`, `expose`, `hide`, `reconstruct` all operate on the FOptionalPinFromProperty
	// surface common to every UAnimGraphNode_Base subclass — the editor's right-click
	// "Show as Pin" path. Implemented before `add` / `remove` so the dynamic-pin
	// branches below remain a clean fallback for the specific blend nodes.
	UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(TargetNode);

	auto AppendCurrentPins = [&](const TSharedPtr<FJsonObject>& Out)
	{
		TArray<TSharedPtr<FJsonValue>> PinList;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (!Pin) continue;
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->GetName());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
			PinObj->SetStringField(TEXT("type"), PinTypeToString(Pin->PinType));
			PinList.Add(MakeShared<FJsonValueObject>(PinObj));
		}
		Out->SetArrayField(TEXT("pins"), PinList);
		Out->SetNumberField(TEXT("total_pins"), TargetNode->Pins.Num());
	};

	if (Action == TEXT("list"))
	{
		if (!AnimNode)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Node '%s' (class: %s) is not a UAnimGraphNode_Base — 'list' only applies to AnimGraph nodes"),
					*NodeToken, *TargetNode->GetClass()->GetName()));
		}
		TArray<TSharedPtr<FJsonValue>> OptArr;
		for (int32 i = 0; i < AnimNode->ShowPinForProperties.Num(); ++i)
		{
			const FOptionalPinFromProperty& Opt = AnimNode->ShowPinForProperties[i];
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetNumberField(TEXT("index"), i);
			Entry->SetStringField(TEXT("property_name"), Opt.PropertyName.ToString());
			Entry->SetBoolField(TEXT("show_pin"), Opt.bShowPin);
			Entry->SetBoolField(TEXT("can_toggle_visibility"), Opt.bCanToggleVisibility);
			if (!Opt.PropertyFriendlyName.IsEmpty())
			{
				Entry->SetStringField(TEXT("friendly_name"), Opt.PropertyFriendlyName);
			}
			OptArr.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Result->SetArrayField(TEXT("optional_pins"), OptArr);
		Result->SetNumberField(TEXT("optional_pin_count"), OptArr.Num());
		AppendCurrentPins(Result);
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("expose") || Action == TEXT("hide"))
	{
		if (!AnimNode)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Node '%s' (class: %s) is not a UAnimGraphNode_Base — '%s' only applies to AnimGraph nodes"),
					*NodeToken, *TargetNode->GetClass()->GetName(), *Action));
		}
		FString PropertyName;
		if (!TryGetStringParamAny(InParams, { TEXT("property_name"), TEXT("pin_name") }, PropertyName))
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Missing 'property_name' (alias: pin_name) for action '%s'. Run action='list' to see available optional pins."), *Action));
		}

		const FName PropertyFName(*PropertyName);
		int32 OptionalIdx = INDEX_NONE;
		for (int32 i = 0; i < AnimNode->ShowPinForProperties.Num(); ++i)
		{
			if (AnimNode->ShowPinForProperties[i].PropertyName == PropertyFName)
			{
				OptionalIdx = i;
				break;
			}
		}
		if (OptionalIdx == INDEX_NONE)
		{
			TArray<FString> AvailableNames;
			for (const FOptionalPinFromProperty& Opt : AnimNode->ShowPinForProperties)
			{
				AvailableNames.Add(Opt.PropertyName.ToString());
			}
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Property '%s' is not exposable on node class '%s'. Available: %s"),
					*PropertyName, *AnimNode->GetClass()->GetName(),
					AvailableNames.Num() > 0 ? *FString::Join(AvailableNames, TEXT(", ")) : TEXT("(none)")));
		}

		const FOptionalPinFromProperty& Opt = AnimNode->ShowPinForProperties[OptionalIdx];
		if (!Opt.bCanToggleVisibility)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Property '%s' visibility is locked (bCanToggleVisibility=false) — editor disallows toggling this pin"), *PropertyName));
		}

		const bool bDesiredShow = (Action == TEXT("expose"));
		AnimNode->Modify();
		// SetPinVisibility is the editor's right-click → "Show as Pin" path:
		// caches old shown pins, flips bShowPin, calls FOptionalPinManager::EvaluateOldShownPins
		// (re-syncs default pins), calls ReconstructNode(), broadcasts PinVisibilityChangedEvent.
		// Equivalent to clicking the eye icon in the Details panel.
		AnimNode->SetPinVisibility(bDesiredShow, OptionalIdx);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

		Result->SetStringField(TEXT("property_name"), PropertyName);
		Result->SetBoolField(TEXT("show_pin"), bDesiredShow);
		Result->SetNumberField(TEXT("optional_pin_index"), OptionalIdx);
		AppendCurrentPins(Result);
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("reconstruct"))
	{
		TargetNode->Modify();
		TargetNode->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
		AppendCurrentPins(Result);
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("add"))
	{
		if (UAnimGraphNode_LayeredBoneBlend* LBBNode = Cast<UAnimGraphNode_LayeredBoneBlend>(TargetNode))
		{
			LBBNode->AddPinToBlendByFilter();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
			Result->SetStringField(TEXT("node_type"), TEXT("LayeredBoneBlend"));
		}
		else if (UAnimGraphNode_BlendListByInt* BLINode = Cast<UAnimGraphNode_BlendListByInt>(TargetNode))
		{
			BLINode->AddPinToBlendList();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
			Result->SetStringField(TEXT("node_type"), TEXT("BlendListByInt"));
		}
		else if (Cast<UAnimGraphNode_BlendListByEnum>(TargetNode))
		{
			// ExposeEnumElementAsPin and VisibleEnumEntries are protected - cannot add pins via MCP
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				TEXT("BlendListByEnum 'add' is not supported via MCP (ExposeEnumElementAsPin is protected). Use the editor context menu to add enum pins. Remove is supported."));
		}
		else
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Node '%s' (class: %s) does not support dynamic-array pins. ")
					TEXT("Dynamic-array pins (action='add'): LayeredBoneBlend, BlendListByInt, BlendListByEnum. ")
					TEXT("For property-backed optional pins (LookAt, ModifyBone, AimOffset, etc.), use action='list' to inspect, then action='expose'/'hide' with property_name=<name>."),
					*NodeToken, *TargetNode->GetClass()->GetName()));
		}
	}
	else if (Action == TEXT("remove"))
	{
		FString PinName;
		if (auto Err = RequireStringParam(InParams, TEXT("pin_name"), PinName)) return Err;

		UEdGraphPin* TargetPin = nullptr;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && (Pin->GetName() == PinName || Pin->GetDisplayName().ToString() == PinName))
			{
				TargetPin = Pin;
				break;
			}
		}

		if (!TargetPin)
		{
			TArray<FString> PinNames;
			for (UEdGraphPin* Pin : TargetNode->Pins)
			{
				if (Pin) PinNames.Add(Pin->GetName());
			}
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Pin '%s' not found. Available: %s"),
					*PinName, *FString::Join(PinNames, TEXT(", "))));
		}

		if (UAnimGraphNode_LayeredBoneBlend* LBBNode = Cast<UAnimGraphNode_LayeredBoneBlend>(TargetNode))
		{
			LBBNode->RemovePinFromBlendByFilter(TargetPin);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
			Result->SetStringField(TEXT("node_type"), TEXT("LayeredBoneBlend"));
		}
		else if (UAnimGraphNode_BlendListByInt* BLINode = Cast<UAnimGraphNode_BlendListByInt>(TargetNode))
		{
			BLINode->RemovePinFromBlendList(TargetPin);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
			Result->SetStringField(TEXT("node_type"), TEXT("BlendListByInt"));
		}
		else if (UAnimGraphNode_BlendListByEnum* BLENode = Cast<UAnimGraphNode_BlendListByEnum>(TargetNode))
		{
			BLENode->RemovePinFromBlendList(TargetPin);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
			Result->SetStringField(TEXT("node_type"), TEXT("BlendListByEnum"));
		}
		else
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Node '%s' does not support pin removal"), *NodeToken));
		}

		Result->SetStringField(TEXT("removed_pin"), PinName);
	}
	else
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown action '%s'. Supported: list, expose, hide, reconstruct, add, remove"), *Action));
	}

	AppendCurrentPins(Result);
	return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
}

// =============================================================================
// blend_profile
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleBlendProfile(
	const TSharedPtr<FJsonObject>& InParams)
{
	FString Action, SkeletonPath;
	if (auto Err = RequireStringParam(InParams, TEXT("action"), Action)) return Err;
	if (auto Err = RequireStringParam(InParams, TEXT("skeleton_path"), SkeletonPath)) return Err;

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Skeleton at: %s"), *SkeletonPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("skeleton"), SkeletonPath);
	Result->SetStringField(TEXT("action"), Action);

	if (Action == TEXT("list"))
	{
		TArray<TSharedPtr<FJsonValue>> ProfileList;
		for (UBlendProfile* Profile : Skeleton->BlendProfiles)
		{
			if (!Profile) continue;
			TSharedPtr<FJsonObject> ProfObj = MakeShared<FJsonObject>();
			ProfObj->SetStringField(TEXT("name"), Profile->GetFName().ToString());
			ProfObj->SetStringField(TEXT("mode"), BlendProfileModeToString(Profile->GetMode()));
			ProfObj->SetNumberField(TEXT("entries"), Profile->GetNumBlendEntries());
			ProfileList.Add(MakeShared<FJsonValueObject>(ProfObj));
		}
		Result->SetArrayField(TEXT("profiles"), ProfileList);
		Result->SetNumberField(TEXT("count"), ProfileList.Num());
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("get"))
	{
		FString ProfileName;
		if (auto Err = RequireStringParam(InParams, TEXT("profile_name"), ProfileName)) return Err;

		UBlendProfile* Profile = Skeleton->GetBlendProfile(FName(*ProfileName));
		if (!Profile)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("BlendProfile '%s' not found on skeleton"), *ProfileName));
		}

		Result->SetStringField(TEXT("profile_name"), ProfileName);
		Result->SetStringField(TEXT("mode"), BlendProfileModeToString(Profile->GetMode()));

		TArray<TSharedPtr<FJsonValue>> Entries;
		for (int32 i = 0; i < Profile->GetNumBlendEntries(); i++)
		{
			const FBlendProfileBoneEntry& Entry = Profile->GetEntry(i);
			TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
			EntryObj->SetStringField(TEXT("bone"), Entry.BoneReference.BoneName.ToString());
			EntryObj->SetNumberField(TEXT("weight"), Entry.BlendScale);
			Entries.Add(MakeShared<FJsonValueObject>(EntryObj));
		}
		Result->SetArrayField(TEXT("entries"), Entries);
		Result->SetNumberField(TEXT("entry_count"), Profile->GetNumBlendEntries());
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("create"))
	{
		FString ProfileName;
		if (auto Err = RequireStringParam(InParams, TEXT("profile_name"), ProfileName)) return Err;

		UBlendProfile* Profile = Skeleton->GetBlendProfile(FName(*ProfileName));
		if (Profile)
		{
			Result->SetStringField(TEXT("status"), TEXT("already_exists"));
		}
		else
		{
			Profile = Skeleton->CreateNewBlendProfile(FName(*ProfileName));
			if (!Profile)
			{
				return FAgenticMCPCommonUtils::CreateErrorResponse(
					FString::Printf(TEXT("Failed to create BlendProfile '%s'"), *ProfileName));
			}
			Result->SetStringField(TEXT("status"), TEXT("created"));
		}

		FString ModeStr;
		if (InParams->TryGetStringField(TEXT("mode"), ModeStr))
		{
			if (ModeStr == TEXT("BlendMask"))
				Profile->Mode = EBlendProfileMode::BlendMask;
			else if (ModeStr == TEXT("WeightFactor"))
				Profile->Mode = EBlendProfileMode::WeightFactor;
			else if (ModeStr == TEXT("TimeFactor"))
				Profile->Mode = EBlendProfileMode::TimeFactor;
		}

		Result->SetStringField(TEXT("profile_name"), ProfileName);

		const TSharedPtr<FJsonObject>* BonesPtr = nullptr;
		if (InParams->TryGetObjectField(TEXT("bones"), BonesPtr) && BonesPtr)
		{
			bool bRecursive = false;
			InParams->TryGetBoolField(TEXT("recursive"), bRecursive);

			int32 BonesSet = 0;
			for (const auto& Pair : (*BonesPtr)->Values)
			{
				double Weight;
				if (Pair.Value->TryGetNumber(Weight))
				{
					Profile->SetBoneBlendScale(FName(*Pair.Key), (float)Weight, bRecursive, true);
					BonesSet++;
				}
			}
			Result->SetNumberField(TEXT("bones_set"), BonesSet);
		}

		Skeleton->Modify();
		Skeleton->MarkPackageDirty();
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("set_bones"))
	{
		FString ProfileName;
		if (auto Err = RequireStringParam(InParams, TEXT("profile_name"), ProfileName)) return Err;

		UBlendProfile* Profile = Skeleton->GetBlendProfile(FName(*ProfileName));
		if (!Profile)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("BlendProfile '%s' not found"), *ProfileName));
		}

		const TSharedPtr<FJsonObject>* BonesPtr = nullptr;
		if (!InParams->TryGetObjectField(TEXT("bones"), BonesPtr) || !BonesPtr)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(TEXT("Missing required parameter: bones (JSON object)"));
		}

		bool bRecursive = false;
		InParams->TryGetBoolField(TEXT("recursive"), bRecursive);

		int32 BonesSet = 0;
		for (const auto& Pair : (*BonesPtr)->Values)
		{
			double Weight;
			if (Pair.Value->TryGetNumber(Weight))
			{
				Profile->SetBoneBlendScale(FName(*Pair.Key), (float)Weight, bRecursive, true);
				BonesSet++;
			}
		}

		Result->SetStringField(TEXT("profile_name"), ProfileName);
		Result->SetNumberField(TEXT("bones_set"), BonesSet);

		Skeleton->Modify();
		Skeleton->MarkPackageDirty();
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	return FAgenticMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown action '%s'. Supported: create, set_bones, get, list"), *Action));
}

// =============================================================================
// state_machine
// =============================================================================

TSharedPtr<FJsonObject> FAgenticMCPGraphAnalysis::HandleStateMachine(
	const TSharedPtr<FJsonObject>& InParams)
{
	FString Action, BlueprintPath;
	if (auto Err = RequireStringParam(InParams, TEXT("action"), Action)) return Err;
	if (auto Err = RequireStringParam(InParams, TEXT("blueprint_path"), BlueprintPath)) return Err;

	UAnimBlueprint* AnimBlueprint = nullptr;
	if (auto Err = LoadAnimBlueprintChecked(BlueprintPath, AnimBlueprint)) return Err;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("action"), Action);

	if (Action == TEXT("create"))
	{
		FString GraphName = TEXT("AnimGraph");
		InParams->TryGetStringField(TEXT("graph_name"), GraphName);

		UEdGraph* TargetGraph = FindGraphByName(AnimBlueprint, GraphName);
		if (!TargetGraph)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
		}

		int32 PosX = 0, PosY = 0;
		InParams->TryGetNumberField(TEXT("x"), PosX);
		InParams->TryGetNumberField(TEXT("y"), PosY);

		UAnimGraphNode_StateMachine* SMNode = NewObject<UAnimGraphNode_StateMachine>(TargetGraph);
		SMNode->CreateNewGuid();
		SMNode->PostPlacedNewNode();
		SMNode->AllocateDefaultPins();
		SMNode->NodePosX = PosX;
		SMNode->NodePosY = PosY;
		TargetGraph->AddNode(SMNode, false, false);

		FString SMName;
		if (InParams->TryGetStringField(TEXT("name"), SMName))
		{
			if (SMNode->EditorStateMachineGraph)
			{
				FBlueprintEditorUtils::RenameGraph(SMNode->EditorStateMachineGraph, *SMName);
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

		Result->SetStringField(TEXT("node_name"), SMNode->GetName());
		Result->SetStringField(TEXT("graph"), GraphName);
		if (SMNode->EditorStateMachineGraph)
		{
			Result->SetStringField(TEXT("sm_graph"), SMNode->EditorStateMachineGraph->GetName());
		}
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	FString SMNodeName;
	if (auto Err = RequireStringParam(InParams, TEXT("state_machine"), SMNodeName)) return Err;

	FString GraphName = TEXT("AnimGraph");
	InParams->TryGetStringField(TEXT("graph_name"), GraphName);

	UEdGraph* ParentGraph = FindGraphByName(AnimBlueprint, GraphName);
	if (!ParentGraph)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
	}

	UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(FindNodeByName(ParentGraph, SMNodeName));

	if (!SMNode || !SMNode->EditorStateMachineGraph)
	{
		return FAgenticMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("State machine node '%s' not found or has no graph"), *SMNodeName));
	}

	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;

	if (Action == TEXT("add_state"))
	{
		FString StateName;
		if (auto Err = RequireStringParam(InParams, TEXT("state_name"), StateName)) return Err;

		int32 PosX = 300, PosY = 0;
		InParams->TryGetNumberField(TEXT("x"), PosX);
		InParams->TryGetNumberField(TEXT("y"), PosY);

		UAnimStateNode* StateNode = NewObject<UAnimStateNode>(SMGraph);
		StateNode->CreateNewGuid();
		StateNode->PostPlacedNewNode();
		StateNode->AllocateDefaultPins();
		StateNode->NodePosX = PosX;
		StateNode->NodePosY = PosY;
		SMGraph->AddNode(StateNode, false, false);

		StateNode->Modify();
		StateNode->bCanRenameNode = true;
		FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, *StateName);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

		Result->SetStringField(TEXT("state_name"), StateNode->GetName());
		Result->SetStringField(TEXT("state_title"), StateNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		if (StateNode->BoundGraph)
		{
			Result->SetStringField(TEXT("state_graph"), StateNode->BoundGraph->GetName());
		}
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("add_transition"))
	{
		FString SourceStateName, TargetStateName;
		if (auto Err = RequireStringParam(InParams, TEXT("source_state"), SourceStateName)) return Err;
		if (auto Err = RequireStringParam(InParams, TEXT("target_state"), TargetStateName)) return Err;
		UAnimStateNodeBase* SourceState = nullptr;
		UAnimStateNodeBase* TargetState = nullptr;
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			if (!Node) continue;
			UAnimStateNodeBase* StateBase = Cast<UAnimStateNodeBase>(Node);
			if (!StateBase) continue;
			if (Node->GetName() == SourceStateName || Node->GetName().Contains(SourceStateName))
			{
				SourceState = StateBase;
			}
			if (Node->GetName() == TargetStateName || Node->GetName().Contains(TargetStateName))
			{
				TargetState = StateBase;
			}
		}

		if (!SourceState && SMGraph->EntryNode)
		{
			if (SMGraph->EntryNode->GetName() == SourceStateName ||
				SMGraph->EntryNode->GetName().Contains(SourceStateName) ||
				SourceStateName == TEXT("Entry"))
			{
				SourceState = nullptr;
				if (TargetState && SMGraph->EntryNode->GetOutputPin() && TargetState->GetInputPin())
				{
					SMGraph->EntryNode->GetOutputPin()->MakeLinkTo(TargetState->GetInputPin());
					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

					Result->SetStringField(TEXT("source"), TEXT("Entry"));
					Result->SetStringField(TEXT("target"), TargetState->GetName());
					Result->SetBoolField(TEXT("connected"), true);
					return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
				}
			}
		}

		if (!SourceState)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Source state '%s' not found in state machine"), *SourceStateName));
		}
		if (!TargetState)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Target state '%s' not found in state machine"), *TargetStateName));
		}

		UAnimStateTransitionNode* TransNode = NewObject<UAnimStateTransitionNode>(SMGraph);
		TransNode->CreateNewGuid();
		TransNode->PostPlacedNewNode();
		TransNode->AllocateDefaultPins();
		TransNode->NodePosX = (SourceState->NodePosX + TargetState->NodePosX) / 2;
		TransNode->NodePosY = (SourceState->NodePosY + TargetState->NodePosY) / 2;
		SMGraph->AddNode(TransNode, false, false);

		TransNode->CreateConnections(SourceState, TargetState);

		double Crossfade = 0.2;
		if (InParams->TryGetNumberField(TEXT("crossfade"), Crossfade))
		{
			TransNode->CrossfadeDuration = (float)Crossfade;
		}

		FString BlendModeStr;
		if (InParams->TryGetStringField(TEXT("blend_mode"), BlendModeStr))
		{
			if (BlendModeStr == TEXT("Linear"))
				TransNode->BlendMode = EAlphaBlendOption::Linear;
			else if (BlendModeStr == TEXT("Cubic") || BlendModeStr == TEXT("HermiteCubic"))
				TransNode->BlendMode = EAlphaBlendOption::HermiteCubic;
			else if (BlendModeStr == TEXT("Inertialization"))
				TransNode->BlendMode = EAlphaBlendOption::Linear; // Inertialization set via type
		}

		bool bAutomatic = false;
		if (InParams->TryGetBoolField(TEXT("automatic"), bAutomatic))
		{
			TransNode->bAutomaticRuleBasedOnSequencePlayerInState = bAutomatic;
		}

		double AutoTime = 0.0;
		if (InParams->TryGetNumberField(TEXT("automatic_time"), AutoTime))
		{
			TransNode->AutomaticRuleTriggerTime = (float)AutoTime;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

		Result->SetStringField(TEXT("transition_name"), TransNode->GetName());
		Result->SetStringField(TEXT("source"), SourceState->GetName());
		Result->SetStringField(TEXT("target"), TargetState->GetName());
		Result->SetNumberField(TEXT("crossfade"), TransNode->CrossfadeDuration);
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("set_transition"))
	{
		FString TransitionName;
		if (auto Err = RequireStringParam(InParams, TEXT("transition_name"), TransitionName)) return Err;

		UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(FindNodeByName(SMGraph, TransitionName));

		if (!TransNode)
		{
			return FAgenticMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Transition '%s' not found"), *TransitionName));
		}

		double Crossfade;
		if (InParams->TryGetNumberField(TEXT("crossfade"), Crossfade))
		{
			TransNode->CrossfadeDuration = (float)Crossfade;
		}

		int32 Priority;
		if (InParams->TryGetNumberField(TEXT("priority"), Priority))
		{
			TransNode->PriorityOrder = Priority;
		}

		bool bAutomatic;
		if (InParams->TryGetBoolField(TEXT("automatic"), bAutomatic))
		{
			TransNode->bAutomaticRuleBasedOnSequencePlayerInState = bAutomatic;
		}

		double AutoTime;
		if (InParams->TryGetNumberField(TEXT("automatic_time"), AutoTime))
		{
			TransNode->AutomaticRuleTriggerTime = (float)AutoTime;
		}

		bool bBidirectional;
		if (InParams->TryGetBoolField(TEXT("bidirectional"), bBidirectional))
		{
			TransNode->Bidirectional = bBidirectional;
		}

		FBlueprintEditorUtils::MarkBlueprintAsModified(AnimBlueprint);

		Result->SetStringField(TEXT("transition_name"), TransNode->GetName());
		Result->SetNumberField(TEXT("crossfade"), TransNode->CrossfadeDuration);
		Result->SetNumberField(TEXT("priority"), TransNode->PriorityOrder);
		Result->SetBoolField(TEXT("automatic"), TransNode->bAutomaticRuleBasedOnSequencePlayerInState);
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	if (Action == TEXT("list"))
	{
		TArray<TSharedPtr<FJsonValue>> States;
		TArray<TSharedPtr<FJsonValue>> Transitions;

		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			if (!Node) continue;

			if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
			{
				TSharedPtr<FJsonObject> StateObj = MakeShared<FJsonObject>();
				StateObj->SetStringField(TEXT("name"), StateNode->GetName());
				StateObj->SetStringField(TEXT("title"), StateNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
				StateObj->SetNumberField(TEXT("x"), StateNode->NodePosX);
				StateObj->SetNumberField(TEXT("y"), StateNode->NodePosY);
				if (StateNode->BoundGraph)
				{
					StateObj->SetStringField(TEXT("graph"), StateNode->BoundGraph->GetName());
				}
				States.Add(MakeShared<FJsonValueObject>(StateObj));
			}
			else if (UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node))
			{
				TSharedPtr<FJsonObject> TransObj = MakeShared<FJsonObject>();
				TransObj->SetStringField(TEXT("name"), TransNode->GetName());
				if (TransNode->GetPreviousState())
					TransObj->SetStringField(TEXT("source"), TransNode->GetPreviousState()->GetName());
				if (TransNode->GetNextState())
					TransObj->SetStringField(TEXT("target"), TransNode->GetNextState()->GetName());
				TransObj->SetNumberField(TEXT("crossfade"), TransNode->CrossfadeDuration);
				TransObj->SetNumberField(TEXT("priority"), TransNode->PriorityOrder);
				TransObj->SetBoolField(TEXT("automatic"), TransNode->bAutomaticRuleBasedOnSequencePlayerInState);
				TransObj->SetBoolField(TEXT("bidirectional"), TransNode->Bidirectional);
				Transitions.Add(MakeShared<FJsonValueObject>(TransObj));
			}
		}

		// Entry node info
		if (SMGraph->EntryNode)
		{
			UEdGraphNode* EntryTarget = SMGraph->EntryNode->GetOutputNode();
			Result->SetStringField(TEXT("entry_state"),
				EntryTarget ? EntryTarget->GetName() : TEXT("(none)"));
		}

		Result->SetArrayField(TEXT("states"), States);
		Result->SetArrayField(TEXT("transitions"), Transitions);
		Result->SetNumberField(TEXT("state_count"), States.Num());
		Result->SetNumberField(TEXT("transition_count"), Transitions.Num());
		return FAgenticMCPCommonUtils::CreateSuccessResponse(Result);
	}

	return FAgenticMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown action '%s'. Supported: create, add_state, add_transition, set_transition, list"), *Action));
}
