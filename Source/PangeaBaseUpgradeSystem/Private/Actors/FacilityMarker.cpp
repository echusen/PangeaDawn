// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/FacilityMarker.h"

AFacilityMarker::AFacilityMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Optional: add arrow or billboard here
}

void AFacilityMarker::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AddTag(FacilityTag);
}

