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

USTRUCT()
struct FBAVariableDefaults
{
	GENERATED_BODY()

	FBAVariableDefaults();

	/* Variable default Private */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariablePrivate;

	/* Variable default Instance Editable */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariableInstanceEditable;

	/* Variable default Blueprint Read Only */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariableBlueprintReadOnly;

	/* Variable default Expose on Spawn */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariableExposeOnSpawn;

	/* Variable default Expose to Cinematics */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariableExposeToCinematics;

	/* Variable default Transient */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariableTransient;

	/* Variable default Save Game */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariableSaveGame;

	/* Variable default Advanced Display */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultVariableAdvancedDisplay;

	/* Variable default Config Variable */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bDefaultConfigVariable;

	/* Variable default Name */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	FString DefaultVariableName;

	/* Variable default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	FText DefaultVariableTooltip;

	/* Variable default Category */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	FText DefaultVariableCategory;

	/* Should these defaults apply to event dispatchers? */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	bool bApplyVariableDefaultsToEventDispatchers;
};

USTRUCT()
struct FBAFunctionDefaults
{
	GENERATED_BODY()

	FBAFunctionDefaults();

	/* Function default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	EBAFunctionAccessSpecifier DefaultFunctionAccessSpecifier;

	/* Function default Pure */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	bool bDefaultFunctionPure;
	
	/* Function default Const */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	bool bDefaultFunctionConst;

	/* Function default Exec */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	bool bDefaultFunctionExec;

	/* Function default ThreadSafe */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	bool bDefaultFunctionThreadSafe;

	/* Function default Tooltip */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	FText DefaultFunctionTooltip;

	/* Function default Keywords */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	FText DefaultFunctionKeywords;

	/* Function default Category */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	FText DefaultFunctionCategory;
};

USTRUCT()
struct FBACustomEventDefaults
{
	GENERATED_BODY()

	FBACustomEventDefaults();

	/* Event default AccessSpecifier */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults)
	EBAFunctionAccessSpecifier DefaultEventAccessSpecifier;

	/* Event default Net Reliable (for RPC calls) */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults)
	bool bDefaultEventNetReliable;
};

UENUM()
enum class EBAAffixType : uint8
{
	Prefix UMETA(DisplayName = "Prefix", Tooltip = "At the start"),
	Suffix UMETA(DisplayName = "Suffix", Tooltip = "At the end"),
};

USTRUCT()
struct FBAStringAffix
{
	GENERATED_BODY()

	FBAStringAffix() = default;
	FBAStringAffix(const FString& Affix) : AffixValue(Affix) {}

	UPROPERTY(EditAnywhere, config, Category = Default)
	FString AffixValue;

	UPROPERTY(EditAnywhere, config, Category = Default)
	EBAAffixType AffixType = EBAAffixType::Prefix;

	bool Matches(const FString& String) const
	{
		return (AffixType == EBAAffixType::Prefix) ? String.StartsWith(AffixValue) : String.EndsWith(AffixValue);
	}

	void AddAffix(FString& String) const
	{
		String = (AffixType == EBAAffixType::Prefix) ? (AffixValue + String) : (String + AffixValue);
	}

	bool RemoveAffix(FString& String) const
	{
		return (AffixType == EBAAffixType::Prefix) ? String.RemoveFromStart(AffixValue) : String.RemoveFromEnd(AffixValue);
	}
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

	/* Set the according replication flags after renaming a custom event by matching the affixes below */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	bool bSetReplicationFlagsAfterRenaming;

	/* If there is no matching affix in the title, apply "NotReplicated" */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication, meta=(EditCondition="bSetReplicationFlagsAfterRenaming"))
	bool bClearReplicationFlagsWhenRenaming;

	/* Add the according affix to the title after changing replication flags */
	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	bool bAddReplicationAffixToCustomEventTitle;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	FBAStringAffix MulticastAffix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	FBAStringAffix ServerAffix;

	UPROPERTY(EditAnywhere, Config, Category = CustomEventReplication)
	FBAStringAffix ClientAffix;

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
	FBAVariableDefaults VariableDefaults;

	/* When holding down this key while you create a new variable, these defaults will apply */
	UPROPERTY(EditAnywhere, config, Category = VariableDefaults)
	TMap<FKey, FBAVariableDefaults> VariableDefaultsHotkeys;

	////////////////////////////////////////////////////////////
	// Function defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on functions when they are created */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	bool bEnableFunctionDefaults;

	/* Function defaults */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults, meta = (EditCondition = "bEnableFunctionDefaults"))
	FBAFunctionDefaults FunctionDefaults;

	/* When holding down this key while you create a new function, these defaults will apply */
	UPROPERTY(EditAnywhere, config, Category = FunctionDefaults)
	TMap<FKey, FBAFunctionDefaults> FunctionDefaultsHotkeys;

	////////////////////////////////////////////////////////////
	// Custom event defaults
	////////////////////////////////////////////////////////////

	/* Set default properties on custom events when they are created */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults)
	bool bEnableCustomEventDefaults;

	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults, meta = (EditCondition = "bEnableCustomEventDefaults"))
	FBACustomEventDefaults CustomEventDefaults;

	/* When holding down this key while you create a new custom event, these defaults will apply */
	UPROPERTY(EditAnywhere, config, Category = CustomEventDefaults)
	TMap<FKey, FBACustomEventDefaults> CustomEventDefaultsHotkeys;

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

	/* Used smart wire operation, if this key is held then pin conversions are allowed */
	UPROPERTY(EditAnywhere, config, Category = "Inputs")
	FKey AllowConversionConnectionsModifier;

	/* Enter key moves focus between pin value widgets */
	UPROPERTY(EditAnywhere, config, Category = "Inputs")
	bool bEnterCyclesPinValues;

	/* Enter key tries to interact with the selected or hovered pin widget */
	UPROPERTY(EditAnywhere, config, Category = "Inputs")
	bool bEnterInteractsWithPin;

	/* Tab key moves focus between pin value widgets */
	UPROPERTY(EditAnywhere, config, Category = "Inputs")
	bool bTabCyclePinValues;

	/** Extra input chords to for dragging selected nodes with cursor (same as left-click-dragging) */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features")
	TArray<FInputChord> AdditionalDragNodesChords;

	/** Whether AdditionalDragNodesChords require you to have your mouse over a node */
	UPROPERTY(EditAnywhere, config, Category = "Input|Mouse Features")
	bool bDragRequiresNodeUnderCursor;

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