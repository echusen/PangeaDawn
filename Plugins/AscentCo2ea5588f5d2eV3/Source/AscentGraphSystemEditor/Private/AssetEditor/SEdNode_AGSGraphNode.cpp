// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "SEdNode_AGSGraphNode.h"
#include "AGSGraphEditorPCH.h"
#include "Colors_AGSGraph.h"
#include "EdNode_AGSGraphNode.h"
#include "GraphEditorSettings.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "SLevelOfDetailBranchNode.h"
#include "ScopedTransaction.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "EdNode_AGSGraph"

//////////////////////////////////////////////////////////////////////////
class SAGSGraphPin : public SGraphPin {
public:
    SLATE_BEGIN_ARGS(SAGSGraphPin) { }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
    {
        this->SetCursor(EMouseCursor::Default);

        bShowLabel = true;

        GraphPinObj = InPin;
        check(GraphPinObj != nullptr);

        const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
        check(Schema);

        SBorder::Construct(SBorder::FArguments()
                .BorderImage(this, &SAGSGraphPin::GetPinBorder)
                .BorderBackgroundColor(this, &SAGSGraphPin::GetPinColor)
                .OnMouseButtonDown(this, &SAGSGraphPin::OnPinMouseDown)
                .Cursor(this, &SAGSGraphPin::GetPinCursor)
                .Padding(FMargin(10.0f)));
    }

protected:
    virtual FSlateColor GetPinColor() const override
    {
        return AGSGraphColors::Pin::Default;
    }

    virtual TSharedRef<SWidget> GetDefaultValueWidget() override
    {
        return SNew(STextBlock);
    }

    const FSlateBrush* GetPinBorder() const
    {
        return FAppStyle::GetBrush(TEXT("Graph.StateNode.Body"));
    }
};

