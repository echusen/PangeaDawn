// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Data/ACFCharacterFragment.h"
#include "ACFCharacterCapsuleChannelFragment.generated.h"

/**
 * Fragment for UACFCharacterDataAsset that overrides the capsule component's
 * collision channel (object type). Add this as an instanced fragment in the
 * DataAsset's Fragments array to set a specific collision channel (e.g.
 * Pawn, Vehicle, Custom…) on the character's capsule at init time.
 *
 * The existing CapsuleHalfHeight / CapsuleRadius from the DataAsset are
 * applied first; this fragment then changes the channel independently.
 */
UCLASS(Blueprintable, DisplayName = "Capsule Collision Channel Fragment")
class ASCENTCOMBATFRAMEWORK_API UACFCharacterCapsuleChannelFragment : public UACFCharacterFragment
{
	GENERATED_BODY()

public:
	UACFCharacterCapsuleChannelFragment() {}

	/** Capsule collision channel (object type) to apply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TEnumAsByte<ECollisionChannel> CapsuleCollisionChannel = ECC_Pawn;

	/** Optional collision profile name to apply after setting the channel.
	 *  When non-empty, calls SetCollisionProfileName(…) on the capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	FName CollisionProfileName = NAME_None;

	virtual void ApplyFragment_Implementation(APawn* PawnOwner) override;
};