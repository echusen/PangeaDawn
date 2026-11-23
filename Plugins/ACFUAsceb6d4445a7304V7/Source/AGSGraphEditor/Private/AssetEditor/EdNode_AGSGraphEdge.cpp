// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved. 

#include "EdNode_AGSGraphEdge.h"
#include "AGSGraphEdge.h"
#include "EdNode_AGSGraphNode.h"

#define LOCTEXT_NAMESPACE "EdNode_AGSGraphEdge"

void UEdNode_AGSGraphEdge::SetEdge(UAGSGraphEdge* Edge)
{
	AGSGraphEdge = Edge;
}

void UEdNode_AGSGraphEdge::AllocateDefaultPins()
{
	UEdGraphPin* Inputs = CreatePin(EGPD_Input, TEXT("Edge"), FName(), TEXT("In"));
	Inputs->bHidden = true;
	UEdGraphPin* Outputs = CreatePin(EGPD_Output, TEXT("Edge"), FName(), TEXT("Out"));
	Outputs->bHidden = true;
}

FText UEdNode_AGSGraphEdge::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText();
}

void UEdNode_AGSGraphEdge::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (!IsValid(this) || HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return;
	}

	if (Pin && Pin->LinkedTo.Num() == 0)
	{
		// Do not Modify() the parent graph here: UEdGraph_AGSGraph::Modify walks every
		// node and the engine then calls GetWorld() on objects already being destroyed.
		Modify();
		DestroyNode();
	}
}

void UEdNode_AGSGraphEdge::PrepareForCopying()
{
	AGSGraphEdge->Rename(nullptr, this, REN_DontCreateRedirectors | REN_DoNotDirty);
}

void UEdNode_AGSGraphEdge::CreateConnections(UEdNode_AGSGraphNode* Start, UEdNode_AGSGraphNode* End)
{
	Pins[0]->Modify();
	Pins[0]->LinkedTo.Empty();

	Start->GetOutputPin()->Modify();
	Pins[0]->MakeLinkTo(Start->GetOutputPin());

	// This to next
	Pins[1]->Modify();
	Pins[1]->LinkedTo.Empty();

	End->GetInputPin()->Modify();
	Pins[1]->MakeLinkTo(End->GetInputPin());
}

UEdNode_AGSGraphNode* UEdNode_AGSGraphEdge::GetStartNode()
{
	if (Pins[0]->LinkedTo.Num() > 0)
	{
		return Cast<UEdNode_AGSGraphNode>(Pins[0]->LinkedTo[0]->GetOwningNode());
	}
	else
	{
		return nullptr;
	}
}

UEdNode_AGSGraphNode* UEdNode_AGSGraphEdge::GetEndNode()
{
	if (Pins[1]->LinkedTo.Num() > 0)
	{
		return Cast<UEdNode_AGSGraphNode>(Pins[1]->LinkedTo[0]->GetOwningNode());
	}
	else
	{
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE

