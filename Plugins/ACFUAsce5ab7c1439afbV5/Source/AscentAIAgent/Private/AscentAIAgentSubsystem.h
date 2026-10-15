// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "Commands/UECommandBase.h"
#include "AscentAIAgentSubsystem.generated.h"

/**
 * Editor subsystem that owns Yarin's private MCP server and the command
 * implementations behind the Ascent toolsets. The Slate panel does not call
 * these directly: Codex reaches them through Epic's tool registry.
 */
UCLASS()
class UAscentAIAgentSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FString GetSelectedBlueprintNodes() const;
    FString AddTagToDataTable(const TSharedPtr<FJsonObject>& Params);

    /**
     * Dispatch a toolset wire name to the matching command domain.
     * Runs inline on the game thread; marshals and waits otherwise.
     */
    FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    void RegisterCommandDomains();
    FUECommandBase* FindDomainForCommand(const FString& CommandType) const;

    TSharedPtr<FJsonObject> CollectSelectedBlueprintNodesData() const;

    TArray<TUniquePtr<FUECommandBase>> CommandDomains;
};
