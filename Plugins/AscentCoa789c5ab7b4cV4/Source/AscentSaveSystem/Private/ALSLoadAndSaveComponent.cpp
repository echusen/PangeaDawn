// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ALSLoadAndSaveComponent.h"
#include "ALSLoadAndSaveSubsystem.h"
#include <Kismet/GameplayStatics.h>

// Sets default values for this component's properties
UALSLoadAndSaveComponent::UALSLoadAndSaveComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
    bAlreadyLoaded = false;
}

// Called when the game starts
void UALSLoadAndSaveComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoReload && !bAlreadyLoaded) {
        LoadActor();
    }
}

void UALSLoadAndSaveComponent::SaveActor()
{
    if (GetSaveSubsystem()->SaveActor(GetOwner())) {
        OnActorSaved.Broadcast();
    }
}

void UALSLoadAndSaveComponent::LoadActor()
{

    if (GetSaveSubsystem()->LoadActor(GetOwner())) {
        DispatchLoaded();

    }
}

void UALSLoadAndSaveComponent::DispatchLoaded()
{
    bAlreadyLoaded = true;
    OnActorLoaded.Broadcast();
}

UALSLoadAndSaveSubsystem* UALSLoadAndSaveComponent::GetSaveSubsystem() const
{
    return UGameplayStatics::GetGameInstance(this)->GetSubsystem<UALSLoadAndSaveSubsystem>();
}
