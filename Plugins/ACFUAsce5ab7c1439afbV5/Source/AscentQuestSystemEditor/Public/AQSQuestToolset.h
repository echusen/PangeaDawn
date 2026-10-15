// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AQSQuestToolset.generated.h"

/**
 * An instanced object authored on a quest node: an AGS action or condition. Properties are written
 * through reflection by name, using the value formats DescribeClass reports.
 */
USTRUCT(BlueprintType)
struct ASCENTQUESTSYSTEMEDITOR_API FAQSQuestObjectSpec
{
    GENERATED_BODY()

    /** Class name or path, e.g. "ACF_FadeToBlackActionBP". See ListClasses. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString Class;

    /** Property name to value. GameplayTags take the bare tag name; arrays take comma separated items. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TMap<FString, FString> Properties;
};

/** One node of a quest graph. */
USTRUCT(BlueprintType)
struct ASCENTQUESTSYSTEMEDITOR_API FAQSQuestNodeSpec
{
    GENERATED_BODY()

    /** Unique id inside the spec, referenced by edges. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString Id;

    /** start, objective, success or failure. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString Kind;

    /**
     * GUID of the existing node this entry updates, as returned by ExportQuest. Keeping it keeps save games
     * that reference this objective valid. Empty for new nodes.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString NodeGuid;

    /** Objective nodes: objective class name or path (see ListClasses "objective"). Empty uses the inert base class. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString ObjectiveClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString ObjectiveName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString ObjectiveDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString ObjectiveTag;

    /** ETag (targets found by gameplay tag) or ESoftRef (specific level actors). Inferred when empty. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString TargetRefType;

    /** ETag targets: gameplay tags carried by the targets' AQSQuestTargetComponent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FString> ReferencedTargets;

    /** ESoftRef targets: level actor paths, e.g. "/Game/Maps/L_Town.L_Town:PersistentLevel.BP_Guard_2". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FString> ReferencedActors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    int32 Repetitions = 1;

    /** Class specific objective properties (see DescribeClass), e.g. the item to craft. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TMap<FString, FString> ObjectiveProperties;

    /** Objective nodes: conditions that must hold for the objective to complete. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FAQSQuestObjectSpec> CompleteConditions;

    /** Actions run when the node activates. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FAQSQuestObjectSpec> ActivationActions;

    /** Actions run when the node completes, e.g. rewards. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FAQSQuestObjectSpec> CompletionActions;

    /** Success and failure nodes: quest started when this end is reached. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString NextQuestTag;

    /** Editor graph position. When every node is at the origin the graph is laid out automatically. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FVector2D Position = FVector2D::ZeroVector;
};

/** A transition between two nodes. */
USTRUCT(BlueprintType)
struct ASCENTQUESTSYSTEMEDITOR_API FAQSQuestEdgeSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString From;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString To;

    /** Branch name, used when an objective completes with a transition filter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString TransitionName;
};

/** A complete quest: asset identity, graph and runtime registration. */
USTRUCT(BlueprintType)
struct ASCENTQUESTSYSTEMEDITOR_API FAQSQuestSpec
{
    GENERATED_BODY()

    /** Quest asset path, e.g. "/Game/MyProject/Quests/AQS_LostSword". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString AssetPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString QuestTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString QuestName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString QuestDescription;

    /**
     * AQSQuestData DataTable read by the AQSQuestManagerComponent. StartQuest only finds quests listed there,
     * so a new quest that is not registered can never start.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString QuestsDB;

    /** Row name in QuestsDB. Defaults to the asset name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    FString DBRowName;

    /** Required to modify a quest that already exists. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    bool bOverwriteExisting = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FAQSQuestNodeSpec> Nodes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF")
    TArray<FAQSQuestEdgeSpec> Edges;
};

/** What saving a spec would do, or did. */
USTRUCT(BlueprintType)
struct ASCENTQUESTSYSTEMEDITOR_API FAQSQuestReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString AssetPath;

    /** True when the asset does not exist yet. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bCreatesAsset = false;

    /** Existing nodes kept by GUID. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    int32 KeptNodes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    int32 AddedNodes = 0;

    /** Existing nodes that the spec drops. Save games inside them lose that progress. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> RemovedNodes;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    bool bRegisteredInDB = false;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> Errors;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    TArray<FString> Warnings;
};

USTRUCT(BlueprintType)
struct ASCENTQUESTSYSTEMEDITOR_API FAQSQuestClassInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString ClassPath;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Description;
};

USTRUCT(BlueprintType)
struct ASCENTQUESTSYSTEMEDITOR_API FAQSQuestPropertyInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Type;

    /** Class default, in the same text format the spec accepts. */
    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString DefaultValue;

    UPROPERTY(BlueprintReadOnly, Category = "ACF")
    FString Description;
};

/**
 * Ascent Quest System tools for AI assistants: discover objective, action and condition classes, read a
 * quest as a spec, and create or update quests from a spec. Updates keep node GUIDs, so save games stay valid.
 */
UCLASS(BlueprintType, Hidden)
class ASCENTQUESTSYSTEMEDITOR_API UAQSQuestToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Lists the installed classes of one kind, native and Blueprint, discovered live from the project.
     * Objectives built on the base class have no completion logic, so always pick a subclass.
     * @param Kind "objective", "action" or "condition".
     * @return Class names and paths.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static TArray<FAQSQuestClassInfo> ListClasses(const FString& Kind);

    /**
     * Lists the editable properties of an objective, action or condition class, with type and default value.
     * Use these names in ObjectiveProperties or FAQSQuestObjectSpec::Properties.
     * @param Class Class name or path from ListClasses.
     * @return Editable properties.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static TArray<FAQSQuestPropertyInfo> DescribeClass(const FString& Class);

    /**
     * Reads an existing quest as a spec, including node GUIDs, class specific properties that differ from the
     * class defaults, actions and conditions. Edit it and pass it to SaveQuest with bOverwriteExisting.
     * QuestsDB is not read back; set it again to (re)register the quest.
     * @param QuestPath Quest asset path.
     * @return The quest spec.
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "ACF")
    static FAQSQuestSpec ExportQuest(const FString& QuestPath);

    /**
     * Checks a spec exactly as SaveQuest would and writes nothing: structure, tags, classes, properties,
     * targets, QuestsDB, and which existing nodes would be kept, added or removed.
     * @param Spec The quest spec.
     * @return The report; bValid is false when SaveQuest would refuse it.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FAQSQuestReport ValidateQuest(const FAQSQuestSpec& Spec);

    /**
     * Creates or updates the quest asset from a spec, writing both the runtime and the editor graph, saves it,
     * and registers it in QuestsDB. Raises a script error, and writes nothing, when the spec is not valid.
     * @param Spec The quest spec. Set bOverwriteExisting to modify an existing quest.
     * @return The report of what was written.
     */
    UFUNCTION(meta = (AICallable), Category = "ACF")
    static FAQSQuestReport SaveQuest(const FAQSQuestSpec& Spec);
};
