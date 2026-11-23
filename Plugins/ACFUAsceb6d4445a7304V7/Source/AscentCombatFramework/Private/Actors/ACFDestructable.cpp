// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actors/ACFDestructable.h"
#include "Components/ACFDestructableComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

AACFDestructable::AACFDestructable()
{
	GeometryCollectionComp = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	SetRootComponent(GeometryCollectionComp);

	PrimaryActorTick.bCanEverTick = false;
	GeometryCollectionComp->SetUsingAbsoluteLocation(false);
	GeometryCollectionComp->SetUsingAbsoluteRotation(false);
	GeometryCollectionComp->SetUsingAbsoluteScale(false);
	GeometryCollectionComp->SetCollisionProfileName(TEXT("Destructible"));

	DestructableComp = CreateDefaultSubobject<UACFDestructableComponent>(TEXT("Destructable Component"));
}

void AACFDestructable::BeginPlay()
{
	Super::BeginPlay();

	if (DestructableComp && !DestructableComp->OnDestroyed.IsAlreadyBound(this, &AACFDestructable::HandleDestructionTriggered)) {
		DestructableComp->OnDestroyed.AddDynamic(this, &AACFDestructable::HandleDestructionTriggered);
	}
}

void AACFDestructable::HandleDestructionTriggered()
{
	OnActorDestroyed();
}
