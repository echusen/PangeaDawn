// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AscentAnimGraphToolset.generated.h"

/// Animation Blueprint authoring tools — AnimGraph inspection and nodes, state machines, and skeleton
/// blend profiles. Epic's toolsets cover Control Rig and Sequencer but not AnimGraphs.
/// All tools return a JSON string: {"status":"success","success":true,...}
/// on success or {"status":"error","success":false,"error":...} on failure.
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentAnimGraphToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:

    /**
     * Describes an Animation Blueprint: target skeleton, variables, event graphs, and the AnimGraph's
     * root nodes, blend nodes (with their FAnimNode properties), and state machines with states and transitions.
     * @param Path Full Animation Blueprint asset path, e.g. "/Game/Characters/ABP_Hero".
     * @return JSON with "anim_graph": {"root_nodes","blend_nodes","state_machines"} plus variables and event graphs.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString Analyze(const FString& Path);

    /**
     * Adds a node to an AnimGraph.
     * @param Path Full Animation Blueprint asset path.
     * @param Graph Graph name, e.g. a state's bound graph. Pass an empty string for "AnimGraph".
     * @param NodeType "SequencePlayer", "SequenceEvaluator", "SaveCachedPose", "UseCachedPose", or any
     *   AnimGraphNode_* class name without the prefix, e.g. "LayeredBoneBlend", "BlendListByInt", "TwoWayBlend".
     * @param X Node X position.
     * @param Y Node Y position.
     * @param OptionsJson Optional JSON object. Pass an empty string to skip. Keys: "animation" (animation
     *   asset path for asset players), "cache_name" (SaveCachedPose name, or the cache a UseCachedPose reads),
     *   "update_function" / "become_relevant_function" / "initial_update_function" (AnimBP function names),
     *   "node_properties" (object of inner FAnimNode properties, values in Unreal import-text form).
     * @return JSON with node_name, node_guid, node_class and output pins.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString AddNode(const FString& Path, const FString& Graph, const FString& NodeType,
        int32 X, int32 Y, const FString& OptionsJson);

    /**
     * Connects the pose output of one AnimGraph node to a pose input of another.
     * @param Path Full Animation Blueprint asset path.
     * @param Graph Graph name. Pass an empty string for "AnimGraph".
     * @param SourceNode Source node name (exact or substring match).
     * @param SourcePin Source output pin. Pass an empty string for "Pose".
     * @param TargetNode Target node name (exact or substring match), e.g. the output pose root node.
     * @param TargetPin Target input pin, e.g. "BlendPose_0". Pass an empty string for "Result".
     * @return JSON with the connected pins.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString ConnectNodes(const FString& Path, const FString& Graph, const FString& SourceNode,
        const FString& SourcePin, const FString& TargetNode, const FString& TargetPin);

    /**
     * Sets properties on an AnimGraph node. Keys are inner FAnimNode property names (values in Unreal
     * import-text form); "UpdateFunction", "BecomeRelevantFunction", "InitialUpdateFunction" and
     * "AlwaysDynamicProperties" target the node wrapper instead.
     * @param Path Full Animation Blueprint asset path.
     * @param Graph Graph name. Pass an empty string for "AnimGraph".
     * @param Node Node name or node GUID.
     * @param PropertiesJson JSON object of property name to value, e.g. {"bLoopAnimation":true,"PlayRate":1.2}.
     * @return JSON with the properties applied and where each one was written.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString SetNodeProperties(const FString& Path, const FString& Graph, const FString& Node,
        const FString& PropertiesJson);

    /**
     * Removes a node from an AnimGraph. The output pose root node cannot be removed.
     * @param Path Full Animation Blueprint asset path.
     * @param Graph Graph name. Pass an empty string for "AnimGraph".
     * @param Node Node name or node GUID.
     * @return JSON with the removed node and the remaining node count.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString RemoveNode(const FString& Path, const FString& Graph, const FString& Node);

    /**
     * Edits the pins of an AnimGraph node.
     * @param Path Full Animation Blueprint asset path.
     * @param Graph Graph name. Pass an empty string for "AnimGraph".
     * @param Node Node name or node GUID.
     * @param Action "list" (optional property pins and current pins), "expose" / "hide" (show a property as a
     *   pin, like the Details panel eye icon), "reconstruct", "add" (new blend pin on LayeredBoneBlend or
     *   BlendListByInt), or "remove" (blend pin on LayeredBoneBlend, BlendListByInt or BlendListByEnum).
     * @param PinName Property name for "expose" / "hide", pin name for "remove". Pass an empty string otherwise.
     * @return JSON with the node's pins after the edit.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString EditPins(const FString& Path, const FString& Graph, const FString& Node,
        const FString& Action, const FString& PinName);

    /**
     * Reads or edits blend profiles on a skeleton.
     * @param SkeletonPath Full skeleton asset path.
     * @param Action "list", "get", "create" (also updates an existing profile), or "set_bones".
     * @param ProfileName Blend profile name. Required for every action except "list".
     * @param Mode "TimeFactor", "WeightFactor", or "BlendMask"; used by "create". Pass an empty string to keep the default.
     * @param BonesJson JSON object of bone name to blend scale, e.g. {"spine_01":1.0}. Required for
     *   "set_bones", optional for "create". Pass an empty string to skip.
     * @param bRecursive Whether each bone scale also applies to all of its child bones.
     * @return JSON with the profile mode and bone entries, or the number of bones set.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString BlendProfile(const FString& SkeletonPath, const FString& Action, const FString& ProfileName,
        const FString& Mode, const FString& BonesJson, bool bRecursive);

    /**
     * Creates and edits AnimGraph state machines.
     * @param Path Full Animation Blueprint asset path.
     * @param Graph Graph that contains (or will contain) the state machine node. Pass an empty string for "AnimGraph".
     * @param Action "create", "add_state", "add_transition", "set_transition", or "list".
     * @param StateMachineNode State machine node name, as returned by "create". Required for every action except "create".
     * @param OptionsJson Optional JSON object. Pass an empty string to skip. Keys per action:
     *   create: "name", "x", "y". add_state: "state_name" (required), "x", "y".
     *   add_transition: "source_state" (required; "Entry" links the entry point), "target_state" (required),
     *   "crossfade", "blend_mode" ("Linear" or "HermiteCubic"), "automatic", "automatic_time".
     *   set_transition: "transition_name" (required), "crossfade", "priority", "automatic", "automatic_time", "bidirectional".
     * @return JSON with the created or edited node names, or the states and transitions for "list".
     */
    UFUNCTION(meta = (AICallable), Category = "AscentAnimGraph")
    static FString StateMachine(const FString& Path, const FString& Graph, const FString& Action,
        const FString& StateMachineNode, const FString& OptionsJson);
};
