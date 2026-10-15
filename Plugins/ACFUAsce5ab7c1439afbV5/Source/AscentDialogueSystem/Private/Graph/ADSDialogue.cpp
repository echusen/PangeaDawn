// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Graph/ADSDialogue.h"
#include "ADSDialoguePartecipantComponent.h"
#include "ADSDialogueDeveloperSettings.h"
#include "ADSDialogueSubsystem.h"
#include "AGSGraphNode.h"
#include "Graph/ADSDialogueNode.h"
#include "Graph/ADSDialogueResponseNode.h"
#include "Graph/ADSGraphEdge.h"
#include "Graph/ADSGraphNode.h"
#include "Graph/ADSStartDialogueNode.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Paths.h"
#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>

UADSDialogue::UADSDialogue()
{
	NodeType = UADSGraphNode::StaticClass();
	EdgeType = UADSGraphEdge::StaticClass();
	UseDefaultCameraSettings = true;
}

UAGSGraphNode* UADSDialogue::StartDialogue(class APlayerController* inController, const TArray<UADSBaseDialoguePartecipantComponent*>& participants)
{
	ClearPauseState();
	controller = inController;
	DeactivateAllNodes();
	partecipantsRef.Empty();
	for (const auto& participant : participants) {
		partecipantsRef.Add(participant->GetParticipantTag(), participant);
		UADSDialoguePartecipantComponent* partecipant = Cast<UADSDialoguePartecipantComponent>(participant);
		if (partecipant) {
			partecipant->RegisterDialogueEvents(this);
		}
	}

	if (partecipantsRef.Num() == 0) {
		UE_LOG(LogTemp, Error, TEXT("Missing Partecipants! - UADSDialogue::StartDialogue"));
		return nullptr;
	}

	for (UAGSGraphNode* root : RootNodes) {
		UADSStartDialogueNode* startNode = Cast<UADSStartDialogueNode>(root);
		if (startNode) {
			if (startNode->CanBeActivated(controller)) {
				currentDialogueStart = startNode;
				currentNode = startNode;
				
				bIsStarted = true;
				ensure(ActivateNode(startNode));
				OnDialogueStarted.Broadcast(this);
				return startNode;
			}
		}
	}

	return nullptr;
}

bool UADSDialogue::ActivateNode(class UAGSGraphNode* node)
{
	if (node && AllNodes.Contains(node)) {
		if (currentNode) {
			currentNode->DeactivateNode();
		}

		currentNode = Cast<UADSGraphNode>(node);
		OnDialogueNodeActivated.Broadcast(currentNode->GetNodeId());
	}
	return Super::ActivateNode(node);
}

TArray<UADSDialogueResponseNode*> UADSDialogue::GetAllButtonAnswersForCurrentNode() const
{
	if (bPauseRequested || bIsPaused) {
		return TArray<class UADSDialogueResponseNode*>();
	}

	UADSDialogueNode* currentDialogueNode = Cast<UADSDialogueNode>(currentNode);
	if (currentDialogueNode) {
		return currentDialogueNode->GetAllValidAnswers(controller);
	}

	return TArray<class UADSDialogueResponseNode*>();
}

class UADSDialogueNode* UADSDialogue::MoveToNextNode()
{
	if (!bIsStarted) {
		return nullptr;
	}

	if (bIsPaused) {
		return nullptr;
	}

	if (bPauseRequested) {
		if (IsCurrentNodeWaitingToFinish()) {
			return nullptr;
		}
		EnterPausedState();
		return nullptr;
	}

	if (currentNode) {
		for (UAGSGraphNode* child : currentNode->ChildrenNodes) {
			UADSDialogueNode* response = Cast<UADSDialogueNode>(child);
			if (response && response->CanBeActivated(controller)) {
				const UADSGraphEdge* edge = Cast<UADSGraphEdge>(currentNode->GetEdge(response));
				if (edge && edge->CanBeActivated(controller)) {
					ensure(ActivateNode(response));
					return response;
				}
			}
		}
		EndDialogue();
	}

	return nullptr;
}

bool UADSDialogue::PauseDialogue()
{
	if (!bIsStarted) {
		return false;
	}
	if (bIsPaused) {
		return true;
	}

	bPauseRequested = true;
	if (!IsCurrentNodeWaitingToFinish()) {
		EnterPausedState();
	}
	return true;
}

bool UADSDialogue::ResumeDialogue()
{
	if (!bIsStarted || (!bPauseRequested && !bIsPaused)) {
		return false;
	}

	const bool bNodeStillPlaying = IsCurrentNodeWaitingToFinish();
	ClearPauseState();
	OnDialogueResumed.Broadcast(this);

	if (bNodeStillPlaying) {
		return true;
	}

	if (UADSDialogueNode* DialogueNode = Cast<UADSDialogueNode>(currentNode)) {
		if (DialogueNode->GetAllValidAnswers(controller).Num() > 0) {
			return true;
		}
	}

	MoveToNextNode();
	return true;
}

bool UADSDialogue::IsCurrentNodeWaitingToFinish() const
{
	const UADSDialogueNode* DialogueNode = Cast<UADSDialogueNode>(currentNode);
	return DialogueNode && DialogueNode->HasPendingAutoProgress();
}

void UADSDialogue::EnterPausedState()
{
	if (bIsPaused) {
		return;
	}

	bPauseRequested = false;
	bIsPaused = true;
	OnDialoguePaused.Broadcast(this);
}