//////////////////////////////////////////////////////////////////////////
void SEdNode_AGSGraphNode::Construct(const FArguments& InArgs, UEdNode_AGSGraphNode* InNode)
{
    GraphNode = InNode;
    UpdateGraphNode();
    InNode->SEdNode = this;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SEdNode_AGSGraphNode::UpdateGraphNode()
{
    const FMargin NodePadding = FMargin(2.0f);

    InputPins.Empty();
    OutputPins.Empty();

    // Reset variables that are going to be exposed, in case we are refreshing an already setup node.
    RightNodeBox.Reset();
    LeftNodeBox.Reset();
    OutputPinBox.Reset();

    TSharedPtr<SErrorText> ErrorText;
    TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

    this->ContentScale.Bind(this, &SGraphNode::GetContentScale);
    this->GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
            [SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
                    .Padding(0.0f)
                    .BorderBackgroundColor(this, &SEdNode_AGSGraphNode::GetBorderBackgroundColor)
                        [SNew(SOverlay)

                            // Pins and node details
                            + SOverlay::Slot()
                                .HAlign(HAlign_Fill)
                                .VAlign(VAlign_Fill)
                                    [SNew(SVerticalBox)

                                        // INPUT PIN AREA
                                        + SVerticalBox::Slot()
                                            .AutoHeight()
                                                [SNew(SBox)
                                                        .MinDesiredHeight(NodePadding.Top)
                                                            [SAssignNew(LeftNodeBox, SVerticalBox)]]

                                        // STATE NAME AREA
                                        + SVerticalBox::Slot()
                                            .Padding(FMargin(NodePadding.Left, 0.0f, NodePadding.Right, 0.0f))
                                                [SNew(SVerticalBox)
                                                    + SVerticalBox::Slot()
                                                        .AutoHeight()
                                                            [SAssignNew(NodeBody, SBorder)
                                                                    .BorderImage(FAppStyle::GetBrush("BTEditor.Graph.BTNode.Body"))
                                                                    .BorderBackgroundColor(this, &SEdNode_AGSGraphNode::GetBackgroundColor)
                                                                    .HAlign(HAlign_Fill)
                                                                    .VAlign(VAlign_Center)
                                                                    .Visibility(EVisibility::SelfHitTestInvisible)
                                                                        [SNew(SOverlay)
                                                                            + SOverlay::Slot()
                                                                                .HAlign(HAlign_Fill)
                                                                                .VAlign(VAlign_Fill)
                                                                                    [SNew(SVerticalBox)
                                                                                        + SVerticalBox::Slot()
                                                                                            .AutoHeight()
                                                                                                [SNew(SHorizontalBox)
                                                                                                    + SHorizontalBox::Slot()
                                                                                                        .AutoWidth()
                                                                                                            [
                                                                                                                // POPUP ERROR MESSAGE
                                                                                                                SAssignNew(ErrorText, SErrorText)
                                                                                                                    .BackgroundColor(this, &SEdNode_AGSGraphNode::GetErrorColor)
                                                                                                                    .ToolTipText(this, &SEdNode_AGSGraphNode::GetErrorMsgToolTip)]

                                                                                                    + SHorizontalBox::Slot()
                                                                                                        .AutoWidth()
                                                                                                            [SNew(SHorizontalBox)
                                                                                                                + SHorizontalBox::Slot()
                                                                                                                    .Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
                                                                                                                        [SNew(SVerticalBox)
                                                                                                                            + SVerticalBox::Slot()
                                                                                                                                .AutoHeight()
                                                                                                                                .Padding(FMargin(4.0f, 4.f, 2., 4.))
                                                                                                                                    [SAssignNew(InlineEditableText, SInlineEditableTextBlock)
                                                                                                                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                                                                                                                                            .Text(NodeTitle.Get(), &SNodeTitle::GetHeadTitle)
                                                                                                                                            .OnVerifyTextChanged(this, &SEdNode_AGSGraphNode::OnVerifyNameTextChanged)
                                                                                                                                            .OnTextCommitted(this, &SEdNode_AGSGraphNode::OnNameTextCommited)
                                                                                                                                            .IsReadOnly(this, &SEdNode_AGSGraphNode::IsNameReadOnly)
                                                                                                                                            .IsSelected(this, &SEdNode_AGSGraphNode::IsSelectedExclusively)
                                                                                                                                            .Justification(ETextJustify::Center)]
                                                                                                                            + SVerticalBox::Slot()
                                                                                                                                .AutoHeight()
                                                                                                                                .Padding(FMargin(4.0f, 2.f, 4., 4.))
                                                                                                                                    [SAssignNew(InlineEditableText, SInlineEditableTextBlock)
                                                                                                                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                                                                                                                            .Text(this, &SEdNode_AGSGraphNode::GetParagraphTitle)
                                                                                                                                            //	.OnVerifyTextChanged(this, &SEdNode_AGSGraphNode::OnVerifyNameTextChanged)
                                                                                                                                            .OnTextCommitted(this, &SEdNode_AGSGraphNode::OnNameTextCommited)
                                                                                                                                            .IsReadOnly(this, &SEdNode_AGSGraphNode::IsNameReadOnly)

    ]
                                                                                                                            + SVerticalBox::Slot()
                                                                                                                                .AutoHeight()
                                                                                                                                    [NodeTitle.ToSharedRef()]]]]]]]]

                                        // OUTPUT PIN AREA
                                        + SVerticalBox::Slot()
                                            .AutoHeight()
                                                [SNew(SBox)
                                                        .MinDesiredHeight(NodePadding.Bottom)
                                                            [SAssignNew(RightNodeBox, SVerticalBox)
                                                                + SVerticalBox::Slot()
                                                                    .HAlign(HAlign_Fill)
                                                                    .VAlign(VAlign_Fill)
                                                                    .Padding(20.0f, 0.0f)
                                                                    .FillHeight(1.0f)
                                                                        [SAssignNew(OutputPinBox, SHorizontalBox)]]]]]];

    // Create comment bubble
    TSharedPtr<SCommentBubble> CommentBubble;
    const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

    SAssignNew(CommentBubble, SCommentBubble)
        .GraphNode(GraphNode)
        .Text(this, &SGraphNode::GetNodeComment)
        .OnTextCommitted(this, &SGraphNode::OnCommentTextCommitted)
        .ColorAndOpacity(CommentColor)
        .AllowPinning(true)
        .EnableTitleBarBubble(true)
        .EnableBubbleCtrls(true)
        .GraphLOD(this, &SGraphNode::GetCurrentLOD)
        .IsGraphNodeHovered(this, &SGraphNode::IsHovered);

    GetOrAddSlot(ENodeZone::TopCenter)
        .SlotOffset2f(TAttribute<FVector2f>(CommentBubble.Get(), &SCommentBubble::GetOffset2f))
        .SlotSize2f(TAttribute<FVector2f>(CommentBubble.Get(), &SCommentBubble::GetSize2f))
        .AllowScaling(TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
        .VAlign(VAlign_Top)
            [CommentBubble.ToSharedRef()];

    ErrorReporting = ErrorText;
    ErrorReporting->SetError(ErrorMsg);
    CreatePinWidgets();
}

void SEdNode_AGSGraphNode::CreatePinWidgets()
{
    UEdNode_AGSGraphNode* StateNode = CastChecked<UEdNode_AGSGraphNode>(GraphNode);

    for (int32 PinIdx = 0; PinIdx < StateNode->Pins.Num(); PinIdx++) {
        UEdGraphPin* MyPin = StateNode->Pins[PinIdx];
        if (!MyPin->bHidden) {
            TSharedPtr<SGraphPin> NewPin = SNew(SAGSGraphPin, MyPin);

            AddPin(NewPin.ToSharedRef());
        }
    }
}

void SEdNode_AGSGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
    PinToAdd->SetOwner(SharedThis(this));

    const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
    const bool bAdvancedParameter = PinObj && PinObj->bAdvancedView;
    if (bAdvancedParameter) {
        PinToAdd->SetVisibility(TAttribute<EVisibility>(PinToAdd, &SGraphPin::IsPinVisibleAsAdvanced));
    }

    if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input) {
        LeftNodeBox->AddSlot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            .FillHeight(1.0f)
            .Padding(20.0f, 0.0f)
                [PinToAdd];
        InputPins.Add(PinToAdd);
    } else // Direction == EEdGraphPinDirection::EGPD_Output
    {
        OutputPinBox->AddSlot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            .FillWidth(1.0f)
                [PinToAdd];
        OutputPins.Add(PinToAdd);
    }
}

