// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Data/ACFCharacterCapsuleChannelFragment.h"
#include "Actors/ACFCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"

void UACFCharacterCapsuleChannelFragment::ApplyFragment_Implementation(APawn* PawnOwner)
{
	if (!PawnOwner)
	{
		return;
	}

	UCapsuleComponent* Capsule = PawnOwner->GetComponentByClass<UCapsuleComponent>();
	if (!Capsule)
	{
		return;
	}

	// Change the object type (collision channel) on the capsule.
	// SetObjectType updates the physics body's channel without touching responses.
	Capsule->SetCollisionObjectType(CapsuleCollisionChannel);

	// Optionally apply a full collision profile (channels + responses) by name.
	if (CollisionProfileName != NAME_None)
	{
		Capsule->SetCollisionProfileName(CollisionProfileName);

		// A profile replaces every response, camera included, so let the character re-state whether
		// it wants to be transparent to camera probes.
		if (AACFCharacter* acfCharacter = Cast<AACFCharacter>(PawnOwner))
		{
			acfCharacter->ApplyCameraCollisionSetup();
		}
	}
}
