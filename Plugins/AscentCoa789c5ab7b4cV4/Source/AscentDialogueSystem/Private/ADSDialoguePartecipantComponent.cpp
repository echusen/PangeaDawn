// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ADSDialoguePartecipantComponent.h"

#include "ADSDialogueDeveloperSettings.h"
#include "ADSDialogueFunctionLibrary.h"
#include "ADSDialogueMasterComponent.h"
#include "ADSDialogueSubsystem.h"
#include <Animation/AnimInstance.h>
#include <Components/SkeletalMeshComponent.h>
#include <Engine/GameInstance.h>
#include <GameFramework/Character.h>
#include <Kismet/GameplayStatics.h>

// Sets default values for this component's properties
UADSDialoguePartecipantComponent::UADSDialoguePartecipantComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	ParticipantNameText = FText::FromString("Default Name");
}

// Called when the game starts
void UADSDialoguePartecipantComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UADSDialogueSubsystem* adsSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
		adsSubsystem->RegisterParticipant(this);
	}

	
	for (const auto& dialogue : Dialogues) {
		UADSDialogue* newDialogue = DuplicateObject(dialogue, GetOuter());
		if (newDialogue) {
			instancedDialogues.Add(newDialogue);
		}
	}
}

void UADSDialoguePartecipantComponent::EndPlay(EEndPlayReason::Type reason)
{
	Super::EndPlay(reason);
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	if (UADSDialogueSubsystem* adsSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
		adsSubsystem->UnregisterParticipant(PartecipantTag);
	}

	/*
	for (const auto& dialogue : instancedDialogues) {
		if (dialogue) {
			dialogue->OnDialogueStarted.RemoveDynamic(this, &UADSDialoguePartecipantComponent::HandleDialogueStarted);
			dialogue->OnDialogueEnded.RemoveDynamic(this, &UADSDialoguePartecipantComponent::HandleDialogueEnded);
		}
	}*/
}

bool UADSDialoguePartecipantComponent::TryStartDialogue(const TArray<UADSBaseDialoguePartecipantComponent*>& participants, UADSDialogue* dialogueToStart)
{
	if (!IsValid(dialogueToStart)) {
		UE_LOG(LogTemp, Error, TEXT("No ADS Dialogue Set!- UADSDialoguePartecipantComponent::TryStartDialogue"));
		return false;
	}
	if (participants.Num() == 0) {
		UE_LOG(LogTemp, Error, TEXT("No participants provided to TryStartDialogue!"));
		return false;
	}
	for (int32 i = 0; i < participants.Num(); ++i) {
		if (!IsValid(participants[i])) {
			UE_LOG(LogTemp, Error, TEXT("Participant at index %d is invalid!"), i);
		}
		else {
			FText name = participants[i]->GetParticipantName();
			if (name.IsEmpty() || name.ToString().Equals(TEXT("Unknown"))) {
				UE_LOG(LogTemp, Warning, TEXT("Participant at index %d has unknown name!"), i);
			}
			else {
				UE_LOG(LogTemp, Log, TEXT("Participant at index %d: %s"), i, *name.ToString());
			}
		}
	}
	RegisterDialogueEvents(dialogueToStart);

	if (UADSDialogueSubsystem* adsSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
		adsSubsystem->StopAllWorldDialogues();
	}

	const bool result = IsValid(dialogueToStart->StartDialogue(UGameplayStatics::GetPlayerController(this, 0), participants));
	if (!result) {
		UE_LOG(LogTemp, Warning, TEXT("StartDialogue failed in TryStartDialogue!"));
	} 
	return result;
}

void UADSDialoguePartecipantComponent::RegisterDialogueEvents(UADSDialogue* dialogueToStart)
{
	// Fixes a randomic bug in which the widget does not get removed
	if (!dialogueToStart->OnDialogueStarted.IsAlreadyBound(this, &UADSDialoguePartecipantComponent::HandleDialogueStarted)) {
		dialogueToStart->OnDialogueStarted.AddDynamic(this, &UADSDialoguePartecipantComponent::HandleDialogueStarted);
	}
	if (!dialogueToStart->OnDialogueEnded.IsAlreadyBound(this, &UADSDialoguePartecipantComponent::HandleDialogueEnded)) {
		dialogueToStart->OnDialogueEnded.AddDynamic(this, &UADSDialoguePartecipantComponent::HandleDialogueEnded);
	}
	if (!dialogueToStart->OnDialogueNodeActivated.IsAlreadyBound(this, &UADSDialoguePartecipantComponent::HandleNodeActivated)) {
		dialogueToStart->OnDialogueNodeActivated.AddDynamic(this, &UADSDialoguePartecipantComponent::HandleNodeActivated);
	}
}

bool UADSDialoguePartecipantComponent::TryStartDialogueFromActors(const TArray<AActor*>& participants, UADSDialogue* dialogueToStart)
{
	TArray<UADSBaseDialoguePartecipantComponent*> participantComps;
	for (const auto actor : participants) {
		UADSBaseDialoguePartecipantComponent* dialogueComp = actor->FindComponentByClass<UADSBaseDialoguePartecipantComponent>();
		if (dialogueComp) {
			participantComps.Add(dialogueComp);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("No ADS Dialogue Component in the participant actor %s!"), *actor->GetName());
			return false;
		}
	}
	return TryStartDialogue(participantComps, dialogueToStart);
}

