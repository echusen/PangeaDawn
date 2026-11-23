// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AscentGraphToolset.generated.h"

/// Blueprint graph authoring gaps that Epic's toolsets leave open.
/// Make / Break / "Set members in" struct nodes hide every member behind a checkbox that only the
/// Details panel can tick, so a graph built over MCP cannot reach a single field of a struct.
/// These tools drive that same list.
/// All tools return a JSON string: {"status":"success","success":true,...}
/// on success or {"status":"error","success":false,"error":...} on failure.
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentGraphToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Lists the members of a Make Struct, Break Struct or "Set members in" node and whether each one
     * currently has a pin. Call this before SetStructMemberPins to learn the exact member names.
     * @param NodePath Full node object path, e.g.
     *   "/Game/UI/WBP_Bar.WBP_Bar:InitStyle.K2Node_SetFieldsInStruct_0".
     * @return JSON with "struct" and "members": [{"name","shown","can_toggle"}].
     */
    UFUNCTION(meta = (AICallable), Category = "AscentGraph")
    static FString ListStructMemberPins(const FString& NodePath);

    /**
     * Shows or hides member pins on a Make Struct, Break Struct or "Set members in" node, then
     * reconstructs it so the pins become connectable.
     * @param NodePath Full node object path, as reported by ListStructMemberPins.
     * @param MembersJson JSON object of member name -> bool, e.g. {"FillImage":true,"Padding":false}.
     *   Members left out keep their current state. Unknown names are reported back, not silently ignored.
     * @return JSON with "changed" (names actually toggled) and "unknown" (names not on the struct).
     */
    UFUNCTION(meta = (AICallable), Category = "AscentGraph")
    static FString SetStructMemberPins(const FString& NodePath, const FString& MembersJson);
};
