// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ACFFirstPersonItemTypes.generated.h"

class USceneComponent;

/**
 * A point on a held item, such as a sight or a muzzle, resolved every frame against the item's components.
 *
 * Measured relative to the first person component's ItemReferenceBone, so it keeps working wherever the item
 * system attaches the item.
 */
USTRUCT(BlueprintType)
struct ASCENTFIRSTPERSON_API FACFFirstPersonItemPoint
{
	GENERATED_BODY()

	/** Socket on the component below. None uses the component's own transform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Point")
	FName SocketName;

	/**
	 * Tag of the component holding the socket, searched on the item and on actors attached to it, such as a sight
	 * attachment. None uses the item's main mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Point")
	FName ComponentTag;

	/** Extra offset in socket space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Point")
	FTransform Offset = FTransform::Identity;

	/** Runtime only, and wins over ComponentTag: for points on a component the item cannot find by tag. */
	UPROPERTY(Transient, BlueprintReadWrite, Category="Item Point")
	TObjectPtr<USceneComponent> Component;

	/** An unset point is ignored rather than resolved to the item mesh origin. */
	bool IsSet() const { return !SocketName.IsNone() || !ComponentTag.IsNone() || Component != nullptr; }
};
