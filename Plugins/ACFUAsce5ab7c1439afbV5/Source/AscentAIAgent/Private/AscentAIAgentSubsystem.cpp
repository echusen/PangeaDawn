// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentAIAgentSubsystem.h"
#include "AscentEpicMCP.h"
#include "Misc/ConfigCacheIni.h"
#include "AscentAIAgent.h"
#include "AscentAIAgentSettings.h"

// Epic MCP server (ModelContextProtocol plugin)
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"

#include "Commands/UECommandBase.h"
#include "Commands/UESelectionCommands.h"
#include "Commands/UEBlueprintCommands.h"
#include "Commands/UEAnimGraphCommands.h"
#include "Commands/UEDataCommands.h"
#include "Commands/UEDataTableCommands.h"
#include "Commands/UEPythonCommands.h"
#include "Toolsets/AscentToolsetUtils.h"

#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformProcess.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Async/Async.h"
#include "Misc/App.h"
#include "GameplayTagsManager.h"
#include "Engine/DataTable.h"

void UAscentAIAgentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RegisterCommandDomains();
    UE_LOG(LogAscentAIAgent, Log, TEXT("AscentAIAgentSubsystem initialized"));
}

void UAscentAIAgentSubsystem::Deinitialize()
{
    AscentEpicMCP::Shutdown();
    Super::Deinitialize();
    UE_LOG(LogAscentAIAgent, Log, TEXT("AscentAIAgentSubsystem deinitialized"));
}

//==========================================================================
// Yarin's own instance of Epic's MCP server (ModelContextProtocol plugin)
//==========================================================================

namespace AscentEpicMCP
{
    // Yarin owns a dedicated FModelContextProtocolServer instance on its own loopback port instead of the module-owned server on the default port 8000.
    // Reason: a project (or another plugin) may already own the module server with its own URL path, tool restrictions, and session permissions. Sharing it would either break that owner or leave
    // Yarin with a filtered tool list. A private unrestricted instance always serves the complete module tool catalog: every Epic ToolsetRegistry tool plus the Ascent toolsets.
    static TUniquePtr<FModelContextProtocolServer> YarinServer;
    static FDelegateHandle RefreshToolsHandle;
    static uint32 ActivePort = 0;
    // Ports probed after the configured MCP Server Port when it is busy.
    static constexpr uint32 PortAttempts = 10;

    /** True when Yarin's MCP server instance is up. */
    bool IsRunning()
    {
        return YarinServer.IsValid() && YarinServer->IsServerRunning();
    }

    /** Loopback URL of Yarin's MCP server, e.g. "http://127.0.0.1:8890/mcp". Valid after Ensure() has succeeded. */
    FString Url()
    {
        return FString::Printf(TEXT("http://127.0.0.1:%u/mcp"), ActivePort != 0 ? ActivePort : static_cast<uint32>(UACFAIAgentDeveloperSettings::Get()->McpServerPort));
    }

    /** Actually start the server. MUST be on the game thread (see Ensure). */
    static void StartServerOnGameThread()
    {
        check(IsInGameThread());
        IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
        if (!Module)
        {
            return;
        }
        // Yarin serves the exact default Epic MCP surface. bEnableToolSearch stays whatever the user
        // configured in Editor Preferences (Epic default: on, exposing list_toolsets, describe_toolset,
        // and call_tool discovery). 
        Module->RefreshTools();
        const uint32 FirstPortToTry = static_cast<uint32>(FMath::Clamp(UACFAIAgentDeveloperSettings::Get()->McpServerPort, 1024, 65535 - static_cast<int32>(PortAttempts)));
        if (IsRunning())
        {
            // Honor a changed MCP Server Port on the next Connect: restart only when the running port left the configured range.
            if (ActivePort >= FirstPortToTry && ActivePort < FirstPortToTry + PortAttempts)
            {
                return;
            }
            YarinServer->StopServer();
            ActivePort = 0;
        }
        if (!YarinServer.IsValid())
        {
            YarinServer = MakeUnique<FModelContextProtocolServer>();
            // The module only forwards tools-list-changed notifications to its own server. Yarin's instance must subscribe itself so connected clients see late-registered toolsets.
            RefreshToolsHandle = Module->OnRefreshTools().AddLambda([]
            {
                if (YarinServer.IsValid()) { YarinServer->ScheduleToolsListChangedBroadcast(); }
            });
        }
        for (uint32 Port = FirstPortToTry; Port < FirstPortToTry + PortAttempts; ++Port)
        {
            // Scoped to this listener: bind loopback only, never inheriting a machine-wide permissive HTTP server binding or port reuse setting.
            TArray<FString> Overrides;
            GConfig->GetArray(TEXT("HTTPServer.Listeners"), TEXT("ListenerOverrides"), Overrides, GEngineIni);
            const FString Override = FString::Printf(TEXT("(Port=%u,BindAddress=127.0.0.1,ReuseAddressAndPort=false)"), Port);
            if (!Overrides.Contains(Override))
            {
                Overrides.Insert(Override, 0);
                GConfig->SetArray(TEXT("HTTPServer.Listeners"), TEXT("ListenerOverrides"), Overrides, GEngineIni);
            }
            YarinServer->StartServer(Port, TEXT("/mcp"));
            if (YarinServer->IsServerRunning())
            {
                ActivePort = Port;
                UE_LOG(LogAscentAIAgent, Log, TEXT("Yarin MCP server listening at %s"), *Url());
                return;
            }
        }
        UE_LOG(LogAscentAIAgent, Warning, TEXT("Yarin MCP server could not bind any port from %u to %u."), FirstPortToTry, FirstPortToTry + PortAttempts - 1);
    }

