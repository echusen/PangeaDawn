// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AscentAIAgent : ModuleRules
{
    public AscentAIAgent(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Toolset modules are many-TU with anonymous-namespace helpers; unity builds
        // hit ODR/helper collisions (known ACF/AUB pattern).
        bUseUnity = false;

        // AUB's source uses AGENTICMCP_API as its module-export macro. Inside our module
        // there's no DLL boundary, so define it to nothing so verbatim AUB headers compile
        // regardless of include order.
        PublicDefinitions.Add("AGENTICMCP_API=");

        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Public"),
                // Mirror of AgenticUEBridge sources lives under Public/AgenticMCP/.
                // Adding it as an include root lets verbatim AUB files resolve their
                // "AgenticMCPModule.h" and "Commands/AgenticMCP*.h" includes unchanged.
                Path.Combine(ModuleDirectory, "Public/AgenticMCP")
            });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EditorSubsystem",
            // Epic MCP: UToolsetDefinition base for the Ascent toolsets (public headers).
            "ToolsetRegistry"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DeveloperSettings",
            "Slate",
            "SlateCore",
            "UnrealEd",
            "EditorFramework",
            "ToolMenus",
            "Projects",
            "Json",
            "JsonUtilities",
            "HTTP",
            "HTTPServer",
            // Epic MCP server management for Yarin's private tool server.
            "ModelContextProtocol",
            "ModelContextProtocolEngine",
            "LevelEditor",
            "WorkspaceMenuStructure",
            "Blutility",
            "AssetTools",
            "AssetRegistry",
            "Kismet",
            "KismetCompiler",
            "BlueprintGraph",
            "PythonScriptPlugin",
            "GameplayTags",
            "GameplayAbilities",
            // AI generation pipeline (image + Meshy 3D) exposed to the assistant as tools.
            "AscentAITools",
            // AgenticMCP mirror (Blueprint commands + graph analysis + common utils)
            "BlueprintEditorLibrary",
            "GraphEditor",
            "PropertyEditor",
            "EditorScriptingUtilities",
            "PhysicsCore",
            "AnimGraph",
            "UMG",
            "UMGEditor"
        });

        // Live Coding is a Windows-only Editor module.
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("LiveCoding");
            PrivateDefinitions.Add("ACF_WITH_LIVE_CODING=1");
        }
        else
        {
            PrivateDefinitions.Add("ACF_WITH_LIVE_CODING=0");
        }
    }
}