UADSDialogue* UADSDialoguePartecipantComponent::GetDialogue(FGameplayTag dialogueTag, bool& bFound) const
{
	if (!instancedDialogues.IsValidIndex(0)) {
		UE_LOG(LogTemp, Warning, TEXT("No available dialogues for this participant!- UADSDialoguePartecipantComponent::GetDialogue"));
		bFound = false;
		return nullptr;
	}

	for (const auto dial : instancedDialogues) {
		if (dial && dial->GetDialogueTag() == dialogueTag) {
			bFound = true;
			return dial;
		}
	}
	bFound = false;
	return nullptr;
}

USkeletalMeshComponent* UADSDialoguePartecipantComponent::GetOwnerMesh()
{
	if (skeletalMesh) {
		return skeletalMesh;
	}

	const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner());
	if (CharacterOwner) {
		skeletalMesh = CharacterOwner->GetMesh();
	}

	skeletalMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	return skeletalMesh;
}

USkeletalMeshComponent* UADSDialoguePartecipantComponent::GetFacialAnimationMesh()
{
	if (facialMesh) {
		return facialMesh;
	}

	// Itera su tutti i componenti dell'attore
	TArray<UActorComponent*> Components;
	GetOwner()->GetComponents(USkeletalMeshComponent::StaticClass(), Components);
	for (UActorComponent* Component : Components) {
		USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(Component);
		if (SkeletalMeshComp && SkeletalMeshComp->ComponentHasTag(FacialSkeletonComponentTag)) {
			facialMesh = SkeletalMeshComp;
			return facialMesh = SkeletalMeshComp;
		}
	}

	return nullptr;
}

void UADSDialoguePartecipantComponent::PlayAnimationOnCharacterOwner(UAnimMontage* animation)
{
	if (GetOwnerMesh()) {

		UAnimInstance* animInstance = skeletalMesh->GetAnimInstance();

		if (animInstance) {
			animInstance->Montage_Play(animation);
		}
	}
}

void UADSDialoguePartecipantComponent::PlayFacialAnimationOnCharacterOwner(UAnimMontage* animation)
{
	if (GetFacialAnimationMesh()) {

		UAnimInstance* animInstance = facialMesh->GetAnimInstance();

		if (animInstance) {
			animInstance->Montage_Play(animation);
		}
	}
}

void UADSDialoguePartecipantComponent::StopFacialAnimationOnCharacterOwner(UAnimMontage* animation)
{
	if (GetFacialAnimationMesh()) {

		UAnimInstance* animInstance = facialMesh->GetAnimInstance();

		if (animInstance) {
			animInstance->Montage_Stop(AnimationBlendoutTime, animation);
		}
	}
}

void UADSDialoguePartecipantComponent::StopAnimationOnCharacterOwner(UAnimMontage* animation)
{
	if (GetOwnerMesh()) {

		UAnimInstance* animInstance = skeletalMesh->GetAnimInstance();

		if (animInstance) {
			animInstance->Montage_Stop(AnimationBlendoutTime, animation);
		}
	}
}

void UADSDialoguePartecipantComponent::AddObjectiveDialogue(FGuid objectiveID, UADSDialogue* dialogue)
{
	if (dialogue) {
		dialogue->DeactivateAllNodes();
		ObjectiveDialogues.Add(objectiveID, dialogue);
	}
}

void UADSDialoguePartecipantComponent::RemoveObjectiveDialogue(FGuid objectiveID)
{
	if (ObjectiveDialogues.Contains(objectiveID)) {
		ObjectiveDialogues.FindAndRemoveChecked(objectiveID);
	}
}

void UADSDialoguePartecipantComponent::OnDialogueStartedEvent_Implementation()
{
}

void UADSDialoguePartecipantComponent::OnDialogueEndedEvent_Implementation()
{
	// Your custom logic here
}

void UADSDialoguePartecipantComponent::HandleDialogueStarted(UADSDialogue* dialogue)
{
	OnDialogueStartedEvent();
	OnDialogueStarted.Broadcast(dialogue);
}

void UADSDialoguePartecipantComponent::HandleDialogueEnded(UADSDialogue* dialogue)
{
	APlayerController* controller = UGameplayStatics::GetPlayerController(this, 0);
	UADSDialogueMasterComponent* dialogueMaster = UADSDialogueFunctionLibrary::GetLocalDialogueMaster(this);
	if (dialogueMaster) {
		dialogueMaster->StopCurrentShot();
	}

	UnregisterDialogueEvents(dialogue);

	OnDialogueEndedEvent();

	OnDialogueEnded.Broadcast(dialogue);
}

void UADSDialoguePartecipantComponent::UnregisterDialogueEvents(UADSDialogue* dialogue)
{
	if (dialogue) {
		dialogue->OnDialogueStarted.RemoveDynamic(this, &UADSDialoguePartecipantComponent::HandleDialogueStarted);
		dialogue->OnDialogueEnded.RemoveDynamic(this, &UADSDialoguePartecipantComponent::HandleDialogueEnded);
		dialogue->OnDialogueNodeActivated.RemoveDynamic(this, &UADSDialoguePartecipantComponent::HandleNodeActivated);
	}
}

void UADSDialoguePartecipantComponent::HandleNodeActivated(const FGuid& nodeId)
{
	OnNodeActivated.Broadcast(nodeId);
}
