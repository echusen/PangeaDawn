// Copyright Insodimension Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "EdGraph/EdGraphPin.h"

// Forward declarations
class UBlueprint;
class UAnimBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UAnimGraphNode_StateMachineBase;
class UAnimStateNode;
class UAnimStateTransitionNode;
class UAnimationStateMachineGraph;
class USkeleton;

/**
 * Handler class for AnimGraph MCP commands: Animation Blueprint introspection,
 * AnimGraph node authoring, state machines and skeleton blend profiles.
 */
class AGENTICMCP_API FAgenticMCPGraphAnalysis
{
public:
	/** Route a command to the appropriate handler */
	TSharedPtr<FJsonObject> HandleCommand(const FString& InCommandType, const TSharedPtr<FJsonObject>& InParams);

private:
	// Top-level Command Handlers

	/** AnimBP-specific: state machines, states, transitions, blend nodes */
	TSharedPtr<FJsonObject> HandleAnalyzeAnimGraph(const TSharedPtr<FJsonObject>& InParams);

	/** Add a node to an AnimGraph (SequenceEvaluator, SequencePlayer, etc.) */
	TSharedPtr<FJsonObject> HandleAddAnimGraphNode(const TSharedPtr<FJsonObject>& InParams);

	/** Connect two AnimGraph nodes via their pose pins */
	TSharedPtr<FJsonObject> HandleConnectAnimGraphNodes(const TSharedPtr<FJsonObject>& InParams);

	/** Set properties on an existing AnimGraph node's inner FAnimNode struct */
	TSharedPtr<FJsonObject> HandleSetAnimGraphNodeProperties(const TSharedPtr<FJsonObject>& InParams);

	/** Remove a node from an AnimGraph (or any Blueprint graph) */
	TSharedPtr<FJsonObject> HandleRemoveAnimGraphNode(const TSharedPtr<FJsonObject>& InParams);

	/** Add or remove dynamic pins on LBB, BlendListByInt, BlendListByEnum nodes */
	TSharedPtr<FJsonObject> HandleAnimGraphPin(const TSharedPtr<FJsonObject>& InParams);

	/** Create, edit, read blend profiles on skeletons */
	TSharedPtr<FJsonObject> HandleBlendProfile(const TSharedPtr<FJsonObject>& InParams);

	/** Create and manage state machines: create SM, add states, add transitions */
	TSharedPtr<FJsonObject> HandleStateMachine(const TSharedPtr<FJsonObject>& InParams);

	// Blueprint Structure Helpers

	/** Describe all user-defined variables */
	TArray<TSharedPtr<FJsonValue>> CollectVariables(UBlueprint* InBlueprint);

	/** Describe all event graphs with their event entry points */
	TArray<TSharedPtr<FJsonValue>> CollectEventGraphs(UBlueprint* InBlueprint);

	// AnimGraph Helpers

	/** Describe a state machine node and all its states/transitions */
	TSharedPtr<FJsonObject> DescribeStateMachine(UAnimGraphNode_StateMachineBase* InStateMachineNode);

	/** Describe a single animation state */
	TSharedPtr<FJsonObject> DescribeAnimState(UAnimStateNode* InStateNode);

	/** Describe a transition between states */
	TSharedPtr<FJsonObject> DescribeAnimTransition(UAnimStateTransitionNode* InTransitionNode);

	// Utility

	/** Convert a pin type to a human-readable string */
	static FString PinTypeToString(const FEdGraphPinType& InPinType);

	/** Find a named graph within a Blueprint (searches all graph arrays) */
	static UEdGraph* FindGraphByName(UBlueprint* InBlueprint, const FString& InGraphName);

	/** Export the inner FAnimNode struct properties as text (Details panel values) */
	static FString ExportAnimNodeProperties(UEdGraphNode* InNode);

	/** Export wrapper AnimGraphNode properties (FMemberReference function bindings, etc.)
	 *  Returns a JSON object with key wrapper-level properties that aren't in the inner FAnimNode. */
	static TSharedPtr<FJsonObject> ExportWrapperProperties(UEdGraphNode* InNode);
};
