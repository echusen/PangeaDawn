#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "BlueprintAssistMisc/BASettingsBase.h"
#include "Framework/Commands/InputChord.h"
#include "Layout/Margin.h"
#include "UObject/Object.h"
#include "BlueprintAssistSettings_EditorFeatures.generated.h"

UENUM()
enum class EBAFunctionAccessSpecifier : uint8
{
	Public UMETA(DisplayName = "Public"),
	Protected UMETA(DisplayName = "Protected"),
	Private UMETA(DisplayName = "Private"),
};

UENUM()
enum class EBAAutoZoomToNode : uint8
{
	Never UMETA(DisplayName = "Never"),
	Always UMETA(DisplayName = "Always"),
	Outside_Viewport UMETA(DisplayName = "Outside viewport"),
};

UENUM()
enum class EBAPinSelectionMethod : uint8
{
	/* Select the right execution pin */
	Execution UMETA(DisplayName = "Execution"),

	/* Select the first value (unlinked parameter) pin, otherwise select the right execution pin */
	Value UMETA(DisplayName = "Value"),
};


UCLASS(Config = EditorPerProjectUserSettings)
class BLUEPRINTASSIST_API UBASettings_EditorFeatures final : public UBASettingsBase
{
	GENERATED_BODY()

public:
	UBASettings_EditorFeatures(const FObjectInitializer& ObjectInitializer);

	////////////////////////////////////////////////////////////
	/// CustomEventReplication
	////////////////////////////////////////////////////////////

	/* Set the according replication flags after renaming a custom event by matching the prefixes below */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	bool bSetReplicationFlagsAfterRenaming;

