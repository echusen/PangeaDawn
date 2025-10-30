// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "Widgets/SBoxPanel.h"

class UEdNode_AGSGraphNode;

class AGSGRAPHEDITOR_API SEdNode_AGSGraphNode : public SGraphNode {
public:
	SLATE_BEGIN_ARGS(SEdNode_AGSGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdNode_AGSGraphNode* InNode);

	virtual void UpdateGraphNode() override;
	virtual void CreatePinWidgets() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual bool IsNameReadOnly() const override;

	void OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo);

	virtual FText GetParagraphTitle() const;

	virtual FSlateColor GetBorderBackgroundColor() const;
	virtual FSlateColor GetBackgroundColor() const;

	virtual EVisibility GetDragOverMarkerVisibility() const;

	virtual const FSlateBrush* GetNameIcon() const;

protected:
	TSharedPtr<SBorder> NodeBody;
	TSharedPtr<SHorizontalBox> OutputPinBox;
};
