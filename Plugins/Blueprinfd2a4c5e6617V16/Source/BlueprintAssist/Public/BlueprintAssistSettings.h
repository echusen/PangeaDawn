// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "EdGraph/EdGraphNode.h"
#include "Framework/Commands/InputChord.h"
#include "BlueprintAssistSettings.generated.h"

class UEdGraph;

UENUM()
enum class EBANodeFormattingStyle : uint8
{
	Expanded UMETA(DisplayName = "Expanded", Tooltip = "Expand execution nodes leaving space for parameter nodes"),
	Compact UMETA(DisplayName = "Compact", Tooltip = "Do not expand execution nodes"),
};

UENUM()
enum class EBAParameterFormattingStyle : uint8
{
	Helixing UMETA(DisplayName = "Helixing", Tooltip = "Place parameter nodes below the execution node"),
	LeftSide UMETA(DisplayName = "Left-side", Tooltip = "Place parameter nodes left of the main execution node"),
};

UENUM()
enum class EBAWiringStyle : uint8
{
	AlwaysMerge UMETA(DisplayName = "Always Merge"),
	MergeWhenNear UMETA(DisplayName = "Merge When Near"),
	SingleWire UMETA(DisplayName = "Single Wire"),
};

UENUM()
enum class EBAAutoFormatting : uint8
{
	Never UMETA(DisplayName = "Never"),
	FormatAllConnected UMETA(DisplayName = "Format all connected nodes"),
	FormatSingleConnected UMETA(DisplayName = "Format relative to a connected node"),
};

UENUM()
enum class EBAFormatAllStyle : uint8
{
	Simple UMETA(DisplayName = "Simple (single column)"),
	Smart UMETA(DisplayName = "Smart (create columns from node position)"),
	NodeType UMETA(DisplayName = "Node Type (columns by node type)"),
};

UENUM()
enum class EBAFormatAllHorizontalAlignment : uint8
{
	RootNode UMETA(DisplayName = "Root Node (align the left-side of the node tree's root node)"),
	Comment UMETA(DisplayName = "Comment (align the left-side of any containing comments)"),
};

UENUM()
enum class EBAFormatterType : uint8
{
	Blueprint UMETA(DisplayName = "Blueprint"),
	BehaviorTree UMETA(DisplayName = "BehaviorTree"),
	Simple UMETA(DisplayName = "Simple formatter"),
};

USTRUCT()
struct FBAKnotTrackSettings
{
	GENERATED_BODY()

	/* Knot nodes x-offset for regular execution wires */
	UPROPERTY(EditAnywhere, config, Category = "BlueprintFormatting")
	int32 KnotXOffset = 0;

	/* Knot node offset for wires that flow backwards in execution */
	UPROPERTY(EditAnywhere, config, Category = "BlueprintFormatting")
	FIntPoint LoopingOffset = FIntPoint(0, 0);
};

USTRUCT()
struct FBAFormatterSettings
{
	GENERATED_BODY()

	FBAFormatterSettings();

	FBAFormatterSettings(FIntPoint InPadding, EBAAutoFormatting InAutoFormatting, EEdGraphPinDirection InFormatterDirection, TArray<FName> InRootNodes = TArray<FName>())
		: Padding(InPadding)
		, AutoFormatting(InAutoFormatting)
		, FormatterDirection(InFormatterDirection)
		, RootNodes(InRootNodes)
	{
	}

	/* Setting to enable / disable all behaviour for this graph type */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings)
	bool bEnabled = true;

	/* Formatter to use */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	EBAFormatterType FormatterType = EBAFormatterType::Simple;

	/* Padding used when formatting nodes */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	FIntPoint Padding = FIntPoint(100, 100);

	/* Auto formatting method to be used for this graph */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	EBAAutoFormatting AutoFormatting = EBAAutoFormatting::Never;

	/* Direction of execution flow in this graph */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	TEnumAsByte<EEdGraphPinDirection> FormatterDirection;

	/* Names of any root nodes that this graph uses */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	TArray<FName> RootNodes;

	/* Name of the execution pin for this graph type */
	UPROPERTY(EditAnywhere, config, Category=FormatterSettings, meta = (EditCondition = "bEnabled"))
	FName ExecPinName;

	FString ToString() const
	{
		return FString::Printf(TEXT("FormatterType %d | ExecPinName %s"), FormatterType, *ExecPinName.ToString());
	}

	EBAAutoFormatting GetAutoFormatting() const;
};

UCLASS(config = EditorPerProjectUserSettings)
class BLUEPRINTASSIST_API UBASettings final : public UBASettingsBase
{
	GENERATED_BODY()

public:
	UBASettings(const FObjectInitializer& ObjectInitializer);