    /**
     * Start Yarin's MCP server instance if it is not already running. Skipped
     * in commandlet / unattended / headless contexts: the server is a
     * developer-only tool and a failed port bind would abort cooks.
     *
     * THREAD SAFETY: FModelContextProtocolServer::StartServer reaches
     * FHttpServerModule::Get(), which asserts IsInGameThread(). Callers may
     * arrive off the game thread, so marshal the start.
     */
    bool Ensure()
    {
        if (IsRunningCommandlet() || FApp::IsUnattended() || !FApp::CanEverRender())
        {
            return false;
        }
        if (!IsInGameThread())
        {
            FEvent* Completed = FPlatformProcess::GetSynchEventFromPool(/*bIsManualReset*/ true);
            AsyncTask(ENamedThreads::GameThread, [Completed]()
            {
                StartServerOnGameThread();
                Completed->Trigger();
            });
            // Bounded: a busy/blocked game thread must not wedge the caller.
            const bool bCompleted = Completed->Wait(10000);
            FPlatformProcess::ReturnSynchEventToPool(Completed);
            if (!bCompleted)
            {
                UE_LOG(LogAscentAIAgent, Warning,
                    TEXT("Timed out waiting for the Yarin MCP server to start on the game thread."));
                return false;
            }
            return IsRunning();
        }

        StartServerOnGameThread();
        return IsRunning();
    }

    /** Stops and destroys Yarin's server instance. Called from the editor subsystem's Deinitialize, before the ModelContextProtocol module shuts down. */
    void Shutdown()
    {
        if (IModelContextProtocolModule* Module = IModelContextProtocolModule::Get(); Module && RefreshToolsHandle.IsValid())
        {
            Module->OnRefreshTools().Remove(RefreshToolsHandle);
        }
        RefreshToolsHandle.Reset();
        YarinServer.Reset();
        ActivePort = 0;
    }
}

void UAscentAIAgentSubsystem::RegisterCommandDomains()
{
    CommandDomains.Reset();
    CommandDomains.Add(MakeUnique<FUESelectionCommands>(this));
    CommandDomains.Add(MakeUnique<FUEBlueprintCommands>(this));
    CommandDomains.Add(MakeUnique<FUEAnimGraphCommands>(this));
    CommandDomains.Add(MakeUnique<FUEDataCommands>(this));
    CommandDomains.Add(MakeUnique<FUEDataTableCommands>(this));
    CommandDomains.Add(MakeUnique<FUEPythonCommands>(this));
}

FUECommandBase* UAscentAIAgentSubsystem::FindDomainForCommand(const FString& CommandType) const
{
    for (const TUniquePtr<FUECommandBase>& Domain : CommandDomains)
    {
        if (Domain.IsValid() && Domain->IsCommand(CommandType))
        {
            return Domain.Get();
        }
    }
    return nullptr;
}


FString UAscentAIAgentSubsystem::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogAscentAIAgent, Log, TEXT("ACFBridge: Executing '%s'"), *CommandType);

    // Running inline on the game thread avoids a self-deadlock (dispatch + wait on the same thread).
    if (IsInGameThread())
    {
        if (FUECommandBase* Domain = FindDomainForCommand(CommandType))
        {
            return Domain->HandleCommand(CommandType, Params);
        }
        return FAscentToolsetUtils::MakeErrorResponse(FString::Printf(TEXT("Unknown command: %s"), *CommandType));
    }

    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        FUECommandBase* Domain = FindDomainForCommand(CommandType);
        Promise.SetValue(Domain
            ? Domain->HandleCommand(CommandType, Params)
            : FAscentToolsetUtils::MakeErrorResponse(FString::Printf(TEXT("Unknown command: %s"), *CommandType)));
    });

    double RequestedTimeoutMs = 0.0;
    const bool bHasRequestedTimeout = Params.IsValid()
        && Params->TryGetNumberField(TEXT("timeout_ms"), RequestedTimeoutMs);
    const float TimeoutSeconds = bHasRequestedTimeout
        ? FMath::Clamp(static_cast<float>(RequestedTimeoutMs / 1000.0), 1.0f, 120.0f)
        : 10.0f;
    if (Future.WaitFor(FTimespan::FromSeconds(TimeoutSeconds)))
    {
        return Future.Get();
    }

    return FAscentToolsetUtils::MakeErrorResponse(TEXT("Game thread timeout — operation took too long"));
}

