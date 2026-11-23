// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFDialogueSplineFollowComponent.h"

#include "ADSDialoguePartecipantComponent.h"
#include "Components/ACFSplineFollowerComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UACFDialogueSplineFollowComponent::UACFDialogueSplineFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UACFDialogueSplineFollowComponent::BeginPlay()
{
	Super::BeginPlay();

	BindControllerChanged();
	TryAutoBind();
}

void UACFDialogueSplineFollowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFollowSplineDialogue();
	UnbindControllerChanged();

	Super::EndPlay(EndPlayReason);
}

bool UACFDialogueSplineFollowComponent::BindFollowSplineDialogue()
{
	UACFSplineFollowerComponent* SplineFollower = FindSplineFollower();
	UADSDialoguePartecipantComponent* DialogueParticipant = FindDialogueParticipant();
	if (!SplineFollower || !DialogueParticipant)
	{
		return false;
	}

	if (BoundSplineFollower == SplineFollower && BoundDialogueParticipant == DialogueParticipant
		&& SplineFollower->OnStartedWaitingForTarget.IsAlreadyBound(this, &UACFDialogueSplineFollowComponent::HandleStartedWaitingForTarget))
	{
		return true;
	}

	UnbindFollowSplineDialogue();

	SplineFollower->OnStartedWaitingForTarget.AddDynamic(this, &UACFDialogueSplineFollowComponent::HandleStartedWaitingForTarget);
	SplineFollower->OnTargetRejoined.AddDynamic(this, &UACFDialogueSplineFollowComponent::HandleTargetRejoined);
	SplineFollower->OnFollowingStopped.AddDynamic(this, &UACFDialogueSplineFollowComponent::HandleFollowingStopped);

	BoundSplineFollower = SplineFollower;
	BoundDialogueParticipant = DialogueParticipant;
	return true;
}

void UACFDialogueSplineFollowComponent::UnbindFollowSplineDialogue()
{
	if (BoundSplineFollower)
	{
		BoundSplineFollower->OnStartedWaitingForTarget.RemoveDynamic(this, &UACFDialogueSplineFollowComponent::HandleStartedWaitingForTarget);
		BoundSplineFollower->OnTargetRejoined.RemoveDynamic(this, &UACFDialogueSplineFollowComponent::HandleTargetRejoined);
		BoundSplineFollower->OnFollowingStopped.RemoveDynamic(this, &UACFDialogueSplineFollowComponent::HandleFollowingStopped);
	}

	BoundSplineFollower = nullptr;
	BoundDialogueParticipant = nullptr;
}

bool UACFDialogueSplineFollowComponent::IsFollowSplineDialogueBound() const
{
	return BoundSplineFollower != nullptr && BoundDialogueParticipant != nullptr;
}

void UACFDialogueSplineFollowComponent::HandleStartedWaitingForTarget()
{
	if (BoundDialogueParticipant)
	{
		BoundDialogueParticipant->PauseActiveDialogues();
	}
}

void UACFDialogueSplineFollowComponent::HandleTargetRejoined()
{
	if (BoundDialogueParticipant)
	{
		BoundDialogueParticipant->ResumeActiveDialogues();
	}
}

void UACFDialogueSplineFollowComponent::HandleFollowingStopped()
{
	if (bResumeDialogueWhenFollowEnds && BoundDialogueParticipant)
	{
		BoundDialogueParticipant->ResumeActiveDialogues();
	}
}

void UACFDialogueSplineFollowComponent::HandleControllerChanged(APawn* /*Pawn*/, AController* /*OldController*/, AController* NewController)
{
	if (!bAutoBind)
	{
		return;
	}

	UnbindFollowSplineDialogue();
	if (NewController)
	{
		TryAutoBind();
	}
}

void UACFDialogueSplineFollowComponent::TryAutoBind()
{
	AActor* Owner = GetOwner();
	if (!bAutoBind || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	BindFollowSplineDialogue();
}

UACFSplineFollowerComponent* UACFDialogueSplineFollowComponent::FindSplineFollower() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (UACFSplineFollowerComponent* Found = Owner->FindComponentByClass<UACFSplineFollowerComponent>())
	{
		return Found;
	}

	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (AController* PawnController = Pawn->GetController())
		{
			return PawnController->FindComponentByClass<UACFSplineFollowerComponent>();
		}
	}
	else if (const AController* OwnerController = Cast<AController>(Owner))
	{
		if (APawn* PossessedPawn = OwnerController->GetPawn())
		{
			return PossessedPawn->FindComponentByClass<UACFSplineFollowerComponent>();
		}
	}

	return nullptr;
}

UADSDialoguePartecipantComponent* UACFDialogueSplineFollowComponent::FindDialogueParticipant() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (UADSDialoguePartecipantComponent* Found = Owner->FindComponentByClass<UADSDialoguePartecipantComponent>())
	{
		return Found;
	}

	if (const AController* OwnerController = Cast<AController>(Owner))
	{
		if (APawn* PossessedPawn = OwnerController->GetPawn())
		{
			return PossessedPawn->FindComponentByClass<UADSDialoguePartecipantComponent>();
		}
	}
	else if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (AController* PawnController = Pawn->GetController())
		{
			return PawnController->FindComponentByClass<UADSDialoguePartecipantComponent>();
		}
	}

	return nullptr;
}

void UACFDialogueSplineFollowComponent::BindControllerChanged()
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (!Pawn->ReceiveControllerChangedDelegate.IsAlreadyBound(this, &UACFDialogueSplineFollowComponent::HandleControllerChanged))
		{
			Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UACFDialogueSplineFollowComponent::HandleControllerChanged);
		}
	}
}

void UACFDialogueSplineFollowComponent::UnbindControllerChanged()
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UACFDialogueSplineFollowComponent::HandleControllerChanged);
	}
}