bool SEdNode_AGSGraphNode::IsNameReadOnly() const
{
    UEdNode_AGSGraphNode* EdNode_Node = Cast<UEdNode_AGSGraphNode>(GraphNode);
    check(EdNode_Node != nullptr);

    UAGSGraph* AGSGraph = EdNode_Node->AGSGraphNode->Graph;
    check(AGSGraph != nullptr);

    return !AGSGraph->bCanRenameNode || SGraphNode::IsNameReadOnly();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SEdNode_AGSGraphNode::OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
    SGraphNode::OnNameTextCommited(InText, CommitInfo);

    UEdNode_AGSGraphNode* MyNode = CastChecked<UEdNode_AGSGraphNode>(GraphNode);

    if (MyNode != nullptr && MyNode->AGSGraphNode != nullptr) {
        const FScopedTransaction Transaction(LOCTEXT("AGSGraphEditorRenameNode", "Generic Graph Editor: Rename Node"));
        MyNode->Modify();
        MyNode->AGSGraphNode->Modify();
        //	MyNode->AGSGraphNode->SetNodeTitle(InText);
        UpdateGraphNode();
    }
}

FText SEdNode_AGSGraphNode::GetParagraphTitle() const
{
    UEdNode_AGSGraphNode* MyNode = CastChecked<UEdNode_AGSGraphNode>(GraphNode);

    return MyNode ? MyNode->GetParagraphTitle() : FText::FromString("Implement me!");
}

FSlateColor SEdNode_AGSGraphNode::GetBorderBackgroundColor() const
{
    UEdNode_AGSGraphNode* MyNode = CastChecked<UEdNode_AGSGraphNode>(GraphNode);
    return MyNode ? MyNode->GetBackgroundColor() : AGSGraphColors::NodeBorder::HighlightAbortRange0;
}

FSlateColor SEdNode_AGSGraphNode::GetBackgroundColor() const
{
    return AGSGraphColors::NodeBody::Default;
}

EVisibility SEdNode_AGSGraphNode::GetDragOverMarkerVisibility() const
{
    return EVisibility::Visible;
}

const FSlateBrush* SEdNode_AGSGraphNode::GetNameIcon() const
{
    return FAppStyle::GetBrush(TEXT("BTEditor.Graph.BTNode.Icon"));
}

#undef LOCTEXT_NAMESPACE