TSharedPtr<FJsonObject> UAscentAIAgentSubsystem::CollectSelectedBlueprintNodesData() const
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> NodesArray;

    // Find the active Blueprint editor
    UAssetEditorSubsystem* AssetEditorSub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (AssetEditorSub)
    {
        TArray<UObject*> EditedAssets = AssetEditorSub->GetAllEditedAssets();
        for (UObject* Asset : EditedAssets)
        {
            UBlueprint* BP = Cast<UBlueprint>(Asset);
            if (!BP) continue;

            // Get all graphs and check for selected nodes
            TArray<UEdGraph*> AllGraphs;
            BP->GetAllGraphs(AllGraphs);
            for (UEdGraph* Graph : AllGraphs)
            {
                if (!Graph) continue;
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (Node && Node->IsSelected())
                    {
                        TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
                        NodeJson->SetStringField(TEXT("name"), Node->GetName());
                        NodeJson->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                        NodeJson->SetStringField(TEXT("class"), Node->GetClass()->GetName());
                        NodeJson->SetStringField(TEXT("graph"), Graph->GetName());

                        // Pins
                        TArray<TSharedPtr<FJsonValue>> PinsArray;
                        for (UEdGraphPin* Pin : Node->Pins)
                        {
                            if (!Pin) continue;
                            TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
                            PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
                            PinJson->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
                            PinJson->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
                            PinJson->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
                            if (!Pin->DefaultValue.IsEmpty())
                            {
                                PinJson->SetStringField(TEXT("default_value"), Pin->DefaultValue);
                            }
                            PinsArray.Add(MakeShared<FJsonValueObject>(PinJson));
                        }
                        NodeJson->SetArrayField(TEXT("pins"), PinsArray);

                        NodesArray.Add(MakeShared<FJsonValueObject>(NodeJson));
                    }
                }
            }
        }
    }

    Data->SetArrayField(TEXT("nodes"), NodesArray);
    Data->SetNumberField(TEXT("count"), NodesArray.Num());
    return Data;
}

FString UAscentAIAgentSubsystem::GetSelectedBlueprintNodes() const
{
    TSharedPtr<FJsonObject> Data = CollectSelectedBlueprintNodesData();
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}


FString UAscentAIAgentSubsystem::AddTagToDataTable(const TSharedPtr<FJsonObject>& Params)
{
    FString DataTablePath = Params->GetStringField(TEXT("datatable_path"));
    FString TagName = Params->GetStringField(TEXT("tag"));
    FString DevComment = Params->GetStringField(TEXT("dev_comment"));

    if (DataTablePath.IsEmpty())
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("Missing required param: datatable_path"));
    }
    if (TagName.IsEmpty())
    {
        return FAscentToolsetUtils::MakeErrorResponse(TEXT("Missing required param: tag"));
    }

    // Load the DataTable
    UDataTable* DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));
    if (!DataTable)
    {
        return FAscentToolsetUtils::MakeErrorResponse(FString::Printf(TEXT("DataTable not found: %s"), *DataTablePath));
    }

    // Verify it's a GameplayTag table
    if (DataTable->GetRowStruct() != FGameplayTagTableRow::StaticStruct())
    {
        return FAscentToolsetUtils::MakeErrorResponse(FString::Printf(TEXT("DataTable '%s' is not a GameplayTag table (row type: %s)"),
            *DataTablePath, *DataTable->GetRowStruct()->GetName()));
    }

    // Check if tag already exists in this table
    FName RowName = FName(*TagName);
    if (DataTable->FindRowUnchecked(RowName))
    {
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("tag"), TagName);
        Data->SetStringField(TEXT("datatable"), DataTablePath);
        Data->SetBoolField(TEXT("already_existed"), true);
        return FAscentToolsetUtils::MakeSuccessResponse(Data);
    }

    // Add the new row
    const FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("Yarin: Add tag %s"), *TagName)));
    DataTable->Modify();

    FGameplayTagTableRow NewRow;
    NewRow.Tag = FName(*TagName);
    NewRow.DevComment = DevComment.IsEmpty() ? TEXT("Added by Yarin") : DevComment;

    DataTable->AddRow(RowName, NewRow);
    DataTable->MarkPackageDirty();

    // Register the tag so it's immediately usable without editor restart
    UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
    FGameplayTag NewTag = TagManager.RequestGameplayTag(FName(*TagName), false);
    if (!NewTag.IsValid())
    {
        TagManager.AddNativeGameplayTag(FName(*TagName), DevComment.IsEmpty() ? TEXT("Added by Yarin") : DevComment);
    }

    UE_LOG(LogAscentAIAgent, Display, TEXT("AddTagToDataTable: Added '%s' to %s"), *TagName, *DataTablePath);

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("tag"), TagName);
    Data->SetStringField(TEXT("datatable"), DataTablePath);
    Data->SetBoolField(TEXT("already_existed"), false);
    Data->SetNumberField(TEXT("total_rows"), DataTable->GetRowMap().Num());
    return FAscentToolsetUtils::MakeSuccessResponse(Data);
}

