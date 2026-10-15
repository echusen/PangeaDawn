// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class AGENTICMCP_API FAgenticMCPBlueprintCommands
{
public:
	FAgenticMCPBlueprintCommands() = default;

	TSharedPtr<FJsonObject> HandleCommand(const FString& InCommandType, const TSharedPtr<FJsonObject>& InParams);

private:
	// Pre-existing handlers
	TSharedPtr<FJsonObject> HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& InParams);
	TSharedPtr<FJsonObject> HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& InParams);
	TSharedPtr<FJsonObject> HandleSetMeshMaterialColor(const TSharedPtr<FJsonObject>& InParams);
	// ─── BP-Expert Phase 1: Read-only introspection ─────────────────────

	/** Full topology dump: parent, components, variables, functions, events, interfaces, delegates, refs */
	TSharedPtr<FJsonObject> HandleBpInspect(const TSharedPtr<FJsonObject>& InParams);

	/** Quick overview: parent class, status, top-level counts */
	TSharedPtr<FJsonObject> HandleBpSummary(const TSharedPtr<FJsonObject>& InParams);

	/** Exec-chain only walk of a named function's flow */
	TSharedPtr<FJsonObject> HandleBpGetFunctionFlow(const TSharedPtr<FJsonObject>& InParams);

	/** Walk parent chain, list virtual UFunctions overridable from BP */
	TSharedPtr<FJsonObject> HandleBpListOverridableFunctions(const TSharedPtr<FJsonObject>& InParams);

	/** SCS components + inherited components from parent class */
	TSharedPtr<FJsonObject> HandleBpGetComponents(const TSharedPtr<FJsonObject>& InParams);

	/** Implemented interfaces + their function lists */
	TSharedPtr<FJsonObject> HandleBpGetInterfaces(const TSharedPtr<FJsonObject>& InParams);

	// ─── BP-Expert Phase 2: Bulk graph editing ──────────────────────────

	/** Bulk add K2 nodes to a graph; returns each node's UE-assigned object name. */
	TSharedPtr<FJsonObject> HandleBpAddNodes(const TSharedPtr<FJsonObject>& InParams);

	/** Reflection-based property setter for an existing K2 node, plus pin-default mutator. */
	TSharedPtr<FJsonObject> HandleBpConfigureNode(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk wire pins via Schema->TryCreateConnection (type-checked, with coercion). */
	TSharedPtr<FJsonObject> HandleBpLinkNodes(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk remove nodes; preserves graph terminators (FunctionEntry/Result) by default. */
	TSharedPtr<FJsonObject> HandleBpRemoveNodes(const TSharedPtr<FJsonObject>& InParams);

	/** Auto-arrange a graph using exec-flow topological sort + data-input layering. */
	TSharedPtr<FJsonObject> HandleBpArrangeGraph(const TSharedPtr<FJsonObject>& InParams);

	/** Remove all user nodes; keep entry/result terminators unless override. */
	TSharedPtr<FJsonObject> HandleBpClearGraph(const TSharedPtr<FJsonObject>& InParams);

	/** Composite: declarative full-graph build (add → configure → link → optional arrange). */
	TSharedPtr<FJsonObject> HandleBpBuildGraph(const TSharedPtr<FJsonObject>& InParams);

	// ─── BP-Expert Phase 3: Variables, functions, dispatchers, RPCs ─────

	/** Bulk add member variables (typed). Each entry: name, type{...}, optional default, replication, flags, category, tooltip. */
	TSharedPtr<FJsonObject> HandleBpAddVariables(const TSharedPtr<FJsonObject>& InParams);

	/** Rename a member variable; references update via FBlueprintEditorUtils::RenameMemberVariable. */
	TSharedPtr<FJsonObject> HandleBpRenameVariable(const TSharedPtr<FJsonObject>& InParams);

	/** Set a variable's literal default value (FBPVariableDescription::DefaultValue). */
	TSharedPtr<FJsonObject> HandleBpSetVariableDefault(const TSharedPtr<FJsonObject>& InParams);

	/** Set instance-editable / expose-on-spawn / private / transient / save-game flags + category + tooltip metadata. */
	TSharedPtr<FJsonObject> HandleBpSetVariableFlags(const TSharedPtr<FJsonObject>& InParams);

	/** Add a new function graph with optional inputs/outputs (each carries FEdGraphPinType). */
	TSharedPtr<FJsonObject> HandleBpAddFunction(const TSharedPtr<FJsonObject>& InParams);

	/** Set Public / Protected / Private access modifier on a function. */
	TSharedPtr<FJsonObject> HandleBpSetFunctionAccess(const TSharedPtr<FJsonObject>& InParams);

	/** Toggle FUNC_BlueprintPure on a function. */
	TSharedPtr<FJsonObject> HandleBpSetFunctionPure(const TSharedPtr<FJsonObject>& InParams);

	/** Set RPC type (server/client/multicast) + reliable + with-validation flags on a function. */
	TSharedPtr<FJsonObject> HandleBpSetFunctionReplication(const TSharedPtr<FJsonObject>& InParams);

	/** Implement a Blueprint Interface (resolves short class names + dotted paths). */
	TSharedPtr<FJsonObject> HandleBpImplementInterface(const TSharedPtr<FJsonObject>& InParams);

	/** Remove a Blueprint Interface (preserve_functions flag controls whether implementing graphs survive). */
	TSharedPtr<FJsonObject> HandleBpRemoveInterface(const TSharedPtr<FJsonObject>& InParams);

	// ─── BP-Expert Phase 4: SCS / Components ────────────────────────────

	/** Bulk add components to the SCS. Each entry: name, class, optional parent + attach_socket + transform. */
	TSharedPtr<FJsonObject> HandleBpAddComponents(const TSharedPtr<FJsonObject>& InParams);

	/** Bulk remove components from the SCS. Refuses to remove the scene root unless `promote_to_root` is given; refuses to remove a node with children unless `cascade=true`. */
	TSharedPtr<FJsonObject> HandleBpRemoveComponents(const TSharedPtr<FJsonObject>& InParams);

	/** Re-parent an SCS node onto a new parent (or promote to root via parent=""). Optional attach_socket on USceneComponent children. */
	TSharedPtr<FJsonObject> HandleBpAttachComponent(const TSharedPtr<FJsonObject>& InParams);

	/** Rename an SCS component variable. Defers to FBlueprintEditorUtils::RenameComponentMemberVariable so references fix up. */
	TSharedPtr<FJsonObject> HandleBpRenameComponent(const TSharedPtr<FJsonObject>& InParams);

	/** Deep dump of a single SCS component: class, parent, socket, is_root, transform, mobility, collision, plus FProperty snapshot on the template. */
	TSharedPtr<FJsonObject> HandleBpGetComponentDetails(const TSharedPtr<FJsonObject>& InParams);

	/** Reflection-based bulk property setter for a component's SCS template. Same dot-path surface as bp_configure_node. */
	TSharedPtr<FJsonObject> HandleBpSetComponentProperty(const TSharedPtr<FJsonObject>& InParams);

	/** Typed collision setter — profile + collision_enabled + per-channel responses in a single call. */
	TSharedPtr<FJsonObject> HandleBpSetComponentCollision(const TSharedPtr<FJsonObject>& InParams);

	/** Set USceneComponent::Mobility (Static / Stationary / Movable). */
	TSharedPtr<FJsonObject> HandleBpSetComponentMobility(const TSharedPtr<FJsonObject>& InParams);

	/** Set the SCS template's relative transform + optional attach_socket. */
	TSharedPtr<FJsonObject> HandleBpSetComponentTransform(const TSharedPtr<FJsonObject>& InParams);

	// ─── BP-Expert Phase 6: Compilation, validation, analysis ───────────

	/** Structural diff over two Blueprints: parent / variables / components / functions / interfaces. */
	TSharedPtr<FJsonObject> HandleBpDiff(const TSharedPtr<FJsonObject>& InParams);

	/** Swap one BP class for another across the current editor world's actors. */
	TSharedPtr<FJsonObject> HandleBpReplaceReferences(const TSharedPtr<FJsonObject>& InParams);

	/** Objective complexity metrics: node count, branch count, cyclomatic complexity, latent / tick / async counts. */
	TSharedPtr<FJsonObject> HandleBpEstimateComplexity(const TSharedPtr<FJsonObject>& InParams);

	/** BP-scoped search across variables, functions, node titles, pin defaults. */
	TSharedPtr<FJsonObject> HandleBpSearch(const TSharedPtr<FJsonObject>& InParams);
};
