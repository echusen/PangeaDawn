// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ALSLoadAndSaveComponent.h"
#include "ALSLoadAndSaveSubsystem.h"
#include "GameFramework/Actor.h"
#include <Kismet/GameplayStatics.h>

// Sets default values for this component's properties
UALSLoadAndSaveComponent::UALSLoadAndSaveComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UALSLoadAndSaveComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!bAutoReload) {
        return;
    }

    if (UALSLoadAndSaveSubsystem* SaveSubsystem = GetSaveSubsystem()) {
        SaveSubsystem->OnLoadFinished.AddUniqueDynamic(this, &UALSLoadAndSaveComponent::HandleSaveSystemLoadFinished);

        // Actors spawned after a load has completed will not receive the past event.
        if (SaveSubsystem->GetCurrentSaveGame() && SaveSubsystem->GetSystemState() == ELoadingState::EIdle) {
            TryAutoLoadActor();
        }
    }
}

void UALSLoadAndSaveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UALSLoadAndSaveSubsystem* SaveSubsystem = GetSaveSubsystem()) {
        SaveSubsystem->OnLoadFinished.RemoveDynamic(this, &UALSLoadAndSaveComponent::HandleSaveSystemLoadFinished);
    }

    Super::EndPlay(EndPlayReason);
}

void UALSLoadAndSaveComponent::SaveActor()
{
    UALSLoadAndSaveSubsystem* SaveSubsystem = GetSaveSubsystem();
    if (SaveSubsystem && SaveSubsystem->SaveActor(GetOwner())) {
          OnActorSaved.Broadcast();
    }
}

void UALSLoadAndSaveComponent::LoadActor()
{

    UALSLoadAndSaveSubsystem* SaveSubsystem = GetSaveSubsystem();
    if (SaveSubsystem && SaveSubsystem->LoadActor(GetOwner())) {
        DispatchLoaded();

    }
}

void UALSLoadAndSaveComponent::HandleSaveSystemLoadFinished(bool bSuccess)
{
    if (bSuccess) {
        TryAutoLoadActor();
    }
}

 void UALSLoadAndSaveComponent::TryAutoLoadActor()
{
    if (bAutoReload ) {
        LoadActor();
    }
}

void UALSLoadAndSaveComponent::DispatchLoaded()
{
    OnActorLoaded.Broadcast();
}

void UALSLoadAndSaveComponent::AddComponentClassToSave(TSubclassOf<UActorComponent> ComponentClass)
{
    if (ComponentClass) {
        ComponentsToSave.AddUnique(ComponentClass);
    }
}

TArray<UActorComponent*> UALSLoadAndSaveComponent::ResolveComponentsToSave() const
{
    TArray<UActorComponent*> Result;
    const AActor* Owner = GetOwner();
    if (!Owner) {
        return Result;
    }

    for (const TSubclassOf<UActorComponent>& CompClass : ComponentsToSave) {
        if (!CompClass) {
            continue;
        }

        TArray<UActorComponent*> Found;
        Owner->GetComponents(CompClass, Found);
        for (UActorComponent* Comp : Found) {
            if (IsValid(Comp)) {
                Result.AddUnique(Comp);
            }
        }
    }
    return Result;
}

UALSLoadAndSaveSubsystem* UALSLoadAndSaveComponent::GetSaveSubsystem() const
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
    return GameInstance ? GameInstance->GetSubsystem<UALSLoadAndSaveSubsystem>() : nullptr;
}
