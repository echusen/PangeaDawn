// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Commands/UECommandBase.h"

class FAgenticMCPGraphAnalysis;

/**
 * Animation Blueprint authoring command surface. Adapter onto the mirrored AgenticUEBridge
 * FAgenticMCPGraphAnalysis handlers, which Epic's toolsets have no equivalent for.
 *
 * Wire formats:
 *   ue_anim_analyze             - AnimGraph, state machines, blend nodes and variables of an AnimBP
 *   ue_anim_add_node            - add an AnimGraph node
 *   ue_anim_connect_nodes       - connect two AnimGraph nodes via pose pins
 *   ue_anim_set_node_properties - set inner FAnimNode properties
 *   ue_anim_remove_node         - remove an AnimGraph node
 *   ue_anim_pin                 - list/expose/hide/add/remove/reconstruct node pins
 *   ue_anim_blend_profile       - list/get/create/set_bones on skeleton blend profiles
 *   ue_anim_state_machine       - create/add_state/add_transition/set_transition/list
 */
class FUEAnimGraphCommands : public FUECommandBase
{
public:
    explicit FUEAnimGraphCommands(UAscentAIAgentSubsystem* InOwner);
    virtual ~FUEAnimGraphCommands();

    virtual bool IsCommand(const FString& CommandType) const override;
    virtual FString HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params) override;

private:
    TUniquePtr<FAgenticMCPGraphAnalysis> GraphAnalysisCommands;
};