	/* When enabled, renaming a custom event with no matching prefix will apply "NotReplicated" */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta=(EditCondition="bSetReplicationFlagsAfterRenaming"))
	bool bClearReplicationFlagsWhenRenamingWithNoPrefix;

	/* Add the according prefix to the title after changing replication flags */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	bool bAddReplicationPrefixToCustomEventTitle;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	FString MulticastPrefix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	FString ServerPrefix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	FString ClientPrefix;

	////////////////////////////////////////////////////////////
	/// Node group
	////////////////////////////////////////////////////////////

	/* Draw an outline to visualise each node group on the graph */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup)
	bool bDrawNodeGroupOutline;

	/* Only draw the group outline when selected */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides))
	bool bOnlyDrawGroupOutlineWhenSelected;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides))
	FLinearColor NodeGroupOutlineColor;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides))
	float NodeGroupOutlineWidth;

	/* Outline margin around each node */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupOutline", EditConditionHides))
	FMargin NodeGroupOutlineMargin;

	/* Draw a fill to show the node groups for selected nodes */
	UPROPERTY(EditAnywhere, Category = NodeGroup)
	bool bDrawNodeGroupFill;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, Config, Category = NodeGroup, meta=(EditCondition="bDrawNodeGroupFill", EditConditionHides))
	FLinearColor NodeGroupFillColor;

	////////////////////////////////////////////////////////////
	/// Graph
	////////////////////////////////////////////////////////////

	/* Distance the viewport moves when running the Shift Camera command. Scaled by zoom distance. */
	UPROPERTY(EditAnywhere, config, Category = "Graph")
	int ShiftCameraDistance;

	/* Automatically add parent nodes to event nodes */
	UPROPERTY(EditAnywhere, config, Category = "Graph")
	bool bAutoAddParentNode;

	/* Change the color of the border around the selected pin */
	UPROPERTY(EditAnywhere, config, Category = "Graph")
	FLinearColor SelectedPinHighlightColor;

	/* Determines which pin should be selected when you click on an execution node */
	UPROPERTY(EditAnywhere, config, Category = "Graph")
	EBAPinSelectionMethod PinSelectionMethod_Execution;

	/* Determines which pin should be selected when you click on a parameter node */
	UPROPERTY(EditAnywhere, config, Category = "Graph")
	EBAPinSelectionMethod PinSelectionMethod_Parameter;

	/* Sets the 'Comment Bubble Pinned' bool for all nodes on the graph (Auto Size Comment plugin handles this value for comments) */
	UPROPERTY(EditAnywhere, config, Category = "Graph|Comments")
	bool bEnableGlobalCommentBubblePinned;

	/* The global 'Comment Bubble Pinned' value */
	UPROPERTY(EditAnywhere, config, Category = "Graph|Comments", meta = (EditCondition = "bEnableGlobalCommentBubblePinned"))
	bool bGlobalCommentBubblePinnedValue;

	/* Determines if we should auto zoom to a newly created node */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour")
	EBAAutoZoomToNode AutoZoomToNodeBehavior = EBAAutoZoomToNode::Outside_Viewport;

	/* Try to insert the node between any current wires when holding down this key */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour")
	FInputChord InsertNewNodeKeyChord;

	/* When creating a new node from a parameter pin, always try to connect the execution. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour")
	bool bAlwaysConnectExecutionFromParameter;

	/* When creating a new node from a parameter pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour")
	bool bAlwaysInsertFromParameter;

	/* When creating a new node from an execution pin, always try to insert between wires. Holding InsertNewNodeChord will disable this. */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour")
	bool bAlwaysInsertFromExecution;

	/* Select the first editable parameter pin when a node is created */
	UPROPERTY(EditAnywhere, config, Category = "Graph|New Node Behaviour")
	bool bSelectValuePinWhenCreatingNewNodes;

	////////////////////////////////////////////////////////////
	/// General
	////////////////////////////////////////////////////////////

	/* Add the BlueprintAssist widget to the toolbar */
	UPROPERTY(EditAnywhere, config, Category = "General")
	bool bAddToolbarWidget;

	/* Automatically rename Function getters and setters when the Function is renamed */
	UPROPERTY(EditAnywhere, config, Category = "General|Getters and Setters")
	bool bAutoRenameGettersAndSetters;

	/* Merge the generate getter and setter into one button */
	UPROPERTY(EditAnywhere, config, Category = "General|Getters and Setters")
	bool bMergeGenerateGetterAndSetterButton;

	////////////////////////////////////////////////////////////
	// Create Variable defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on variables when they are created */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bEnableVariableDefaults;

	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	bool bApplyVariableDefaultsToEventDispatchers;

	/* Variable default Instance Editable */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	bool bDefaultVariableInstanceEditable;

	/* Variable default Blueprint Read Only */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	bool bDefaultVariableBlueprintReadOnly;

	/* Variable default Expose on Spawn */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	bool bDefaultVariableExposeOnSpawn;

	/* Variable default Private */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	bool bDefaultVariablePrivate;

	/* Variable default Expose to Cinematics */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	bool bDefaultVariableExposeToCinematics;

	/* Variable default name */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	FString DefaultVariableName;

	/* Variable default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	FText DefaultVariableTooltip;

	/* Variable default Category */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults, meta = (EditCondition = "bEnableVariableDefaults"))
	FText DefaultVariableCategory;

	////////////////////////////////////////////////////////////
	// Create function defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on functions when they are created */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	bool bEnableFunctionDefaults;

	/* Function default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	EBAFunctionAccessSpecifier DefaultFunctionAccessSpecifier;

	/* Function default Pure */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	bool bDefaultFunctionPure;
	
	/* Function default Const */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	bool bDefaultFunctionConst;

	/* Function default Exec */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	bool bDefaultFunctionExec;

	/* Function default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	FText DefaultFunctionTooltip;

	/* Function default Keywords */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	FText DefaultFunctionKeywords;

	/* Function default Category */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	FText DefaultFunctionCategory;

	////////////////////////////////////////////////////////////
	// Custom event defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on custom events when they are created */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults)
	bool bEnableEventDefaults;

	/* Event default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (EditCondition = "bEnableEventDefaults"))
	EBAFunctionAccessSpecifier DefaultEventAccessSpecifier;

	/* Event default Net Reliable (for RPC calls) */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (EditCondition = "bEnableEventDefaults"))
	bool bDefaultEventNetReliable;

	////////////////////////////////////////////////////////////
	// Inputs
	////////////////////////////////////////////////////////////

	/* Copy the pin value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs")
	FInputChord CopyPinValueChord;

	/* Paste the hovered value to the clipboard */
	UPROPERTY(EditAnywhere, config, Category = "Inputs")
	FInputChord PastePinValueChord;

	/* Focus the hovered node in the details panel */
	UPROPERTY(EditAnywhere, config, Category = "Inputs")
	FInputChord FocusInDetailsPanelChord;

	/** Extra input chords to for dragging selected nodes with cursor (same as left-click-dragging) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features")
	TArray<FInputChord> AdditionalDragNodesChords;

	/** Input chords for group dragging (move all linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features")
	TArray<FInputChord> GroupMovementChords;

	/** Input chords for group dragging (move left linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features")
	TArray<FInputChord> LeftSubTreeMovementChords;

	/** Input chords for group dragging (move right linked nodes) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features")
	TArray<FInputChord> RightSubTreeMovementChords;


	////////////////////////////////////////////////////////////
	// Misc
	////////////////////////////////////////////////////////////

	/* By default the Blueprint Assist Hotkey Menu only displays this plugin's hotkeys. Enable this to display all hotkeys for the editor. */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	bool bDisplayAllHotkeys;

	/* Show the welcome screen when the engine launches */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	bool bShowWelcomeScreenOnLaunch;

	/* Double click on a node to go to definition. Currently only implemented for Cast blueprint node. */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	bool bEnableDoubleClickGoToDefinition;

	/* Knot nodes will be hidden (requires graphs to be re-opened) */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	bool bEnableInvisibleKnotNodes;

	/* Play sound on successful compile */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	bool bPlayLiveCompileSound;

	/** Input for folder bookmarks */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	TArray<FKey> FolderBookmarks;

	/** Duration to differentiate between a click and a drag */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	float ClickTime;

	/* What category to assign to generated getter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	FText DefaultGeneratedGettersCategory;

	/* What category to assign to generated setter functions. Overrides DefaultFunctionCategory. */
	UPROPERTY(EditAnywhere, config, Category = "Misc")
	FText DefaultGeneratedSettersCategory;

	FORCEINLINE static const UBASettings_EditorFeatures& Get() { return *GetDefault<UBASettings_EditorFeatures>(); }
	FORCEINLINE static UBASettings_EditorFeatures& GetMutable() { return *GetMutableDefault<UBASettings_EditorFeatures>(); }

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};

class FBASettingsDetails_EditorFeatures final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};