	static FORCEINLINE const UBASettings& Get()
	{
		return *GetDefault<UBASettings>();
	}
	static FORCEINLINE UBASettings& GetMutable()
	{
		return *GetMutableDefault<UBASettings>();
	}

	////////////////////////////////////////////////////////////
	// General
	////////////////////////////////////////////////////////////

	/* Cache node sizes of any newly detected nodes. Checks upon opening a blueprint or when a new node is added to the graph. */
	UPROPERTY(EditAnywhere, config, Category = General)
	bool bDetectNewNodesAndCacheNodeSizes;

	/* Supported asset editors by name */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = General)
	TArray<FName> SupportedAssetEditors;

	/* Supported graph editors by name */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = General)
	TArray<FName> SupportedGraphEditors;

	////////////////////////////////////////////////////////////
	// Formatting options
	////////////////////////////////////////////////////////////

	/* Enabling this is the same as setting auto formatting to Never for all graphs */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bGloballyDisableAutoFormatting;

	/* Determines how execution nodes are positioned */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	EBANodeFormattingStyle FormattingStyle;

	/* Determines how parameters are positioned */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	EBAParameterFormattingStyle ParameterStyle;

	/* Determines how execution wires are created */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	EBAWiringStyle ExecutionWiringStyle;

	/* Determines how parameter wires are created */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	EBAWiringStyle ParameterWiringStyle;

	/* Reuse knot nodes instead of creating new ones every time */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bUseKnotNodePool;

	/* Should helixing be disabled if there are multiple linked pins */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (InlineEditConditionToggle))
	bool bDisableHelixingWithMultiplePins;

	/* Disable helixing if the number of linked parameter pins is >= than this number */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bDisableHelixingWithMultiplePins"))
	int32 DisableHelixingPinCount;

	/* Whether to use HelixingHeightMax and SingleNodeMaxHeight */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bLimitHelixingHeight;

	/* Helixing is disabled if the total height of the parameter nodes is larger than this value */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bLimitHelixingHeight"))
	int HelixingHeightMax;

	/* Helixing is disabled if a single node is taller than this value */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions, meta = (EditCondition = "bLimitHelixingHeight"))
	int SingleNodeMaxHeight;

	/* Create knot nodes */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bCreateKnotNodes;

	/* Add spacing to nodes so they are always in front of their input parameters */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bExpandNodesAheadOfParameters;

	/* Add horizontal spacing depending on how vertically far from they are from the linked node */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bExpandNodesByHeight;

	/* The maximum horizontal distance allowed to be expanded */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	float ExpandNodesMaxDist;

	/* Add horizontal spacing depending on how vertically far from they are from the linked node (for parameter nodes) */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bExpandParametersByHeight;

	/* The maximum horizontal distance allowed to be expanded (for parameter nodes) */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	float ExpandParametersMaxDist;

	/* Snap nodes to grid (in the x-axis) after formatting */
	UPROPERTY(EditAnywhere, config, Category = FormattingOptions)
	bool bSnapToGrid;

	/* Skip auto formatting if the new node caused any pins to disconnect */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = FormattingOptions)
	bool bSkipAutoFormattingAfterBreakingPins;

	////////////////////////////////////////////////////////////
	/// Format All
	////////////////////////////////////////////////////////////

	/* Determines how nodes are positioned into columns when running formatting all nodes */
	UPROPERTY(EditAnywhere, config, Category = FormatAll)
	EBAFormatAllStyle FormatAllStyle;

	/* Determines how nodes are aligned horizontally */
	UPROPERTY(EditAnywhere, config, Category = FormatAll)
	EBAFormatAllHorizontalAlignment FormatAllHorizontalAlignment;

	/* x values defines padding between columns, y value defines horizontal padding between node trees */
	UPROPERTY(EditAnywhere, config, Category = FormatAll)
	FIntPoint FormatAllPadding;

	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta=(InlineEditConditionToggle))
	bool bUseFormatAllPaddingInComment;

	/* Determines the vertical spacing for the Format All command when event nodes are in the same comment */
	UPROPERTY(EditAnywhere, config, Category = FormatAll, meta=(EditCondition="bUseFormatAllPaddingInComment"))
	int32 FormatAllPaddingInComment;

	/* Call the format all function when a new event node is added to the graph */
	UPROPERTY(EditAnywhere, config, Category = FormatAll)
	bool bAutoPositionEventNodes;

	/* Call the format all function when ANY new node is added to the graph. Useful for when the 'UseColumnsForFormatAll' setting is on. */
	UPROPERTY(EditAnywhere, config, Category = FormatAll)
	bool bAlwaysFormatAll;

	////////////////////////////////////////////////////////////
	// Blueprint formatting
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	FBAFormatterSettings BlueprintFormatterSettings;

	/* Padding used between parameter nodes */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	FIntPoint BlueprintParameterPadding;

	/* Offsets for execution knot tracks */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting)
	FBAKnotTrackSettings BlueprintExecutionKnotSettings;

	/* Offsets for parameter knot tracks */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting)
	FBAKnotTrackSettings BlueprintParameterKnotSettings;

	/* Blueprint formatting will be used for these types of graphs (you can see the type of a graph with the PrintGraphInfo command, default: unbound) */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting)
	TArray<FName> UseBlueprintFormattingForTheseGraphs;

	/* When formatting treat delegate pins as execution pins, recommended to turn this option off and use the 'CreateEvent' node */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	bool bTreatDelegatesAsExecutionPins;

	/* Center node execution branches (Default: center nodes with 3 or more branches) */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	bool bCenterBranches;

	/* Only center branches if we have this (or more) number of branches */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (EditCondition = "bCenterBranches"))
	int NumRequiredBranches;

	/* Center parameters nodes with multiple links */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	bool bCenterBranchesForParameters;

	/* Only center parameters which have this many (or more) number of links */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting, meta = (EditCondition = "bCenterBranchesForParameters"))
	int NumRequiredBranchesForParameters;

	/* Vertical spacing from the last linked pin */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	int VerticalPinSpacing;

	/* Vertical spacing from the last linked pin for parameters */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	int ParameterVerticalPinSpacing;

	/* Spacing used between wire tracks */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	int BlueprintKnotTrackSpacing;

	/* If the knot's vertical dist to the linked pin is less than this value, it won't be created */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category = BlueprintFormatting)
	int CullKnotVerticalThreshold;

	/* The width between pins required for a knot node to be created */
	UPROPERTY(EditAnywhere, config, Category = BlueprintFormatting)
	int KnotNodeDistanceThreshold;

	////////////////////////////////////////////////////////////
	// Other Graphs
	////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, config, Category = OtherGraphs)
	TMap<FName, FBAFormatterSettings> NonBlueprintFormatterSettings;

	/* Extra padding to add between branches */
	UPROPERTY(EditAnywhere, config, Category = BehaviorTree)
	float BehaviorTreeBranchExtraPadding;

	////////////////////////////////////////////////////////////
	// Comment Settings
	////////////////////////////////////////////////////////////

	/* Apply comment padding when formatting */
	UPROPERTY(EditAnywhere, config, Category = Comments)
	bool bApplyCommentPadding;

	/* Add knot nodes to comments after formatting */
	UPROPERTY(EditAnywhere, config, Category = Comments)
	bool bAddKnotNodesToComments;

	/* Padding around the comment box. Make sure this is the same as in the AutoSizeComments setting */
	UPROPERTY(EditAnywhere, config, Category = Comments)
	FIntPoint CommentNodePadding;

	/* Watch the size of comment nodes and refresh the comment bar title size */
	UPROPERTY(EditAnywhere, config, Category = Comments)
	bool bRefreshCommentTitleBarSize;

	////////////////////////////////////////////////////////////
	// Accessibility
	////////////////////////////////////////////////////////////

	/**
	 * When caching nodes, the viewport will jump to each node and this can cause discomfort for photosensitive users.
	 * This setting displays an overlay to prevent this.
	 */
	UPROPERTY(EditAnywhere, config, Category = Accessibility)
	bool bShowOverlayWhenCachingNodes;

	/* Number of pending caching nodes required to show the progress bar in the center of the overlay */
	UPROPERTY(EditAnywhere, config, Category = Accessibility, meta = (EditCondition = "bShowOverlayWhenCachingNodes"))
	int RequiredNodesToShowOverlayProgressBar;

	////////////////////////////////////////////////////////////
	// Experimental
	////////////////////////////////////////////////////////////

	/* Faster formatting will only format chains of nodes have been moved or had connections changed. Greatly increases speed of the format all command. */
	UPROPERTY(EditAnywhere, config, Category = Experimental)
	bool bEnableFasterFormatting;

	/* Align execution nodes to the 8x8 grid when formatting */
	UPROPERTY(EditAnywhere, config, Category = Experimental, DisplayName="Align Exec Nodes To 8x8 Grid")
	bool bAlignExecNodesTo8x8Grid;

	/* Save all before formatting */
	UPROPERTY(EditAnywhere, config, Category = Experimental)
	bool bSaveAllBeforeFormatting;

	/* Run the format all command after saving a graph */
	UPROPERTY(EditAnywhere, config, Category = Experimental)
	bool bFormatAllAfterSaving;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	static FBAFormatterSettings GetFormatterSettings(UEdGraph* Graph);
	static FBAFormatterSettings* FindFormatterSettings(UEdGraph* Graph);
};

class FBASettingsDetails final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