void UADSDialogue::ClearPauseState()
{
	bPauseRequested = false;
	bIsPaused = false;
}

class UADSBaseDialoguePartecipantComponent* UADSDialogue::FindPartecipant(FGameplayTag partecipantTag) const
{
	if (HasPartecipant(partecipantTag)) {
		return *(partecipantsRef.Find(partecipantTag));
	}
	return nullptr;
}

void UADSDialogue::EndDialogue()
{
	ClearPauseState();
	bIsStarted = false;
	DeactivateAllNodes();
	if (currentDialogueStart) {
		currentDialogueStart->ExecuteEndingActions();
	}
	OnDialogueEnded.Broadcast(this);
}

#if WITH_EDITOR
namespace ADSDialoguePathHelper
{
	FString NormalizeToGamePackageDir(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty())
		{
			return FString();
		}

		if (Path.StartsWith(TEXT("/Game/")) || Path.Equals(TEXT("/Game")))
		{
			Path.RemoveFromStart(TEXT("/Game"));
			Path.RemoveFromStart(TEXT("/"));
		}
		else if (Path.StartsWith(TEXT("Game/")))
		{
			Path.RemoveFromStart(TEXT("Game/"));
		}
		else if (Path.Contains(TEXT(":")) || Path.StartsWith(TEXT("//")))
		{
			const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
			const FString FullPath = FPaths::ConvertRelativePathToFull(Path);
			if (FullPath.StartsWith(ContentDir))
			{
				Path = FullPath.RightChop(ContentDir.Len());
				Path.ReplaceInline(TEXT("\\"), TEXT("/"));
			}
		}

		while (Path.StartsWith(TEXT("/")))
		{
			Path.RightChopInline(1);
		}

		if (Path.IsEmpty())
		{
			return FString();
		}
		if (!Path.EndsWith(TEXT("/")))
		{
			Path += TEXT("/");
		}
		return TEXT("/Game/") + Path;
	}

	FString CoalescePackageDir(const FString& CustomPath, const FString& DefaultRelativeToContent)
	{
		const FString NormalizedCustom = NormalizeToGamePackageDir(CustomPath);
		if (!NormalizedCustom.IsEmpty())
		{
			return NormalizedCustom;
		}
		FString NormalizedDefault = NormalizeToGamePackageDir(DefaultRelativeToContent);
		if (NormalizedDefault.IsEmpty())
		{
			NormalizedDefault = TEXT("/Game/");
		}
		return NormalizedDefault;
	}

	FString AppendDialogueSubfolder(const UADSDialogue& Dialogue, FString BaseDir)
	{
		if (!BaseDir.EndsWith(TEXT("/")))
		{
			BaseDir += TEXT("/");
		}

		FString Sanitized;
		const FString FolderName = Dialogue.GetName();
		for (int32 i = 0; i < FolderName.Len(); ++i)
		{
			const TCHAR Ch = FolderName[i];
			if (FChar::IsAlnum(Ch) || Ch == TEXT('_'))
			{
				Sanitized += Ch;
			}
		}
		if (Sanitized.IsEmpty())
		{
			Sanitized = TEXT("Dialogue");
		}

		FString Trimmed = BaseDir;
		Trimmed.RemoveFromEnd(TEXT("/"));
		FString LastSegment;
		Trimmed.Split(TEXT("/"), nullptr, &LastSegment, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastSegment.Equals(Sanitized, ESearchCase::IgnoreCase))
		{
			return BaseDir;
		}

		return BaseDir + Sanitized + TEXT("/");
	}
}

FString UADSDialogue::ResolveVoiceOutputPackagePath() const
{
	const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
	const FString DefaultPath = Settings ? Settings->GetFinalTTSPath() : TEXT("Audio/TTS/Generated/");
#if WITH_EDITORONLY_DATA
	const FString BaseDir = ADSDialoguePathHelper::CoalescePackageDir(VoiceOutputFolder.Path, DefaultPath);
#else
	const FString BaseDir = ADSDialoguePathHelper::NormalizeToGamePackageDir(DefaultPath);
#endif
	return ADSDialoguePathHelper::AppendDialogueSubfolder(*this, BaseDir);
}

FString UADSDialogue::ResolveAnimationOutputPackagePath() const
{
	const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
	const FString DefaultPath = Settings ? Settings->GetFacialAnimationOutputPath() : TEXT("Animation/FacialAnim/Generated/");
#if WITH_EDITORONLY_DATA
	const FString BaseDir = ADSDialoguePathHelper::CoalescePackageDir(AnimationOutputFolder.Path, DefaultPath);
#else
	const FString BaseDir = ADSDialoguePathHelper::NormalizeToGamePackageDir(DefaultPath);
#endif
	return ADSDialoguePathHelper::AppendDialogueSubfolder(*this, BaseDir);
}

FString UADSDialogue::ResolveARKitOutputPackagePath() const
{
	const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
#if WITH_EDITORONLY_DATA
	if (!AnimationOutputFolder.Path.IsEmpty())
	{
		return ResolveAnimationOutputPackagePath() + TEXT("ARKit/");
	}
#endif
	const FString DefaultPath = Settings ? Settings->GetARKitAnimationOutputPath() : TEXT("Animation/FacialAnim/Generated/ARKit/");
	const FString BaseDir = ADSDialoguePathHelper::CoalescePackageDir(FString(), DefaultPath);
	return ADSDialoguePathHelper::AppendDialogueSubfolder(*this, BaseDir);
}
#endif
